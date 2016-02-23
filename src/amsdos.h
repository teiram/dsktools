/* 
 * amsdos.h - AMSDOS file system abstraction
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

#ifndef AMSDOS_H
#define AMSDOS_H

#include <stdint.h>
#include <stdbool.h>
#include "dsk.h"

#define AMSDOS_NUM_DIRENT       64
#define AMSDOS_NAME_LEN         8
#define AMSDOS_EXT_LEN          3
#define AMSDOS_USER_DELETED     0xE5
#define AMSDOS_RECORD_SIZE      128
#define AMSDOS_BLOCKS_DIRENT    16
#define AMSDOS_RECORDS_DIRENT   (AMSDOS_BLOCKS_DIRENT << 3)
#define AMSDOS_BINARY 2
#define AMSDOS_ERROR_SIZE 256
#define BASE_SECTOR_IBM 0x01
#define RESERVED_SECTORS_IBM 1
#define BASE_SECTOR_SYS 0x41
#define RESERVED_SECTORS_SYS 2
#define BASE_SECTOR_DATA 0xC1
#define AMSDOS_SECTOR_SIZE 512

#define AMSDOS_DISK_TYPE(type) \
	type == DISK_TYPE_DATA ? "DATA" : \
		type == DISK_TYPE_IBM ? "IBM" : \
		type == DISK_TYPE_SYSTEM ? "SYSTEM" : "UNKNOWN"

typedef enum {
	DISK_TYPE_DATA = 0,
	DISK_TYPE_IBM = 1,
	DISK_TYPE_SYSTEM = 2,
	DISK_TYPE_UNKNOWN
} amsdos_disk_type;

typedef struct {
	uint8_t user;
	char name[AMSDOS_NAME_LEN];
	uint8_t extension[AMSDOS_EXT_LEN];
	uint8_t extent_low;
	uint8_t extent_high;
	uint8_t unused;
	uint8_t record_count;
	uint8_t blocks[AMSDOS_BLOCKS_DIRENT];
} amsdos_dir_type;

typedef struct {
	uint8_t user;
	char name[AMSDOS_NAME_LEN];
	char extension[AMSDOS_EXT_LEN];
	uint8_t unused0[4];
	uint8_t block_number;
	uint8_t last_block;
	uint8_t type;
	uint8_t data_length;
	uint8_t load_address;
	uint8_t first_block;
	uint16_t logical_length;
	uint8_t entry_address;
	uint8_t unused1[36];
	uint16_t file_length;
	uint8_t unused2;
	uint16_t checksum;
	uint8_t unused3[59];
} amsdos_header_type;

typedef struct {
	dsk_type *dsk;
	uint8_t last_free_block;
} amsdos_type;

typedef struct {
	dsk_info_type dsk_info;
	uint32_t used;
	amsdos_disk_type type;
} amsdos_info_type;

amsdos_type *amsdos_new(const char *filename);
void amsdos_delete(amsdos_type *amsdos);

amsdos_info_type *amsdos_info_get(amsdos_type *amsdos, amsdos_info_type *info);

amsdos_dir_type *amsdos_dir_get(amsdos_type *amsdos,
				amsdos_dir_type *dir_entry, 
				int index);
void amsdos_dir_set(amsdos_type *amsdos,
		    amsdos_dir_type *dir_entry,
		    int index);
char *amsdos_dir_name_get(amsdos_dir_type *dir, char *buffer);
uint32_t amsdos_dir_size_get(amsdos_dir_type* dir_entries, int index);
bool amsdos_dir_deleted(amsdos_dir_type *dir_entry);

int amsdos_file_get(amsdos_type *amsdos, const char *name, 
		    uint8_t user,
		    const char *destination);

int amsdos_file_add(amsdos_type *amsdos, const char *source_file,
		    const char *target_file, uint8_t user);
int amsdos_ascii_file_add(amsdos_type *amsdos, const char *source_file, 
			  const char *target_name, uint8_t user);
int amsdos_binary_file_add(amsdos_type *amsdos, const char *source_file,
			   const char *target_name, uint8_t user, 
			   uint16_t load_address, uint16_t entry_address);
int amsdos_file_remove(amsdos_type *amsdos, const char *name, uint8_t user);
bool amsdos_file_exists(amsdos_type *amsdos, const char *name, uint8_t user);

#endif //AMSDOS_H
