/*
 * DemonStar GLB extractor
 * https://github.com/vs49688/extract-glb
 *
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2019 Zane van Iperen
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
//
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>

#pragma pack(push, 1)
typedef struct glb_header
{
	char		magic[8];
	uint64_t	file_count; /* Probably 2 uint32's, but whatever. */
} glb_header_t;

typedef struct glb_dentry
{
	uint32_t	offset;
	uint32_t	size;
	char		filename[20];
} glb_dentry_t;
#pragma pack(pop)

/* If this fails, errno will always be set. */
static int glb_write_file(FILE *f, glb_dentry_t *entry)
{
	if(fseek(f, entry->offset, SEEK_SET) < 0)
		return -1;

	void *buf = malloc(entry->size);
	if(!buf)
		return -1;

	FILE *of = fopen(entry->filename, "wb");
	if(!of)
		goto fopen_fail;

	if(fread(buf, entry->size, 1, f) != 1)
		goto io_fail;

	if(fwrite(buf, entry->size, 1, of) != 1)
		goto io_fail;

	free(buf);
	fclose(of);
	return 0;
io_fail:
	fclose(of);
fopen_fail:
	free(buf);
	return -1;
}

int main(int argc, char **argv)
{
	if(argc != 2)
	{
		printf("Usage: %s <infile.glb>\n", argv[0]);
		return 2;
	}

	FILE *f = fopen(argv[1], "rb");
	if(!f)
	{
		fprintf(stderr, "Unable to open \"%s\": %s\n", argv[1], strerror(errno));
		return 1;
	}

	glb_header_t h;
	glb_dentry_t *dir = NULL;

	/* Read the header. */
	if(fread(&h, sizeof(h), 1, f) != 1)
		goto fop_failed;

	if(strncmp("GLB2.0", h.magic, sizeof(h.magic)) != 0)
	{
		fprintf(stderr, "Unknown file magic, expected \"GLB2.0\"...\n");
		goto failed;
	}

	if(!(dir = malloc(sizeof(glb_dentry_t) * h.file_count)))
	{
		perror("malloc");
		goto failed;
	}

	if(fread(dir, sizeof(glb_dentry_t) * h.file_count, 1, f) != 1)
		goto fop_failed;

	for(size_t i = 0; i < h.file_count; ++i)
	{
		/* Skip the 'STARTXXX:' and 'ENDXXX:' markers */
		size_t nlen = strnlen(dir[i].filename, sizeof(dir[i].filename));
		if(dir[i].filename[nlen - 1] == ':' || dir[i].size == 0)
			continue;

		if(glb_write_file(f, &dir[i]) < 0)
		{
			fprintf(stderr, "Error extracting %20s: %s\n", dir[i].filename, strerror(errno));
			goto failed;
		}
	}

	free(dir);
	fclose(f);
	return 0;

fop_failed:
	if(ferror(f))
		fprintf(stderr, "%s\n", strerror(errno));
	else if(feof(f))
		fprintf(stderr, "Hit premature EOF\n");

failed:
	if(dir)
		free(dir);

	fclose(f);
	return 1;
}
