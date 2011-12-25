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
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#ifndef __LIBPAYLOAD__
#include <libgen.h>
#include <fcntl.h>
#include <sys/stat.h>
#endif
#include "flash.h"
#include "programmer.h"

#define MAX_ROMLAYOUT	32
#define MAX_ENTRY_LEN	1024
#define WHITESPACE_CHARS " \t"
#define INCLUDE_INSTR "source"

typedef struct {
	unsigned int start;
	unsigned int end;
	unsigned int included;
	char *name;
	char *file;
} romentry_t;

/* rom_entries store the entries specified in a layout file and associated run-time data */
static romentry_t rom_entries[MAX_ROMLAYOUT];
static int num_rom_entries = 0; /* the number of valid rom_entries */

/* include_args holds the arguments specified at the command line with -i. They must be processed at some point
 * so that desired regions are marked as "included" in the rom_entries list. */
static char *include_args[MAX_ROMLAYOUT];
static int num_include_args = 0; /* the number of valid include_args. */

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

#ifndef __LIBPAYLOAD__
/** Parse a \em possibly quoted string.
 *
 * The function expects \a startp to point to the string to be parsed without leading white space. If the
 * string starts with a quotation mark it looks for the second one and removes both in-place, if not then it
 * looks for the first white space character and uses that as delimiter of the wanted string.
 * After returning \a startp will point to a string that is either the original string with the quotation marks
 * removed, or the first word of that string before a white space.
 * If \a endp is not NULL it will be set to point to the first character after the parsed string, which might
 * well be the '\0' at the end of the string pointed to by \a startp if there are no more characters.
 *
 * @param start	Points to the input string.
 * @param end	Is set to the first char following the input string.
 * @return	0 on success, 1 if the parsed string is empty or a quotation mark is not matched.
 */
static int unquote_string(char **startp, char **endp)
{
	char *end;
	size_t len;
	if (**startp == '"') {
		(*startp)++;
		len = strcspn(*startp, "\"");
	} else {
		len = strcspn(*startp, WHITESPACE_CHARS);
	}
	if (len == 0)
		return 1;

	end = *startp + len;
	if (*end != '\0') {
		*end = '\0';
		end++;
	}
	if (endp != NULL)
		*endp = end;
	msg_gspew("%s: start=\"%s\", end=\"%s\"\n", __func__, *startp, end);
	return 0;
}

/** Parse one line in a layout file.
 * @param	file_name The name of the file this line originates from.
 * @param	file_version The version of the layout specification to be used.
 * @param	entry If not NULL fill it with the parsed data, else just detect errors and print diagnostics.
 * @param	file_version Indicates the version of the layout file to be expected.
 * @return	1 on error,
 *		0 if the line could be parsed into a layout entry succesfully,
 *		-1 if a file was successfully sourced.
 */
static int parse_entry(char *file_name, int file_version, char *buf, romentry_t *entry)
{
	int addr_base;
	if (file_version == 1)
		addr_base = 16;
	else if (file_version >= 2)
		addr_base = 0; /* autodetect */
	else
		return 1;

	msg_gdbg2("String to parse: \"%s\".\n", buf);

	/* Skip all white space in the beginning. */
	char *tmp_str = buf + strspn(buf, WHITESPACE_CHARS);
	char *endptr;

	/* Check for include command. */
	if (file_version >= 2 && strncmp(tmp_str, INCLUDE_INSTR, strlen(INCLUDE_INSTR)) == 0) {
		tmp_str += strlen(INCLUDE_INSTR);
		tmp_str += strspn(tmp_str, WHITESPACE_CHARS);
		if (unquote_string(&tmp_str, NULL) != 0) {
			msg_gerr("Error parsing version %d layout entry: Could not find file name in \"%s\".\n",
				 file_version, buf);
				return 1;
		}
		msg_gspew("Source command found with filename \"%s\".\n", tmp_str);

		int ret;
		/* If a relative path is given, append it to the dirname of the current file. */
		if (*tmp_str != '/') {
			/* We need space for: dirname of file_name, '/' , the file name in tmp_strand and '\0'.
			 * Since the dirname of file_name is shorter than file_name this is more than enough: */
			char *path = malloc(strlen(file_name) + strlen(tmp_str) + 2);
			if (path == NULL) {
				msg_gerr("Out of memory!\n");
				return 1;
			}
			strcpy(path, file_name);

			/* A less insane but incomplete dirname implementation... */
			endptr = strrchr(path, '/');
			if (endptr != NULL) {
				endptr[0] = '/';
				endptr[1] = '\0';
			} else {
				/* This is safe because the original file name was at least one char. */
				path[0] = '.';
				path[1] = '/';
				path[2] = '\0';
			}
			strcat(path, tmp_str);
			ret = read_romlayout(path);
			free(path);
		} else
			ret = read_romlayout(tmp_str);
		return ret == 0 ? -1 : 1;
	}

	errno = 0;
	long tmp_long = strtol(tmp_str, &endptr, addr_base);
	if (errno != 0 || endptr == tmp_str || tmp_long < 0 || tmp_long > UINT32_MAX) {
		msg_gerr("Error parsing version %d layout entry: Could not convert start address in \"%s\".\n",
			 file_version, buf);
		return 1;
	}
	uint32_t start = tmp_long;

	tmp_str = endptr + strspn(endptr, WHITESPACE_CHARS);
	if (*tmp_str != ':') {
		msg_gerr("Error parsing version %d layout entry: Address separator does not follow start address in \"%s\""
			 ".\n", file_version, buf);
		return 1;
	}
	tmp_str++;

	errno = 0;
	tmp_long = strtol(tmp_str, &endptr, addr_base);
	if (errno != 0 || endptr == tmp_str || tmp_long < 0 || tmp_long > UINT32_MAX) {
		msg_gerr("Error parsing version %d layout entry: Could not convert end address in \"%s\"\n",
			 file_version, buf);
		return 1;
	}
	uint32_t end = tmp_long;

	size_t skip = strspn(endptr, WHITESPACE_CHARS);
	if (skip == 0) {
		msg_gerr("Error parsing version %d layout entry: End address is not followed by white space in "
			 "\"%s\"\n", file_version, buf);
		return 1;
	}

	tmp_str = endptr + skip;
	/* The region name is either enclosed by quotes or ends with the first whitespace. */
	if (unquote_string(&tmp_str, &endptr) != 0) {
		msg_gerr("Error parsing version %d layout entry: Could not find region name in \"%s\".\n",
			 file_version, buf);
		return 1;
	}

	msg_gdbg("Parsed entry: 0x%08x - 0x%08x named \"%s\"\n", start, end, tmp_str);

	if (start >= end) {
		msg_gerr("Error parsing version %d layout entry: Length of region \"%s\" is not positive.\n",
			 file_version, tmp_str);
		return 1;
	}

	if (find_romentry(tmp_str) >= 0) {
		msg_gerr("Error parsing version %d layout entry: Region name \"%s\" used multiple times.\n",
			 file_version, tmp_str);
		return 1;
	}

	endptr += strspn(endptr, WHITESPACE_CHARS);
	if (strlen(endptr) != 0)
		msg_gerr("Warning: Region name \"%s\" is not followed by white space only.\n", tmp_str);

	if (entry != NULL) {
		entry->name = strdup(tmp_str);
		if (entry->name == NULL) {
			msg_gerr("Out of memory!\n");
			return 1;
		}

		entry->start = start;
		entry->end = end;
		entry->included = 0;
		entry->file = NULL;
	}
	return 0;
}

/* Scan the first line for the determinant version comment and parse it, or assume it is version 1. */
int detect_layout_version(FILE *romlayout)
{
	int c;
	do { /* Skip white space */
		c = fgetc(romlayout);
		if (c == EOF)
			return -1;
	} while (isblank(c));
	ungetc(c, romlayout);

	const char* vcomment = "# flashrom layout ";
	char buf[strlen(vcomment) + 1]; /* comment + \0 */
	if (fgets(buf, sizeof(buf), romlayout) == NULL)
		return -1;
	if (strcmp(vcomment, buf) != 0)
		return 1;
	int version;
	if (fscanf(romlayout, "%d", &version) != 1)
		return -1;
	if (version < 2) {
		msg_gwarn("Warning: Layout file declares itself to be version %d, but self delcaration has\n"
			  "only been possible since version 2. Continuing anyway.\n", version);
	}
	return version;
}

int read_romlayout(char *name)
{
	FILE *romlayout = fopen(name, "r");
	if (romlayout == NULL) {
		msg_gerr("ERROR: Could not open layout file \"%s\".\n", name);
		return -1;
	}

	int file_version = detect_layout_version(romlayout);
	if (file_version < 0) {
		msg_gerr("Could not determine version of layout file \"%s\".\n", name);
		fclose(romlayout);
		return 1;
	}
	if (file_version < 1 || file_version > 2) {
		msg_gerr("Unknown layout file version: %d\n", file_version);
		fclose(romlayout);
		return 1;
	}
	rewind(romlayout);

	msg_gdbg("Parsing layout file \"%s\" according to version %d.\n", name, file_version);
	int linecnt = 0;
	while (!feof(romlayout)) {
		char buf[MAX_ENTRY_LEN];
		char *curchar = buf;
		linecnt++;
		msg_gspew("Parsing line %d of \"%s\".\n", linecnt, name);

		while (true) {
			char c = fgetc(romlayout);
			if (c == '#') {
				if (file_version == 1) {
					msg_gerr("Line %d of version %d layout file \"%s\" contains a "
						 "forbidden #.\n", linecnt, file_version, name);
					fclose(romlayout);
					return 1;
				}
				do { /* Skip characters in comments */
					c = fgetc(romlayout);
				} while (c != EOF && c != '\n');
				continue;
			}
			if (c == EOF || c == '\n') {
				*curchar = '\0';
				break;
			}
			if (curchar == &buf[MAX_ENTRY_LEN - 1]) {
				msg_gerr("Line %d of layout file \"%s\" is longer than the allowed %d chars.\n",
					 linecnt, name, MAX_ENTRY_LEN);
				fclose(romlayout);
				return 1;
			}
			*curchar = c;
			curchar++;
		}

		/* Skip all whitespace or empty lines */
		if (strspn(buf, WHITESPACE_CHARS) == strlen(buf))
			continue;

		romentry_t *entry = (num_rom_entries >= MAX_ROMLAYOUT) ? NULL : &rom_entries[num_rom_entries];
		int ret = parse_entry(name, file_version, buf, entry);
		if (ret > 0) {
			fclose(romlayout);
			return 1;
		}
		/* Only 0 indicates the successfully parsing of a layout entry, -1 indicates a sourced file. */
		if (ret == 0)
			num_rom_entries++;
	}
	fclose(romlayout);
	if (num_rom_entries >= MAX_ROMLAYOUT) {
		msg_gerr("Found %d entries in layout file which is more than the %i allowed.\n",
			 num_rom_entries + 1, MAX_ROMLAYOUT);
		return 1;
	}
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
		free(rom_entries[i].name);
		rom_entries[i].name = NULL;
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

static void copy_old_content(struct flashctx *flash, int oldcontents_valid, uint8_t *oldcontents, uint8_t *newcontents, unsigned int start, unsigned int size)
{
	if (!oldcontents_valid) {
		/* oldcontents is a zero-filled buffer. By reading into
		 * oldcontents, we avoid a rewrite of identical regions even if
		 * an initial full chip read didn't happen. */
		msg_gdbg2("Read a chunk starting from 0x%06x (len=0x%06x).\n",
			  start, size);
		flash->chip->read(flash, oldcontents + start, start, size);
	}
	memcpy(newcontents + start, oldcontents + start, size);
}

/**
 * Modify @newcontents so that it contains the data that should be on the chip
 * eventually. In the case the user wants to update only parts of it, copy
 * the chunks to be preserved from @oldcontents to @newcontents. If @oldcontents
 * is not valid, we need to fetch the current data from the chip first.
 */
int build_new_image(struct flashctx *flash, int oldcontents_valid, uint8_t *oldcontents, uint8_t *newcontents)
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
			copy_old_content(flash, oldcontents_valid, oldcontents,
					 newcontents, start, size - start);
			break;
		}
		/* For non-included region, copy from old content. */
		if (entry->start > start)
			copy_old_content(flash, oldcontents_valid, oldcontents,
					 newcontents, start,
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
