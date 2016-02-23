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
#include <stdbool.h>

#define SHIFTH(value, shift) \
	(value + (1 << shift) - 1) >> shift

#define DSK_HEADER "MV - CPC"
#define DSK_CREATOR "dsktools"
#define EDSK_HEADER "EXTENDED CPC DSK File"
#define DSK_TRACK_HEADER "Track-Info\r\n"
#define BASE_SECTOR_SIZE        128

typedef enum {
	DSK,
	EDSK
} dsk_image_type;

typedef struct {
	char magic[34];
	char creator[14];
	uint8_t tracks;
	uint8_t sides;
	uint8_t track_size[2];
	uint8_t track_size_high[204]; /* Only for EDSK */
} dsk_header_type;

typedef struct {
	char magic[35];
	char creator[15];
	dsk_image_type type;
	uint8_t tracks;
	uint8_t sides;
	uint32_t capacity;
	uint32_t sectors;
	uint8_t first_sector_id;
} dsk_info_type;

typedef struct {
	uint8_t track;
	uint8_t side;
	uint8_t sector_id;
	uint8_t size;
	uint8_t fdc_status_1;
	uint8_t fdc_status_2;
	uint8_t unused1;
	uint8_t unused2;
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
} track_header_type;

typedef struct {
	uint8_t *image;
	dsk_header_type *dsk_info;
	track_header_type **track_info;
} dsk_type;

dsk_type *dsk_new(const char *filename);
dsk_type *dsk_new_from_scratch(dsk_image_type type, 
			       uint8_t tracks, 
			       uint8_t sides,
			       uint16_t tracklen);
void dsk_delete(dsk_type *dsk);
dsk_info_type *dsk_info_get(dsk_type *dsk, dsk_info_type *info);
uint32_t dsk_sector_offset_get(dsk_type *dsk, 
			       uint8_t track_id, uint8_t side, 
			       uint8_t sector_id);
track_header_type *dsk_track_info_get(dsk_type *dsk,
				      uint8_t track,
				      bool validate);
int dsk_image_dump(dsk_type *dsk, const char *destination);
int dsk_sector_write(dsk_type *dsk, const uint8_t *src, uint8_t sector);
uint32_t dsk_track_size_get(dsk_type *dsk, uint8_t track);
int dsk_sector_read(dsk_type *dsk, uint8_t *dst, uint8_t sector);
int dsk_disk_write(dsk_type *dsk, const char *device);
int dsk_disk_read(dsk_type *dsk, const char *device);

#endif //DSK_H
