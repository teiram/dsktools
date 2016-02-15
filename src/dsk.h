/* 
 * dsk.h - DSK abstraction
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

#ifndef DSK_H
#define DSK_H

#include <stdint.h>

#define DSK_HEADER "MV - CPC"
#define NUM_DIRENT 64
#define AMSDOS_NAME_LEN 8
#define AMSDOS_EXT_LEN 3
#define SECTOR_SIZE 512
#define AMSDOS_USER_DELETED 0xE5

#define DSK_ERROR_SIZE 256

#define BASE_SECTOR_IBM 0x01
#define BASE_SECTOR_SYS 0x41
#define BASE_SECTOR_DATA 0xC1

typedef struct {
	char magic[34];
	char creator[14];
	uint8_t tracks;
	uint8_t sides;
	uint8_t track_size[2];
	uint8_t padding[204];
} dsk_info_type;

typedef struct {
	uint8_t track;
	uint8_t side;
	uint8_t sector_id;
	uint8_t size;
	uint8_t fdc_status_1;
	uint8_t fdc_status_2;
	uint16_t unused;
} sector_info_type;

typedef struct {
	char magic[13];
	uint8_t unused0[3];
	uint8_t track_number;
	uint8_t side_number;
	uint16_t unused1;
	uint8_t sector_size;
	uint8_t sector_count;
	uint8_t gap3_length;
	uint8_t unused2;
	sector_info_type sector_info[29];
} track_info_type;

typedef struct {
	uint8_t user;
	char name[8];
	uint8_t extension[3];
	uint8_t extent_low;
	uint8_t extent_high;
	uint8_t unused;
	uint8_t record_count;
	uint8_t blocks[16];
} dir_entry_type;

typedef struct {
	uint8_t *image;
	dsk_info_type *dsk_info;
	track_info_type **track_info;
	char *error;
} dsk_type;

dsk_type *dsk_new(const char *filename);
void dsk_delete(dsk_type *dsk);
uint32_t dsk_get_total_blocks(dsk_type *dsk);
uint32_t dsk_get_used_blocks(dsk_type *dsk);
char *dsk_get_error(dsk_type *dsk);
track_info_type *dsk_get_track_info(dsk_type *dsk, 
				    uint8_t track);

dir_entry_type *dsk_get_dir_entry(dsk_type *dsk, 
				  dir_entry_type *dir_entry, 
				  int index);

uint8_t is_dir_entry_deleted(dir_entry_type *dir_entry);
char *dir_entry_get_name(dir_entry_type *dir_entry, char *name);
uint32_t dir_entry_get_size(dir_entry_type* dir_entries, int index);
int dsk_dump_file(dsk_type *dsk, const char *name, const char *destination,
		  uint8_t user);
#endif //DSK_H
