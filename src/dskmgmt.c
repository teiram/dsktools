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
#include "amsdos.h"
#include "log.h"

typedef enum {
	LIST,
	INFO,
	EXPORT,
	REMOVE,
	ADD,
	WRITE
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

int get_info(const char *filename) {
	amsdos_type *amsdos = amsdos_new(filename);
	if (amsdos) {
		amsdos_info_type info;
		amsdos_info_get(amsdos, &info);
		printf("DSK type\t: %s\n", info.dsk_info.type == DSK ? 
		       "DSK" : "EDSK");
		printf("Disk id\t\t: %s\n", info.dsk_info.magic);
		printf("Creator\t\t: %s\n", info.dsk_info.creator);
		printf("Disk type\t: %s\n",  AMSDOS_DISK_TYPE(info.type));
		printf("Tracks\t\t: %4d\n", info.dsk_info.tracks);
		printf("Sides\t\t: %4d\n", info.dsk_info.sides);
		printf("Sectors\t\t: %4d\n", info.dsk_info.sectors);
		printf("First sector id\t: 0x%02X\n", info.dsk_info.first_sector_id);
		printf("Total size\t: %6d bytes\n", info.dsk_info.capacity);
		printf("Used\t\t: %6d bytes\n", info.used);

		amsdos_delete(amsdos);
		return DSK_OK;
	} else {
		return DSK_ERROR;
	}
}

int list_dsk(const char *filename) {
	amsdos_type *amsdos = amsdos_new(filename);
	if (amsdos) {
		amsdos_dir_type dir_entries[AMSDOS_NUM_DIRENT];
		char buffer[13];
		for (int i = 0; i < AMSDOS_NUM_DIRENT; i++) {
			amsdos_dir_get(amsdos, &dir_entries[i], i);
			LOG(LOG_DEBUG, 
			    "Entry for user %d, name %s, extent %d, records %u", 
			    dir_entries[i].user,
			    amsdos_dir_name_get(&dir_entries[i], buffer),
			    dir_entries[i].extent_low,
			    dir_entries[i].record_count);
		}
		for (int i = 0; i < AMSDOS_NUM_DIRENT; i++) {
			amsdos_dir_type *dir_entry = &dir_entries[i];
			
			if (!amsdos_dir_deleted(dir_entry) && 
			    dir_entry->extent_low == 0 &&
			    dir_entry->record_count > 0) {

				fprintf(stderr, "%s (user %d) %6d bytes\n", 
					amsdos_dir_name_get(dir_entry, buffer),
					dir_entry->user,
					amsdos_dir_size_get(dir_entries, i));
			}
		}
		amsdos_delete(amsdos);
		return DSK_OK;
	} else {
		return DSK_ERROR;
	}
}

int export_dsk(const char *dsk_filename, const char *amsdos_filename, 
	       const char *dst_filename, uint8_t user) {
	amsdos_type *amsdos = amsdos_new(dsk_filename);
	if (amsdos) {
		int status = amsdos_file_get(amsdos, amsdos_filename, user,
					     dst_filename ? dst_filename : amsdos_filename);
		if (status != DSK_OK) {
			fprintf(stderr, "Failure: %s\n", 
				amsdos_error_get(amsdos));
		} 
		amsdos_delete(amsdos);
		return status;
	} else {
		fprintf(stderr, "Unable to open AMSDOS filesystem\n");
		return DSK_ERROR;
	}
}

int write_dsk(const char *dsk_filename, const char *device) {
	dsk_type *dsk = dsk_new(dsk_filename);
	if (dsk) {
		int status = dsk_disk_write(dsk, device);
		if (status != DSK_OK) {
			fprintf(stderr, "Failure: %s\n", 
				dsk_error_get(dsk));
		}
		dsk_delete(dsk);
		return status;
	} else {
		fprintf(stderr, "Unable to open DSK image\n");
		return DSK_ERROR;
	}
}

int add_to_dsk(const char *dsk_filename, const char *source_file,
	       const char *dst_filename, uint8_t user) {
	amsdos_type *amsdos = amsdos_new(dsk_filename);
	if (amsdos) {
		int status = amsdos_file_add(amsdos, source_file,
					     source_file, user);
		if (status != DSK_OK) {
			fprintf(stderr, "Failure: %s\n", 
				amsdos_error_get(amsdos));
		} else {
			dsk_image_dump(amsdos->dsk, dst_filename);
		}
		amsdos_delete(amsdos);
		return status;
	} else {
		fprintf(stderr, "Unable to open AMSDOS filesystem\n");
		return DSK_ERROR;
	}
}

int remove_from_dsk(const char *dsk_filename, const char *amsdos_filename,
                    const char *dst_filename, uint8_t user) {
        amsdos_type *amsdos = amsdos_new(dsk_filename);
        if (amsdos) {
                int status = amsdos_file_remove(amsdos, amsdos_filename, user);
                if (status != DSK_OK) {
                        fprintf(stderr, "Failure: %s\n", 
				amsdos_error_get(amsdos));
                } else {
			dsk_image_dump(amsdos->dsk, dst_filename);
		}
                return status;
        } else {
                fprintf(stderr, "Unable to open AMSDOS filesystem\n");
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
                {"write", 1, 0, 'w'},
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
                c = getopt_long(argc, argv, "lie:u:o:r:a:w:h",
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
		case 'w':
			option = WRITE;
			target_filename = optarg;
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
                return get_info(dsk_filename);
        case EXPORT:
                return export_dsk(dsk_filename, target_filename,
                                  output_filename, target_user);
        case REMOVE:
                return remove_from_dsk(dsk_filename, target_filename,
                                       output_filename, target_user);
	case ADD:
		return add_to_dsk(dsk_filename, target_filename,
				  output_filename, target_user);
	case WRITE:
		return write_dsk(dsk_filename, target_filename);
        }
        return result;
}
