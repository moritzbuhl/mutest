/*
 * Copyright (c) 2022 Moritz Buhl <mbuhl@openbsd.org>
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

#include <sys/socket.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int chr;

void
gen_iovecs(int iovs, int iov_len, struct iovec *iov1, struct iovec *iov2)
{
	int i;

	for (i = 0; i < iovs; i++) {
		if ((iov1[i].iov_base = malloc(iov_len)) == NULL)
			err(1, NULL);

		if ((iov2[i].iov_base = malloc(iov_len)) == NULL)
			err(1, NULL);

		memset(iov1[i].iov_base, chr++, iov_len);
		memset(iov2[i].iov_base, 0, iov_len);
		
		iov1[i].iov_len = iov_len;
		iov2[i].iov_len = iov_len;
	}
}

void
gen_mmsgs(int msgs, int iovs, int iov_len, struct mmsghdr *mmsg1,
    struct mmsghdr *mmsg2)
{
	int i;

	for (i = 0; i < msgs; i++) {
		if ((mmsg1[i].msg_hdr.msg_iov = calloc(iovs, sizeof(struct iovec))) == NULL)
			err(1, NULL);

		if ((mmsg2[i].msg_hdr.msg_iov = calloc(iovs, sizeof(struct iovec))) == NULL)
			err(1, NULL);

		gen_iovecs(iovs, iov_len, mmsg1[i].msg_hdr.msg_iov,
		    mmsg2[i].msg_hdr.msg_iov);
		mmsg1[i].msg_hdr.msg_iovlen = iovs;
		mmsg2[i].msg_hdr.msg_iovlen = iovs;
	}
}

void
rec_free_mmsg(struct mmsghdr *mmsg, int msgs)
{
	int i;
	unsigned int j;

	for (i = 0; i < msgs; i++) {
		for (j = 0; j < mmsg[i].msg_hdr.msg_iovlen; j++)
			free(mmsg[i].msg_hdr.msg_iov[j].iov_base);
		free(mmsg[i].msg_hdr.msg_iov);
	}
	free(mmsg);
}

void
check(int len, struct mmsghdr *mmsg1, struct mmsghdr *mmsg2)
{
	int i;
	unsigned int j;

	for (i = 0; i < len; i++) {
		for (j = 0; j < mmsg1[i].msg_hdr.msg_iovlen; j++)
			if (memcmp(mmsg1[i].msg_hdr.msg_iov[j].iov_base,
			    mmsg2[i].msg_hdr.msg_iov[j].iov_base,
			    mmsg1[i].msg_hdr.msg_iov[j].iov_len) != 0) {
				printf("NOT EQUAL mmsg[%d].msg_hdr.msg_iov"
				    "[%d].iov_base:\n%s\n%s\n", i, j,
				    (char *)mmsg1[i].msg_hdr.msg_iov[j].iov_base,
				    (char *)mmsg2[i].msg_hdr.msg_iov[j].iov_base);
				return;
			}
	}
}

int
main(void)
{
	struct mmsghdr *mmsg1, *mmsg2;
	int s[2], msgs, iovs, iov_len, q, r;

	if (socketpair(AF_UNIX, SOCK_DGRAM, PF_UNSPEC, s) == -1)
		err(1, "socketpair");

	
	for (msgs = 1; msgs <= 1024; msgs *= 2) {
	for (iovs = 1; iovs <= 8192 / msgs; iovs *= 2) {
		iov_len = 8192 / msgs / iovs;
		chr = 1;
printf("sending %d msgs containing %d iovs of length %d, %d bytes\n", msgs, iovs, iov_len, msgs*iovs*iov_len);

		if ((mmsg1 = calloc(msgs, sizeof(struct mmsghdr))) == NULL)
			err(1, NULL);

		if ((mmsg2 = calloc(msgs, sizeof(struct mmsghdr))) == NULL)
			err(1, NULL);

		gen_mmsgs(msgs, iovs, iov_len, mmsg1, mmsg2);
		q = sendmmsg(s[0], mmsg1, msgs, 0);
		if (q == -1) {
			printf("sendmmsg error: %d\n", errno);
			continue;
		}
		if (q != msgs)
			printf("sendmmsg sent %d/%d msgs\n", q, msgs);

		r = recvmmsg(s[1], mmsg2, msgs, MSG_DONTWAIT, NULL);
		if (r == -1) {
			printf("recvmmsg error: %d\n", errno);
			continue;
		}
		if (r != q)
			printf("recvmmsg received %d/%d msgs\n", r, msgs);
		check(r, mmsg1, mmsg2);

		rec_free_mmsg(mmsg1, msgs);
		rec_free_mmsg(mmsg2, msgs);
	}
	}

	close(s[0]);
	close(s[1]);
}
