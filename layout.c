/*
 * This file is part of the flashrom project.
 *
 * Copyright (C) 2005-2008 coresystems GmbH
 * (Written by Stefan Reinauer <stepan@coresystems.de> for coresystems GmbH)
 * Copyright (C) 2011-2013 Stefan Tauner
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#ifndef __LIBPAYLOAD__
#include <fcntl.h>
#include <sys/stat.h>
#endif
#include "flash.h"
#include "programmer.h"

#define MAX_ROMLAYOUT	32

typedef struct {
	unsigned int start;
	unsigned int end;
	unsigned int included;
	char name[256];
	char *file;
} romentry_t;

/* rom_entries store the entries specified in a layout file and associated run-time data */
static romentry_t rom_entries[MAX_ROMLAYOUT];
static int num_rom_entries = 0; /* the number of valid rom_entries */

/* include_args holds the arguments specified at the command line with -i. They must be processed at some point
 * so that desired regions are marked as "included" in the rom_entries list. */
static char *include_args[MAX_ROMLAYOUT];
static int num_include_args = 0; /* the number of valid include_args. */

#ifndef __LIBPAYLOAD__
int read_romlayout(char *name)
{
	FILE *romlayout;
	char tempstr[256];
	int i;

	romlayout = fopen(name, "r");

	if (!romlayout) {
		msg_gerr("ERROR: Could not open ROM layout (%s).\n",
			name);
		return -1;
	}

	while (!feof(romlayout)) {
		char *tstr1, *tstr2;

		if (num_rom_entries >= MAX_ROMLAYOUT) {
			msg_gerr("Maximum number of ROM images (%i) in layout "
				 "file reached.\n", MAX_ROMLAYOUT);
			return 1;
		}
		if (2 != fscanf(romlayout, "%s %s\n", tempstr, rom_entries[num_rom_entries].name))
			continue;
#if 0
		// fscanf does not like arbitrary comments like that :( later
		if (tempstr[0] == '#') {
			continue;
		}
#endif
		tstr1 = strtok(tempstr, ":");
		tstr2 = strtok(NULL, ":");
		if (!tstr1 || !tstr2) {
			msg_gerr("Error parsing layout file. Offending string: \"%s\"\n", tempstr);
			fclose(romlayout);
			return 1;
		}
		rom_entries[num_rom_entries].start = strtol(tstr1, (char **)NULL, 16);
		rom_entries[num_rom_entries].end = strtol(tstr2, (char **)NULL, 16);
		rom_entries[num_rom_entries].included = 0;
		rom_entries[num_rom_entries].file = NULL;
		num_rom_entries++;
	}

	for (i = 0; i < num_rom_entries; i++) {
		msg_gdbg("romlayout %08x - %08x named %s\n",
			     rom_entries[i].start,
			     rom_entries[i].end, rom_entries[i].name);
	}

	fclose(romlayout);

	return 0;
}
#endif

/* returns the index of the entry (or a negative value if it is not found) */
int find_include_arg(const char *const name)
{
	unsigned int i;
	for (i = 0; i < num_include_args; i++) {
		if (!strcmp(include_args[i], name))
			return i;
	}
	return -1;
}

/* register an include argument (-i) for later processing */
int register_include_arg(char *name)
{
	if (num_include_args >= MAX_ROMLAYOUT) {
		msg_gerr("Too many regions included (%i).\n", num_include_args);
		return 1;
	}

	if (name == NULL) {
		msg_gerr("<NULL> is a bad region name.\n");
		return 1;
	}

	if (find_include_arg(name) != -1) {
		msg_gerr("Duplicate region name: \"%s\".\n", name);
		return 1;
	}

	include_args[num_include_args] = name;
	num_include_args++;
	return 0;
}

/* returns the index of the entry (or a negative value if it is not found) */
static int find_romentry(char *name)
{
	int i;
	msg_gspew("Looking for region \"%s\"... ", name);
	for (i = 0; i < num_rom_entries; i++) {
		if (strcmp(rom_entries[i].name, name) == 0) {
			msg_gspew("found.\n");
			return i;
		}
	}
	msg_gspew("not found.\n");
	return -1;
}

/* process -i arguments
 * returns 0 to indicate success, >0 to indicate failure
 */
int process_include_args(void)
{
	int i;
	unsigned int found = 0;

	if (num_include_args == 0)
		return 0;

	/* User has specified an area, but no layout file is loaded. */
	if (num_rom_entries == 0) {
		msg_gerr("Region requested (with -i/--image \"%s\"),\n"
			 "but no layout data is available. To include one use the -l/--layout syntax).\n",
			 include_args[0]);
		return 1;
	}

	for (i = 0; i < num_include_args; i++) {
		char *name = strtok(include_args[i], ":"); /* -i <image>[:<file>] */
		int idx = find_romentry(name);
		if (idx < 0) {
			msg_gerr("Invalid region specified: \"%s\".\n", include_args[i]);
			return 1;
		}
		rom_entries[idx].included = 1;
		found++;

		char *file = strtok(NULL, ""); /* remaining non-zero length token or NULL */
		if (file != NULL) {
			file = strdup(file);
			if (file == NULL) {
				msg_gerr("Out of memory!\n");
				return 1;
			}
			rom_entries[idx].file = file;
		}
	}

	msg_ginfo("Using region%s: \"%s\"", num_include_args > 1 ? "s" : "",
		  include_args[0]);
	for (i = 1; i < num_include_args; i++)
		msg_ginfo(", \"%s\"", include_args[i]);
	msg_ginfo(".\n");
	return 0;
}

void layout_cleanup(void)
{
	int i;
	for (i = 0; i < num_include_args; i++) {
		free(include_args[i]);
		include_args[i] = NULL;
	}
	num_include_args = 0;

	for (i = 0; i < num_rom_entries; i++) {
		free(rom_entries[i].file);
		rom_entries[i].file = NULL;
		rom_entries[i].included = 0;
	}
	num_rom_entries = 0;
}

romentry_t *get_next_included_romentry(unsigned int start)
{
	int i;
	unsigned int best_start = UINT_MAX;
	romentry_t *best_entry = NULL;
	romentry_t *cur;

	/* First come, first serve for overlapping regions. */
	for (i = 0; i < num_rom_entries; i++) {
		cur = &rom_entries[i];
		if (!cur->included)
			continue;
		/* Already past the current entry? */
		if (start > cur->end)
			continue;
		/* Inside the current entry? */
		if (start >= cur->start)
			return cur;
		/* Entry begins after start. */
		if (best_start > cur->start) {
			best_start = cur->start;
			best_entry = cur;
		}
	}
	return best_entry;
}

/* If a file name is specified for this region, read the file contents and
 * overwrite @newcontents in the range specified by @entry. */
static int read_content_from_file(romentry_t *entry, uint8_t *newcontents)
{
	char *file = entry->file;
	if (file == NULL)
		return 0;

	int len = entry->end - entry->start + 1;
	FILE *fp;
	if ((fp = fopen(file, "rb")) == NULL) {
		msg_gerr("Error: Opening layout image file \"%s\" failed: %s\n", file, strerror(errno));
		return 1;
	}

	struct stat file_stat;
	if (fstat(fileno(fp), &file_stat) != 0) {
		msg_gerr("Error: Getting metadata of layout image file \"%s\" failed: %s\n", file, strerror(errno));
		fclose(fp);
		return 1;
	}
	if (file_stat.st_size != len) {
		msg_gerr("Error: Image size (%jd B) doesn't match the flash chip's size (%d B)!\n",
			 (intmax_t)file_stat.st_size, len);
		fclose(fp);
		return 1;
	}

	int numbytes = fread(newcontents + entry->start, 1, len, fp);
	if (ferror(fp)) {
		msg_gerr("Error: Reading layout image file \"%s\" failed: %s\n", file, strerror(errno));
		fclose(fp);
		return 1;
	}
	if (fclose(fp)) {
		msg_gerr("Error: Closing layout image file \"%s\" failed: %s\n", file, strerror(errno));
		return 1;
	}
	if (numbytes != len) {
		msg_gerr("Error: Failed to read layout image file \"%s\" completely.\n"
			 "Got %d bytes, wanted %d!\n", file, numbytes, len);
		return 1;
	}
	return 0;
}

int handle_romentries(const struct flashctx *flash, uint8_t *oldcontents, uint8_t *newcontents)
{
	unsigned int start = 0;
	romentry_t *entry;
	unsigned int size = flash->chip->total_size * 1024;

	/* If no regions were specified for inclusion, assume
	 * that the user wants to write the complete new image.
	 */
	if (num_include_args == 0)
		return 0;

	/* Non-included romentries are ignored.
	 * The union of all included romentries is used from the new image.
	 */
	while (start < size) {
		entry = get_next_included_romentry(start);
		/* No more romentries for remaining region? */
		if (!entry) {
			memcpy(newcontents + start, oldcontents + start,
			       size - start);
			break;
		}
		/* For non-included region, copy from old content. */
		if (entry->start > start)
			memcpy(newcontents + start, oldcontents + start,
			       entry->start - start);
		/* For included region, copy from file if specified. */
		if (read_content_from_file(entry, newcontents) != 0)
			return 1;

		/* Skip to location after current romentry. */
		start = entry->end + 1;
		/* Catch overflow. */
		if (!start)
			break;
	}
	return 0;
}
