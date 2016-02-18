/* 
 * dskmgmt.c - Manages a DSK file
 * Copyright (C)2016 Manuel Teira <manuel.teira@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include "dsk.h"
#include "log.h"

typedef enum {
	LIST,
	INFO,
	EXPORT,
	REMOVE,
	ADD
} option_type;

static int help_exit(const char *message, int exitcode) {
        if (message) {
                fprintf(stderr, "%s\n\n", message);
        }
        fprintf(stderr, "Usage: dskmgmt [options] file.dsk\n");
        fprintf(stderr, " Options: -l          | --list    shows directory\n");
        fprintf(stderr, " Options: -i          | --info    shows info\n");
        fprintf(stderr, " Options: -e filename | --export file from dsk\n");
        fprintf(stderr, " Options: -r filename | --remove file from dsk\n");
        fprintf(stderr, " Options: -u user | --user operation user\n");
        fprintf(stderr, " Options: -o destination | --destination destination file\n");
        fprintf(stderr, "          -h | --help    shows this help\n");
        return exitcode;
}

int info_dsk(const char *filename) {
	dsk_type *dsk = dsk_new(filename);
	if (dsk) {
		char id[35];
		char creator[15];
		strncpy(id, dsk->dsk_info->magic, 34);
		id[34] = 0;
		strncpy(creator, dsk->dsk_info->creator, 14);
		creator[14] = 0;
		printf("Disk id\t\t: %s\n", id);
		printf("Creator\t\t: %s\n", creator);
		printf("Tracks\t\t: %6d\n", dsk->dsk_info->tracks);
		printf("Sides\t\t: %6d\n", dsk->dsk_info->sides);
		printf("Total size\t: %6d bytes\n", dsk_get_total_blocks(dsk) << 7);
		printf("Used\t\t: %6d bytes\n", dsk_get_used_blocks(dsk) << 7);
		dsk_delete(dsk);
		return DSK_OK;

	} else {
		return DSK_ERROR;
	}
}

int list_dsk(const char *filename) {
	dsk_type *dsk = dsk_new(filename);
	if (dsk) {
		dir_entry_type dir_entries[NUM_DIRENT];
		char buffer[13];
		for (int i = 0; i < NUM_DIRENT; i++) {
			dsk_get_dir_entry(dsk, &dir_entries[i], i);
			LOG(LOG_DEBUG, 
			    "Entry for user %d, name %s, extent %d, records %u", 
			    dir_entries[i].user,
			    dir_entry_get_name(&dir_entries[i], buffer),
			    dir_entries[i].extent_low,
			    dir_entries[i].record_count);
		}
		for (int i = 0; i < NUM_DIRENT; i++) {
			dir_entry_type *dir_entry = &dir_entries[i];

			if (!is_dir_entry_deleted(dir_entry) && 
			    dir_entry->extent_low == 0 &&
			    dir_entry->record_count > 0) {

				fprintf(stderr, "%s (user %d) %6d bytes\n", 
					dir_entry_get_name(dir_entry, buffer),
					dir_entry->user,
					dir_entry_get_size(dir_entries, i));
			}
		}
		dsk_delete(dsk);
		return DSK_OK;
	} else {
		return DSK_ERROR;
	}
}

int export_dsk(const char *dsk_filename, const char *amsdos_filename, 
	       const char *dst_filename, uint8_t user) {
	dsk_type *dsk = dsk_new(dsk_filename);
	if (dsk) {
		int status = dsk_dump_file(dsk, amsdos_filename, 
					   dst_filename ? dst_filename : amsdos_filename,
					   user);
		if (status != DSK_OK) {
			fprintf(stderr, "Failure: %s\n", dsk_get_error(dsk));
		}
		dsk_delete(dsk);
		return status;
	} else {
		fprintf(stderr, "Unable to open DSK\n");
		return DSK_ERROR;
	}
}

int add_to_dsk(const char *dsk_filename, const char *source_file,
	       const char *dst_filename, uint8_t user) {
	dsk_type *dsk = dsk_new(dsk_filename);
	if (dsk) {
		int status = dsk_add_file(dsk, source_file,
					  source_file,
					  ASCII, user);
		if (status != DSK_OK) {
			fprintf(stderr, "Failure: %s\n", dsk_get_error(dsk));
		} else {
			dsk_dump_image(dsk, dst_filename);
		}
		dsk_delete(dsk);
		return status;
	} else {
		fprintf(stderr, "Unable to open DSK\n");
		return DSK_ERROR;
	}
}

int remove_from_dsk(const char *dsk_filename, const char *amsdos_filename,
                    const char *dst_filename, uint8_t user) {
        dsk_type *dsk = dsk_new(dsk_filename);
        if (dsk) {
                int status = dsk_remove_file(dsk, amsdos_filename,
                                             dst_filename, user);
                if (status != DSK_OK) {
                        fprintf(stderr, "Failure: %s\n", dsk_get_error(dsk));
                }
                return status;
        } else {
                fprintf(stderr, "Unable to open DSK\n");
                return DSK_ERROR;
        }
}

int main(int argc, char *argv[]) {

        static struct option long_options[] = {
                {"list", 0, 0, 'l'},
                {"info", 0, 0, 'i'},
                {"export", 1, 0, 'e'},
                {"user", 1, 0, 'u'},
                {"destination", 1, 0, 'o'},
                {"remove", 1, 0, 'r'},
                {"add", 1, 0, 'a'},
                {0, 0, 0, 0}
        };
        int c;
        option_type option = INFO;
        char *dsk_filename;
        char *target_filename = NULL;
        uint8_t target_user = 0;
        char *output_filename = NULL;

        do {
                int option_index = 0;
                c = getopt_long(argc, argv, "lie:u:o:r:a:h",
				long_options, &option_index);
                switch(c) {
                case 'h':
                case '?':
                        exit(help_exit(0, 0));
                        break;
                case 'l':
                        option = LIST;
                        break;
                case 'i':
                        option = INFO;
                        break;
                case 'e':
                        option = EXPORT;
                        target_filename = optarg;
                        break;
                case 'u':
                        target_user = atoi(optarg);
                        break;
                case 'o':
                        output_filename = optarg;
                        break;
		case 'a':
			option = ADD;
			target_filename = optarg;
			break;
                case 'r':
                        option = REMOVE;
                        target_filename = optarg;
                        break;
                }
        } while (c != -1);

        if (argc - optind != 1) {
                exit(help_exit("Error: No dsk filename provided", 1));
        } else {
                dsk_filename = argv[optind];
        }

        int result = 0;
        switch (option) {
        case LIST:
                return list_dsk(dsk_filename);
        case INFO:
                return info_dsk(dsk_filename);
        case EXPORT:
                return export_dsk(dsk_filename, target_filename,
                                  output_filename, target_user);
        case REMOVE:
                return remove_from_dsk(dsk_filename, target_filename,
                                       output_filename, target_user);
	case ADD:
		return add_to_dsk(dsk_filename, target_filename,
				  output_filename, target_user);
        }
        return result;
}
