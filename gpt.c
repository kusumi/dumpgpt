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
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include "./gpt.h"
#include "./subr.h"
#include "./freebsd/sys/disk/gpt.h"

#define UNIT_SIZE	512

bool dump_opt_verbose;
bool dump_opt_symbol;
bool dump_opt_noalt;

static int
try_known_uuid_to_str(const uuid_t *uuid, char **s)
{
	int ret;

	if (dump_opt_symbol) {
		ret = known_uuid_to_str(uuid, s);
		if (ret)
			ret = uuid_to_str(uuid, s);
	} else {
		ret = uuid_to_str(uuid, s);
	}

	return ret;
}

static int
dump_header(int fd, off_t hdr_lba, struct gpt_hdr *ret_hdr)
{
	char buf[UNIT_SIZE] = {0};
	assert(sizeof(buf) % 512 == 0);

	const struct gpt_hdr *hdr = (void*)buf;
	off_t hdr_offset = hdr_lba * sizeof(buf);

	int ret = pread(fd, buf, sizeof(buf), hdr_offset);
	if (ret == -1) {
		perror("pread");
		return errno;
	} else if (ret != sizeof(buf)) {
		fprintf(stderr, "failed to read\n");
		return EINVAL;
	}

	char sig[9] = {0};
	memcpy(sig, hdr->hdr_sig, sizeof(hdr->hdr_sig));
	if (strncmp(sig, "EFI PART", sizeof(sig))) {
		fprintf(stderr, "not GPT\n");
		return EINVAL;
	}

	printf("sig      = \"%c%c%c%c%c%c%c%c\"\n",
		hdr->hdr_sig[0],
		hdr->hdr_sig[1],
		hdr->hdr_sig[2],
		hdr->hdr_sig[3],
		hdr->hdr_sig[4],
		hdr->hdr_sig[5],
		hdr->hdr_sig[6],
		hdr->hdr_sig[7]);

	const unsigned char *p = (void*)&hdr->hdr_revision;
	printf("revision = %02x %02x %02x %02x\n",
		p[0], p[1], p[2], p[3]);

	printf("size     = %u\n", hdr->hdr_size);
	printf("crc_self = 0x%x\n", hdr->hdr_crc_self);
	printf("lba_self = 0x%016lx\n", hdr->hdr_lba_self);
	printf("lba_alt  = 0x%016lx\n", hdr->hdr_lba_alt);
	printf("lba_start= 0x%016lx\n", hdr->hdr_lba_start);
	printf("lba_end  = 0x%016lx\n", hdr->hdr_lba_end);

	char *s = NULL;
	ret = try_known_uuid_to_str((void*)&hdr->hdr_uuid, &s);
	if (ret)
		return ret;
	printf("uuid     = %s\n", s);
	free(s);

	printf("lba_table= 0x%016lx\n", hdr->hdr_lba_table);
	printf("entries  = %d\n", hdr->hdr_entries);
	printf("entsz    = %d\n", hdr->hdr_entsz);
	printf("crc_table= 0x%x\n", hdr->hdr_crc_table);

	if (ret_hdr)
		*ret_hdr = *hdr;

	/* XXX */
	if (hdr->hdr_entries > 512) {
		fprintf(stderr, "likely corrupted entries %d ???\n",
			hdr->hdr_entries);
		return EINVAL;
	}

	return 0;
}

static int
dump_entries(int fd, const struct gpt_hdr *hdr)
{
	char buf[UNIT_SIZE] = {0};
	assert(sizeof(buf) % 512 == 0);

	uint64_t lba_table_size = hdr->hdr_entsz * hdr->hdr_entries;
	int lba_table_sectors = lba_table_size / sizeof(buf);
	int total = 0;

	printf("%-3s %-36s %-36s %-16s %-16s %-16s %s\n",
		"#", "type", "uniq", "lba_start", "lba_end", "attr", "name");

	for (int i = 0; i < lba_table_sectors; i++) {
		off_t offset = (hdr->hdr_lba_table + i) * sizeof(buf);
		int ret = pread(fd, buf, sizeof(buf), offset);
		if (ret == -1) {
			perror("pread");
			return errno;
		} else if (ret != sizeof(buf)) {
			fprintf(stderr, "failed to read\n");
			return EINVAL;
		}

		int sector_entries = sizeof(buf) / hdr->hdr_entsz;
		const struct gpt_ent *p = (void*)buf;

		for (int j = 0; j < sector_entries; j++) {
			const struct gpt_ent empty = {0};
			if (!dump_opt_verbose &&
			    !memcmp(p, &empty, sizeof(empty)))
				goto next;

			char *s1 = NULL;
			ret = try_known_uuid_to_str((void*)&p->ent_type, &s1);
			if (ret)
				return ret;

			char *s2 = NULL;
			ret = try_known_uuid_to_str((void*)&p->ent_uuid, &s2);
			if (ret)
				return ret;

			char name[37] = {0};
			for (int k = 0; k < 36; k++)
				name[k] = p->ent_name[k] & 0xFF; /* XXX ascii */

			printf("%-3d %-36s %-36s %016lx %016lx %016lx %s\n",
				i * sector_entries + j,
				s1,
				s2,
				p->ent_lba_start,
				p->ent_lba_end,
				p->ent_attr,
				name);
			free(s1);
			free(s2);
next:
			total++;
			p++;
		}
	}
	assert(total == hdr->hdr_entries);

	return 0;
}

int
dump_gpt(int fd)
{
	struct gpt_hdr hdr1 = {0};
	struct gpt_hdr hdr2 = {0};
	int ret;

	/* primary header */
	printf("primary header\n");
	ret = dump_header(fd, 1, &hdr1);
	if (ret)
		return ret;

	/* secondary header */
	if (!dump_opt_noalt) {
		printf("\n");
		printf("secondary header\n");
		ret = dump_header(fd, hdr1.hdr_lba_alt, &hdr2);
		if (ret)
			return ret;
	}

	/* primary entries */
	printf("\n");
	printf("primary entries\n");
	ret = dump_entries(fd, &hdr1);
	if (ret)
		return ret;

	/* secondary entries */
	if (!dump_opt_noalt) {
		printf("\n");
		printf("secondary entries\n");
		ret = dump_entries(fd, &hdr2);
		if (ret)
			return ret;
	}

	return 0;
}
