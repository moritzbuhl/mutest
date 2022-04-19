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

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int msgs = 1024;
int msgsiz = 256;

void
usage(void)
{
	fprintf(stderr, "send [-m] [-n messages] [-s size] file\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct sockaddr_in saddr;
	ssize_t size;
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

	if ((fd = open(argv[0], O_RDONLY)) == -1)
		err(1, "%s", argv[0]);

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		err(1, "socket");

	if (mflag) {
		struct mmsghdr	*mmsg;
		struct iovec	*iov;

		if ((mmsg = calloc(msgs, sizeof(struct mmsghdr))) == NULL)
			err(1, NULL);

		if ((iov = calloc(msgs, sizeof(struct iovec))) == NULL)
			err(1, NULL);

		for (int i = 0; i < msgs; i++) {
			mmsg[i].msg_hdr.msg_name = &saddr;
			mmsg[i].msg_hdr.msg_namelen = sizeof saddr;
			mmsg[i].msg_hdr.msg_iov = &iov[i];
			mmsg[i].msg_hdr.msg_iovlen = 1;

			iov[i].iov_base = malloc(msgsiz);
			iov[i].iov_len = msgsiz;
		}

		while ((size = readv(fd, iov, msgs)) > 0) {
			int vlen = size / msgsiz;
			int cnt = 0;

			if (vlen * msgsiz < size)
				vlen++;

			do {
				vlen -= cnt;
				int ret = sendmmsg(s, mmsg + cnt, vlen, 0);
				if (ret == -1)
					err(1, "sendmmsg");
				cnt += ret;
			} while (vlen > 0);
		}

		for (int i = 0; i < msgs; i++)
			free(iov[i].iov_base);
		free(iov);
		free(mmsg);

	} else {
		struct msghdr msg;
		char buf[msgsiz];

		memset(&msg, 0, sizeof msg);
		msg.msg_name = &saddr;
		msg.msg_namelen = sizeof saddr;
		msg.msg_iov = &(struct iovec) {
			.iov_base = buf,
		};
		msg.msg_iovlen = 1;

		while ((size = read(fd, buf, sizeof buf)) > 0) {
			msg.msg_iov->iov_len = size;
			if (sendmsg(s, &msg, 0) == -1)
				err(1, "sendmsg");
		}

		if (size == -1)
			err(1, "read");
	}

	close(s);
	close(fd);

	return 0;
}
