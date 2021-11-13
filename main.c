/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 Tomohiro Kusumi <tkusumi@netbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>

#include "./gpt.h"
#include "./subr.h"

static void
usage(const char *progname)
{
	fprintf(stderr, "usage: %s: <gpt_image_path>\n", progname);
}

int
main(int argc, char **argv)
{
	if (argc < 2) {
		usage(argv[0]);
		exit(1);
	}

	if (!is_le()) {
		fprintf(stderr, "big-endian arch unsupported\n");
		exit(1);
	}

	int i, c;
	struct option lo[] = {
		{"verbose", 0, 0, 'V'},
		{0, 0, 0, 0},
	};

	while ((c = getopt_long(argc, argv, "hu", lo, &i)) != -1) {
		switch (c) {
		case 'V':
			dump_opt_verbose = 1;
			break;
		case 'h':
		case 'u':
		default:
			usage(argv[0]);
			exit(1);
			break;
		}
	}
	argc -= optind;
	argv += optind;

	const char *device = argv[0];
	printf("%s\n", device);
	printf("\n");

	int fd = open(device, O_RDONLY);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	int ret = dump_gpt(fd);
	if (ret) {
		fprintf(stderr, "failed %d\n", ret);
		exit(1);
	}

	close(fd);

	return 0;
}
