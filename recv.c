/*
 * Copyright (c) 2021 Jan Klemkow <jan@openbsd.org>
 * Copyright (c) 2021, 2022 Moritz Buhl <mbuhl@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int run = 1;
int msgs = 1024;
int msgsiz = 256;

void
status(int sig)
{
	if (sig == SIGINT)
		run = 0;
}

void
usage(void)
{
	fprintf(stderr, "recv [-m] [-n messages] [-s size] file\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct timeval start, end;
	struct sockaddr_in saddr;
	ssize_t size;
	size_t bytes = 0;
	const char *errstr;
	int ch, fd, s, mflag = 0;

	while ((ch = getopt(argc, argv, "mn:s:")) != -1) {
		switch (ch) {
		case 'm':
			mflag = 1;
			break;
		case 'n':
			msgs = strtonum(optarg, 0, INT_MAX, &errstr);
                        if (errstr != NULL)
                                errx(1, "msgs is %s: %s", errstr, optarg);
			break;
		case 's':
			msgsiz = strtonum(optarg, 0, INT_MAX, &errstr);
                        if (errstr != NULL)
                                errx(1, "msgsiz is %s: %s", errstr, optarg);
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	memset(&saddr, 0, sizeof saddr);
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	saddr.sin_port = htons(1234);

	if ((fd = open(argv[0], O_WRONLY)) == -1)
		err(1, "%s", argv[0]);

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		err(1, "socket");

	if (bind(s, (struct sockaddr *)&saddr, sizeof saddr) == -1)
		err(1, "bind");

	if (gettimeofday(&start, NULL) == -1)
		err(1, "gettimeofday");

	if (signal(SIGINT, status) == SIG_ERR)
		err(1, "signal");

	if (mflag) {
		struct mmsghdr	*mmsg;
		struct iovec	*iov;
		int cnt;

		if ((mmsg = calloc(msgs, sizeof(struct mmsghdr))) == NULL)
			err(1, NULL);

		if ((iov = calloc(msgs, sizeof(struct iovec))) == NULL)
			err(1, NULL);

		for (int i = 0; i < msgs; i++) {
			mmsg[i].msg_hdr.msg_iov = &iov[i];
			mmsg[i].msg_hdr.msg_iovlen = 1;

			iov[i].iov_base = malloc(msgsiz);
			iov[i].iov_len = msgsiz;
		}
 again:
		while ((cnt = recvmmsg(s, mmsg, msgs, MSG_DONTWAIT,
		    NULL)) > 0) {
			if ((size = writev(fd, iov, cnt)) == -1)
				err(1, "writev");

			bytes += size;
		}

		if (cnt == -1 && run) {
			if (errno == EAGAIN)
				goto again;

			err(1, "recvmmsg");
		}

		for (int i = 0; i < msgs; i++)
			free(iov[i].iov_base);
		free(iov);
		free(mmsg);

	} else {
		struct msghdr msg;
		char buf[msgsiz];

		memset(&msg, 0, sizeof msg);
		msg.msg_iov = &(struct iovec) {
			.iov_base = buf,
			.iov_len = sizeof buf,
		};
		msg.msg_iovlen = 1;

		while (run && (size = recvmsg(s, &msg, 0)) > 0) {
			if ((size = write(fd, msg.msg_iov->iov_base, size)) == -1)
				err(1, "write");

			bytes += size;
		}

		if (size == -1)
			err(1, "recvmsg");
	}

	close(s);
	close(fd);

	if (gettimeofday(&end, NULL) == -1)
		err(1, "gettimeofday");

	printf("%zu bytes in %llu sec\n", bytes, end.tv_sec - start.tv_sec);
	printf("%llu bytes/s\n", bytes / MAX(end.tv_sec - start.tv_sec, 1));

	return 0;
}
