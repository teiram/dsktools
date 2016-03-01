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
#include <string.h>
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

#define AMSDOS_DISK_STR(type) \
	type == DISK_TYPE_DATA ? "DATA" : \
		type == DISK_TYPE_IBM ? "IBM" : \
		type == DISK_TYPE_SYSTEM ? "SYSTEM" : "UNKNOWN"

#define AMSDOS_DISK_TYPE(str) \
	strcmp(str, "DATA") ? strcmp(str, "IBM") ? strcmp(str, "SYSTEM") ? DISK_TYPE_UNKNOWN : DISK_TYPE_SYSTEM : DISK_TYPE_IBM : DISK_TYPE_DATA

#define AMSDOS_FILE_TYPE(type) \
	type == AMSDOS_TYPE_BASIC ? "BASIC" : \
		type == AMSDOS_TYPE_BASIC_PROTECTED ? "BASIC (PROTECTED)" : \
		type == AMSDOS_TYPE_BINARY ? "BINARY" : \
		type == AMSDOS_TYPE_BINARY_PROTECTED ? "BINARY (PROTECTED)" : \
		type == AMSDOS_TYPE_ASCII ? "ASCII" : "UNKNOWN"

#define AMSDOS_FLAG_REP(flags, item, onrep, offrep) \
	flags & item ? onrep : offrep

#define AMSDOS_FLAGS_FROM_REP(rep, pattern) \
	rep[0] == pattern[0] ? READ_ONLY : 0x0 | \
		rep[1] == pattern[1] ? SYSTEM : 0x0 | \
		rep[2] == pattern[2] ? ARCHIVED : 0x0

typedef enum {
	DISK_TYPE_DATA = 0,
	DISK_TYPE_IBM = 1,
	DISK_TYPE_SYSTEM = 2,
	DISK_TYPE_UNKNOWN
} amsdos_disk_type;

typedef enum {
	AMSDOS_TYPE_BASIC = 0,
	AMSDOS_TYPE_BASIC_PROTECTED = 1,
	AMSDOS_TYPE_BINARY = 2,
	AMSDOS_TYPE_BINARY_PROTECTED = 3,
	AMSDOS_TYPE_ASCII = 4,
	AMSDOS_TYPE_UNKNOWN
} amsdos_file_type;

typedef enum {
	READ_ONLY = 1,
	SYSTEM = 2,
	ARCHIVED = 4
} amsdos_attribute_type;

#pragma pack(1)

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

typedef struct _amsdos_file_info_list {
	char name[AMSDOS_NAME_LEN + 1];
	char extension[AMSDOS_EXT_LEN + 1];
	uint8_t flags;
	uint32_t size;
	uint8_t extents;
	uint8_t amsdos_type;
	uint16_t load_address;
	uint16_t exec_address;
	struct _amsdos_file_info_list *next;
} amsdos_file_info_list;

typedef struct {
	uint8_t user;                   //Offset 00 (0x00)
	char name[AMSDOS_NAME_LEN];     //Offset 01-08 (0x01-0x08)
	char extension[AMSDOS_EXT_LEN]; //Offset 09-11 (0x09-0x0b)
	uint8_t unused0[4];             //Offset 12-15 (0x0c-0x0f)
	uint8_t block_number;           //Offset 16 (0x10)
	uint8_t last_block;             //Offset 17 (0x11)
	uint8_t type;                   //Offset 18 (0x12)
	uint16_t data_length;           //Offset 19-20 (0x13-0x14)
	uint16_t load_address;          //Offset 21-22 (0x15-0x16)
	uint8_t first_block;            //Offset 23 (0x17)
	uint16_t logical_length;        //Offset 24-25 (0x18-0x19)
	uint16_t entry_address;         //Offset 26-27 (0x1a-0x1b)
	uint8_t unused1[36];            //Offset 28 (0x1c)
	uint16_t file_length;           //Offset 64-65 (0x40-0x41)
	uint8_t unused2;                //Offset 66 (0x42)
	uint16_t checksum;              //Offset 67-68 (0x43-0x44)
	uint8_t unused3[59];            //Offset 69-128 (0x45-0x80)
} amsdos_header_type;

typedef struct {
	dsk_type *dsk;
	amsdos_dir_type *dir_entries[AMSDOS_NUM_DIRENT];
	uint8_t last_free_block;
} amsdos_type;

typedef struct {
	dsk_info_type dsk_info;
	uint32_t used;
	amsdos_disk_type type;
} amsdos_info_type;

amsdos_type *amsdos_new(const char *filename);
amsdos_type *amsdos_new_empty(uint8_t tracks, uint8_t sides, 
			      uint8_t sectors_per_track,
			      uint8_t sector_size,
			      uint16_t tracklen, 
			      amsdos_disk_type type);
void amsdos_delete(amsdos_type *amsdos);

amsdos_info_type *amsdos_get_info(amsdos_type *amsdos, amsdos_info_type *info);

amsdos_dir_type *amsdos_get_dir(amsdos_type *amsdos, int index);
char *amsdos_get_dir_name(amsdos_dir_type *dir, char *buffer);
char *amsdos_get_dir_basename(amsdos_dir_type *dir, char *buffer);
char *amsdos_get_dir_extension(amsdos_dir_type *dir, char *buffer);
uint32_t amsdos_get_dir_size(amsdos_type* amsdos, int index);
bool amsdos_is_dir_deleted(amsdos_dir_type *dir_entry);

int amsdos_get_file(amsdos_type *amsdos, const char *name, 
		    uint8_t user,
		    const char *destination);

amsdos_file_info_list *amsdos_get_file_info_list(amsdos_type *amsdos);
amsdos_file_info_list *amsdos_get_file_info(amsdos_type *amsdos, int index);
void amsdos_file_info_free(amsdos_file_info_list *list);

int amsdos_add_file(amsdos_type *amsdos, const char *source_file,
		    const char *target_file, uint8_t user);
int amsdos_add_ascii_file(amsdos_type *amsdos, const char *source_file, 
			  const char *target_name, uint8_t user);
int amsdos_add_binary_file(amsdos_type *amsdos, const char *source_file,
			   const char *target_name, uint8_t user, 
			   uint16_t load_address, uint16_t entry_address);
int amsdos_remove_file(amsdos_type *amsdos, const char *name, uint8_t user);
bool amsdos_exists_file(amsdos_type *amsdos, const char *name, uint8_t user);
amsdos_header_type *amsdos_get_file_header(amsdos_type *amsdos, uint8_t file_index);
#endif //AMSDOS_H
