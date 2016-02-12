/* 
 * dsk.c - DSK Abstraction
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
#include <stdint.h>
#include <string.h>
#include "dsk.h"

static int is_dsk_image(dsk_type *dsk) {
	if (dsk && dsk->dsk_info) {
		return !strncmp(dsk->dsk_info->magic, 
				DSK_HEADER, 8);
	} else {
		return 0;
	}
}

static int min_sector_index(track_info_type *track) {
	int i;
	uint8_t min = ~0;
	for (i = 0; i < track->sector_count; i++) {
		if (track->sector_info[i].sector_id < min) {
			min = track->sector_info[i].sector_id;
		}
	}
	return min;
}

static uint32_t get_track_size(dsk_type *dsk) {
	return dsk->dsk_info->track_size[0] +
		(dsk->dsk_info->track_size[1] << 8);
}

static uint32_t get_sector_offset(dsk_type *dsk, 
			   uint8_t track_id, uint8_t side, 
			   uint8_t sector_id) {
	int i,j;
	uint32_t offset = 0;
	uint32_t track_size = get_track_size(dsk);
	for (i = 0; i < dsk->dsk_info->tracks; i++) {
		track_info_type *track = dsk_get_track_info(dsk, i);
		if (track->track_number == track_id &&
		    track->side_number == side) {
			for (j = 0; j < track->sector_count; j++) {
				sector_info_type *sinfo = &track->sector_info[j];
				if (sinfo->sector_id == sector_id) {
					offset += sizeof(track_info_type);
					return offset;
				} else {
					offset += 128 << sinfo->size;
				}
			}
		} else {
			offset += track_size;
		}
	}
}

uint8_t is_dir_entry_deleted(dir_entry_type *dir_entry) {
	return dir_entry->user == 0xe5 ? 1 : 0;
}

char *dir_entry_get_name(dir_entry_type *dir_entry, char *name) {
	memcpy(name, dir_entry->name, 8);
	name[8] = '.';
	memcpy(name + 9, dir_entry->extension, 3);
	name[12] = 0;
	name[9] &= 0x7F;
	name[10] &= 0x7F;
	return name;
}

uint32_t dir_entry_get_size(dir_entry_type* dir_entries, int index) {
	int blocks = 0;
	int file_user = dir_entries[index].user;
	int lookahead = index;
	do {
		if (dir_entries[lookahead].user == file_user) {
			blocks += dir_entries[lookahead].record_count;
		}
		lookahead++;
	} while (dir_entries[lookahead].extent_low && lookahead < NUM_DIRENT);
	return blocks << 7;
}

track_info_type *dsk_get_track_info(dsk_type *dsk, 
				    uint8_t track) {
	if (dsk && dsk->image) {
		if (!dsk->track_info[track]) {
			dsk->track_info[track] = (track_info_type*) 
				(dsk->image + (get_track_size(dsk) * track));
		}
		return dsk->track_info[track];
	} 
	return 0;
}

void dsk_delete(dsk_type *dsk) {
	if (dsk) {
		if (dsk->dsk_info) {
			free(dsk->dsk_info);
		}
		if (dsk->image) {
			free(dsk->image);
		}
		if (dsk->track_info) {
			free(dsk->track_info);
		}
		free(dsk);
	}
}

dsk_type *dsk_new(const char *filename) {
	dsk_type *dsk = calloc(1, sizeof(dsk_type));
	FILE *fh = 0;
	if ((fh = fopen(filename, "r")) != 0) {
		dsk->dsk_info = (dsk_info_type *) malloc(sizeof(dsk_info_type));
		int nread = fread(dsk->dsk_info, 
				  sizeof(dsk_info_type), 1, fh);
		if (nread == 1 &&
		    is_dsk_image(dsk)) {
			int disk_size = dsk->dsk_info->tracks *
				get_track_size(dsk);
			dsk->image = (uint8_t*) malloc(disk_size);
			nread = fread(dsk->image,
				      disk_size, 1, fh);
			fclose(fh);
			if (nread == 1) {
				dsk->track_info = (track_info_type**) 
					calloc(dsk->dsk_info->tracks,
					       sizeof(track_info_type**));
				return dsk;
			}
		} else {
			printf("Unable to read %d, was %d\n",
			       sizeof(dsk_info_type), nread);
		}
	}
	if (fh != 0) {
		fclose(fh);
	}
	dsk_delete(dsk);
	return 0;
}
	

dir_entry_type *dsk_get_dir_entry(dsk_type *dsk, 
				  dir_entry_type *dir_entry, 
				  int index) {
	track_info_type *track0 = 
		dsk_get_track_info(dsk, 0);
	
	int base_sector = min_sector_index(track0);
	int sector_id = (index >> 4) + base_sector;
	int track = 0;
	if (base_sector == 0x41) {
		track = 2;
	} else if (base_sector == 1) {
		track = 1;
	}
	memcpy(dir_entry, dsk->image 
	       + get_sector_offset(dsk, track, 0, sector_id)
	       + ((index & 15) << 5),
	       sizeof(dir_entry_type));
	return dir_entry;
}


uint32_t dsk_get_total_blocks(dsk_type *dsk) {
	int i;
	uint32_t blocks = 0;
	for (i = 0; i < dsk->dsk_info->tracks; i++) {
		track_info_type *track = 
			dsk_get_track_info(dsk, i);
		blocks += track->sector_count * track->sector_size;
	}
	return blocks;
}

uint32_t dsk_get_used_blocks(dsk_type *dsk) {
	int i;
	dir_entry_type dir_entry;
	uint32_t blocks = 0;
	for (i = 0; i < NUM_DIRENT; i++) {
		dsk_get_dir_entry(dsk, &dir_entry, i);
		if (!is_dir_entry_deleted(&dir_entry)) {
			blocks += dir_entry.record_count;
		}
	}
	return blocks;
}
