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
#include "error.h"

typedef enum {
	LIST,
	INFO,
	EXPORT,
	DELETE,
	ADD,
	WRITE,
	READ,
	NEW
} option_type;

static int help_exit(const char *message, int exitcode) {
        if (message) {
                fprintf(stderr, "%s\n\n", message);
        }
        fprintf(stderr, "Usage: dskmgmt [options] <dskfile>\n");
	fprintf(stderr, " -Working modes. Mutually exclusive:\n");
        fprintf(stderr, " -i, --info\t\t\tshows dsk info\n");
        fprintf(stderr, " -l, --list\t\t\tshows dsk AMSDOS directory\n");
        fprintf(stderr, " -e, --export=FILENAME\t\texports file from dsk\n");
        fprintf(stderr, " -d, --delete=FILENAME\t\tdeletes file from dsk\n");
        fprintf(stderr, " -a, --add=FILENAME\t\tadds file to dsk\n");
        fprintf(stderr, " -w, --write=DEVICE\t\twrites dsk to device\n");
        fprintf(stderr, " -r, --read=DEVICE\t\treads device to dsk\n");
	fprintf(stderr, " -n, --new\t\t\tcreates a new empty dsk\n");
	fprintf(stderr, " -Modifying flags (with default values)\n");
        fprintf(stderr, " -u, --user=user\t\tuser for amsdos file operations (0)\n");
        fprintf(stderr, " -o, --destination=FILENAME\toutput filename\n");
	fprintf(stderr, " -t, --tracks=TRACKS\t\tnumber of tracks (40)\n");
	fprintf(stderr, " -s, --sides=SIDES\t\tnumber of sides (1)\n");
	fprintf(stderr, " -T, --tracklen=TRACKLEN\tsize of a track (0x1200)\n");
	fprintf(stderr, " -S, --sectorsize=SSIZE\t\tsector size (2)\n");
	fprintf(stderr, " -P, --sectrack=SECTRACK\tsectors per track (9)\n");
	fprintf(stderr, " -A, --amsdostype=TYPE\t\ttype of disk to create DATA,IBM,SYSTEM (DATA)\n");
        fprintf(stderr, " -h, --help\t\t\tshows this help\n");
        return exitcode;
}

int get_info(const char *filename) {
	amsdos_type *amsdos = amsdos_new(filename);
	if (amsdos) {
		amsdos_info_type info;
		amsdos_get_info(amsdos, &info);
		printf("DSK type\t: %s\n", info.dsk_info.type == DSK ? 
		       "DSK" : "EDSK");
		printf("Disk id\t\t: %s\n", info.dsk_info.magic);
		printf("Creator\t\t: %s\n", info.dsk_info.creator);
		printf("Disk type\t: %s\n",  AMSDOS_DISK_STR(info.type));
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
			amsdos_get_dir(amsdos, &dir_entries[i], i);
			LOG(LOG_DEBUG, 
			    "Entry for user %d, name %s, extent %d, records %u", 
			    dir_entries[i].user,
			    amsdos_get_dir_name(&dir_entries[i], buffer),
			    dir_entries[i].extent_low,
			    dir_entries[i].record_count);
		}
		for (int i = 0; i < AMSDOS_NUM_DIRENT; i++) {
			amsdos_dir_type *dir_entry = &dir_entries[i];
			
			if (!amsdos_is_dir_deleted(dir_entry) && 
			    dir_entry->extent_low == 0 &&
			    dir_entry->record_count > 0) {

				fprintf(stderr, "%s (user %d) %6d bytes\n", 
					amsdos_get_dir_name(dir_entry, buffer),
					dir_entry->user,
					amsdos_get_dir_size(dir_entries, i));
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
		int status = amsdos_get_file(amsdos, amsdos_filename, user,
					     dst_filename ? dst_filename : amsdos_filename);
		if (status != DSK_OK) {
			fprintf(stderr, "Failure: %s\n", error_get_error_message());
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
		int status = dsk_write_to_disk(dsk, device);
		if (status != DSK_OK) {
			fprintf(stderr, "Failure: %s\n", error_get_error_message());
		}
		dsk_delete(dsk);
		return status;
	} else {
		fprintf(stderr, "Unable to open DSK image\n");
		return DSK_ERROR;
	}
}

int read_dsk(const char *device, uint8_t tracks, uint8_t sides, 
	     const char *destination) {
	dsk_type *dsk = dsk_new_empty(DSK, tracks, sides, 0x1200);
	int status = dsk_read_from_disk(dsk, device);
	if (status == DSK_OK) {
		dsk_save_image(dsk, destination);
	} else {
		fprintf(stderr, "Failure: %s\n", error_get_error_message());
	}
	dsk_delete(dsk);
	return status;
}

int new_dsk(const char *destination, uint8_t tracks, uint8_t sides, 
	    uint8_t sectors_per_track, uint8_t sector_size,
	    uint16_t tracklen, amsdos_disk_type type) {
	amsdos_type *amsdos = amsdos_new_empty(tracks, sides, 
					       sectors_per_track,
					       sector_size,
					       tracklen,
					       type);
	int status = dsk_save_image(amsdos->dsk, destination);
	amsdos_delete(amsdos);
	if (status != DSK_OK) {
		fprintf(stderr, "Failure: %s", error_get_error_message());
	}
	return status;
}

int add_to_dsk(const char *dsk_filename, const char *source_file,
	       const char *dst_filename, uint8_t user) {
	amsdos_type *amsdos = amsdos_new(dsk_filename);
	if (amsdos) {
		int status = amsdos_add_file(amsdos, source_file,
					     source_file, user);
		if (status != DSK_OK) {
			fprintf(stderr, "Failure: %s\n", error_get_error_message());
		} else {
			dsk_save_image(amsdos->dsk, dst_filename);
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
                int status = amsdos_remove_file(amsdos, amsdos_filename, user);
                if (status != DSK_OK) {
                        fprintf(stderr, "Failure: %s\n", error_get_error_message());
                } else {
			dsk_save_image(amsdos->dsk, dst_filename);
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
                {"delete", 1, 0, 'd'},
                {"add", 1, 0, 'a'},
                {"write", 1, 0, 'w'},
                {"read", 1, 0, 'r'},
		{"new", 0, 0, 'n'},
		{"tracks", 1, 0, 't'},
		{"sides", 1, 0, 's'},
		{"tracklen", 1, 0, 'T'},
		{"sectorsize", 1, 0, 'S'},
		{"sectrack", 1, 0, 'P'},
		{"amsdostype", 1, 0, 'A'},
                {0, 0, 0, 0}
        };
        int c;
        option_type option = INFO;
        char *dsk_filename = NULL;
        char *target_filename = NULL;
        uint8_t target_user = 0;
        char *output_filename = NULL;
	uint8_t tracks = 40;
	uint8_t sides = 1;
	uint8_t sectors_per_track = 9;
	uint8_t sectorsize = 2;
	uint16_t tracklen = 0x1200;
	amsdos_disk_type type = DISK_TYPE_DATA;

        do {
                int option_index = 0;
                c = getopt_long(argc, argv, "lie:u:o:d:r:a:w:tsnTSPhA:",
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
		case 'n':
			option = NEW;
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
                case 'd':
                        option = DELETE;
                        target_filename = optarg;
                        break;
		case 'w':
			option = WRITE;
			target_filename = optarg;
			break;
		case 'r':
			option = READ;
			target_filename = optarg;
			break;
		case 't':
                        tracks = atoi(optarg);
                        break;			
		case 's':
			sides = atoi(optarg);
			break;
		case 'T':
			tracklen = atoi(optarg);
			break;
		case 'S':
			sectorsize = atoi(optarg);
			break;
		case 'P':
			sectors_per_track = atoi(optarg);
			break;
		case 'A':
			type = AMSDOS_DISK_TYPE(optarg);
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
                return get_info(dsk_filename);
        case EXPORT:
                return export_dsk(dsk_filename, target_filename,
                                  output_filename, target_user);
        case DELETE:
                return remove_from_dsk(dsk_filename, target_filename,
                                       output_filename, target_user);
	case ADD:
		return add_to_dsk(dsk_filename, target_filename,
				  output_filename, target_user);
	case WRITE:
		return write_dsk(dsk_filename, target_filename);
	case READ:
		return read_dsk(target_filename, tracks, 
				sides, dsk_filename);
	case NEW:
		return new_dsk(dsk_filename, tracks, sides, 
			       sectors_per_track, sectorsize, 
			       tracklen, type);
        }
        return result;
}
