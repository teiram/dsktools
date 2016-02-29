/* 
 * amsdos.c - AMSDOS file system Abstraction
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
#include <stdarg.h>
#include <stdbool.h>
#include <getopt.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "amsdos.h"
#include "log.h"
#include "error.h"

static uint8_t base_track_index(amsdos_type *amsdos) {
	dsk_info_type dsk_info;
	dsk_get_info(amsdos->dsk, &dsk_info);

	switch (dsk_info.first_sector_id) {
	case BASE_SECTOR_IBM:
		return RESERVED_SECTORS_IBM;
	case BASE_SECTOR_SYS:
		return RESERVED_SECTORS_SYS;
	default:
		return 0;
	}
}

static uint8_t base_sector(amsdos_disk_type type) {
	switch (type) {
	case DISK_TYPE_DATA:
		return BASE_SECTOR_DATA;
	case DISK_TYPE_IBM:
		return BASE_SECTOR_IBM;
	case DISK_TYPE_SYSTEM:
		return BASE_SECTOR_SYS;
	default:
		LOG(LOG_ERROR, "Unknown type of disk %d", type);
		return 0;
	}
}

static const char* get_basename(const char *name) {
	/* 
	 * Take only the last path segment. Only considering
	 * unix like paths so far
	 */
	LOG(LOG_TRACE, "get_basename(name=%s)", name);
	const char *result = name;
	const char *ptr = name;
	do {
		ptr = strstr(ptr, "/");
		if (ptr) {
			result = ++ptr;
		}
	} while (ptr);
	LOG(LOG_TRACE, "Basename calculated as: %s", result);
	return result;
}

static char *get_amsdos_filename(const char *name, char *buffer) {
	LOG(LOG_TRACE, "get_amsdos_filename(name=%s)", name);

	const char *name_ptr = get_basename(name);
	char *separator = strstr(name_ptr, ".");
	size_t len = separator ? MIN(separator - name_ptr, AMSDOS_NAME_LEN) :
		MIN(strlen(name_ptr), AMSDOS_NAME_LEN);

	strncpy(buffer, name_ptr, len);
	int i;
	for (i = 0; i < AMSDOS_NAME_LEN; i++) {
		buffer[i] = i < len ? toupper(buffer[i]) : ' ';
	}
	buffer[AMSDOS_NAME_LEN] = 0;
	LOG(LOG_TRACE, "Calculated amsdos name: %s", buffer);
	return buffer;
}

static char *get_amsdos_extension(const char *name, char *buffer) {
	LOG(LOG_TRACE, "get_amsdos_extension(name=%s)", name);
	const char* name_ptr = get_basename(name);
	char *separator = strstr(name_ptr, ".");
	int len = 0;
	if (separator) {
		len = strlen(separator + 1);
		strncpy(buffer, separator + 1, AMSDOS_EXT_LEN);
	}
	for (int i = 0; i < AMSDOS_EXT_LEN; i++) {
		buffer[i] = i < len ? toupper(buffer[i]) : ' ';
	}
	buffer[AMSDOS_EXT_LEN] = 0;
	LOG(LOG_TRACE, "Calculated amsdos extension: %s", buffer);
	return buffer;
}

static uint16_t get_amsdos_checksum(amsdos_header_type *header) {
	uint16_t checksum = 0;
	for (int i = 0; i < 67; i++) {
		checksum += *(((uint8_t*)header) + i);
	}
	return checksum;
}

static void set_amsdos_checksum(amsdos_header_type *header) {
	header->checksum = get_amsdos_checksum(header);
}

static amsdos_header_type *init_amsdos_header(amsdos_header_type *header,
					      const char *name, 
					      uint16_t size,
					      uint8_t user,
					      uint16_t load_address,
					      uint16_t entry_address) {
	memset(header, 0, sizeof(amsdos_header_type));
	/* Be aware that these functions set a NULL at end, 
	 * but they can be used here because of the order
	 * and because there are some unused bytes after the
	 * extension
	 */
	get_amsdos_filename(name, header->name);
	get_amsdos_extension(name, header->extension);

	header->data_length = 0;
	header->logical_length = header->file_length = size;
	header->type = AMSDOS_BINARY;
	header->user = user;
	header->load_address = load_address;
	header->entry_address = entry_address;
	set_amsdos_checksum(header);
	return header;
}

static bool is_amsdos_header(amsdos_header_type *header) {
	return header->checksum == get_amsdos_checksum(header);
}

char *amsdos_get_dir_basename(amsdos_dir_type *dir_entry, char *buffer) {
	memcpy(buffer, dir_entry->name, AMSDOS_NAME_LEN);
	buffer[AMSDOS_NAME_LEN] = 0;
	return buffer;
}

char *amsdos_get_dir_extension(amsdos_dir_type *dir_entry, 
			       char *buffer) {
	memcpy(buffer, dir_entry->extension, AMSDOS_EXT_LEN);
	buffer[AMSDOS_EXT_LEN] = 0;
	/* Strip attributes */
	buffer[0] &= 0x7F;
	buffer[1] &= 0x7F;
	return buffer;
}

static uint8_t get_free_dir_entry_count(amsdos_type *amsdos) {
	LOG(LOG_DEBUG, "get_free_dir_entry_count");
	uint8_t free_entries = 0;
	amsdos_dir_type dir_entry;
	for (int i = 0; i < AMSDOS_NUM_DIRENT; i++) {
		amsdos_get_dir(amsdos, &dir_entry, i);
		if (amsdos_is_dir_deleted(&dir_entry)) {
			++free_entries;
		}
	}
	LOG(LOG_TRACE, "Free dir entries: %d", free_entries);
	return free_entries;
}

static int8_t get_next_free_dir_entry(amsdos_type *amsdos) {
	LOG(LOG_DEBUG, "get_next_free_dir_entry");
	amsdos_dir_type dir_entry;
	for (int i = 0; i < AMSDOS_NUM_DIRENT; i++) {
		amsdos_get_dir(amsdos, &dir_entry, i);
		if (amsdos_is_dir_deleted(&dir_entry)) {
			return i;
		}
	}
	LOG(LOG_WARN, "Exhausted directory entries");
	return -1;
}

static amsdos_dir_type *init_dir_entry(amsdos_dir_type *dir_entry) {
	memset(dir_entry, 0, sizeof(amsdos_dir_type));
	return dir_entry;
}


static uint32_t get_dir_entry_offset(amsdos_type *amsdos, int index) {
	dsk_info_type dsk_info;
	dsk_get_info(amsdos->dsk, &dsk_info);

	uint8_t sector_id = dsk_info.first_sector_id + (index >> 4);
	uint8_t track = base_track_index(amsdos);
	return dsk_get_sector_offset(amsdos->dsk, track, 0, sector_id)
		+ ((index & 15) << 5);
}

static int8_t get_dir_entry_for_file(amsdos_type *amsdos, 
				     const char *name, 
				     uint8_t user,
				     amsdos_dir_type *entry) {
	char filename[AMSDOS_NAME_LEN + 1];
	char extension[AMSDOS_EXT_LEN + 1];
	char buffer[AMSDOS_NAME_LEN + 1];
	get_amsdos_filename(name, (char*) &filename);
	get_amsdos_extension(name, (char *)&extension);
	LOG(LOG_DEBUG,"get_dir_entry_for_file(amsdos(name=%s.%s, user %u))", filename, extension, user);
	for (int i = 0; i < AMSDOS_NUM_DIRENT; i++) {
		amsdos_get_dir(amsdos, entry, i);
		if (strncmp(filename, amsdos_get_dir_basename(entry, buffer), 
			    AMSDOS_NAME_LEN) == 0 &&
		    strncmp(extension, amsdos_get_dir_extension(entry, buffer),
			    AMSDOS_EXT_LEN) == 0 &&
		    entry->user == user) {
			LOG(LOG_DEBUG, "Found matching directory entry %u", i);
			return i;
		}
	}
	LOG(LOG_DEBUG, "No matching entry for file found");
	return -1;
}

static void read_entry_sector(amsdos_type *amsdos, FILE *fd, uint8_t sector,
			      uint8_t remaining_records) {
	uint8_t buffer[AMSDOS_SECTOR_SIZE];

	dsk_read_sector(amsdos->dsk, buffer, sector);
	fwrite(buffer, MIN(AMSDOS_SECTOR_SIZE, remaining_records << 7), 1, fd);
}
	
static void read_entry_blocks(amsdos_type *amsdos, FILE *fd, 
			      amsdos_dir_type *dir_entry) {
	char name[16];
	LOG(LOG_DEBUG, 
	    "read_entry_blocks(entry=%s, extent_low=%u, records=%u)",
	    amsdos_get_dir_name(dir_entry, name),
	    dir_entry->extent_low,
	    dir_entry->record_count);
	    
	uint8_t block_count = SHIFTH(dir_entry->record_count, 3);
	LOG(LOG_TRACE, "Reading %d blocks for %d sectors", block_count,
	    dir_entry->record_count);
	int32_t remaining_records = dir_entry->record_count;
	for (int i = 0; i < block_count; i++) {
		if (dir_entry->blocks[i] > 0 && remaining_records > 0) {
			uint8_t sector = dir_entry->blocks[i] << 1;
			read_entry_sector(amsdos, fd, sector, 
					  remaining_records);
			remaining_records -= 4;
			read_entry_sector(amsdos, fd, sector + 1,
					  remaining_records);
			remaining_records -= 4;
		}
	}
}

static bool is_block_in_use(amsdos_type *amsdos, uint8_t block) {
	/* To be optimized */
	amsdos_dir_type dir_entry;
	for (int i = 0; i < AMSDOS_NUM_DIRENT; i++) {
		amsdos_get_dir(amsdos, &dir_entry, i);
		if (!amsdos_is_dir_deleted(&dir_entry)) {
			int block_count = (dir_entry.record_count + 7) >> 3;
			for (int j = 0; j < block_count; j++) {
				if (dir_entry.blocks[j] == block) {
					LOG(LOG_TRACE, 
					    "Block %02x in use by entry %u", 
					    block, i);
					return true;
				}
			}
		}
	}
	return false;
}

static uint32_t get_block_count(amsdos_type *amsdos) {
	dsk_info_type dsk_info;
	dsk_get_info(amsdos->dsk, &dsk_info);
	return dsk_info.sectors >> 1;
}

static int8_t get_free_block(amsdos_type *amsdos) {
	if (amsdos->last_free_block == 0) {
		uint8_t base_block = 0;
		for (int i = 0; i < base_track_index(amsdos); i++) {
			base_block += 
				SHIFTH(dsk_get_track_size(amsdos->dsk, i)
				       - sizeof(track_header_type), 10);
		}
		/* Skip directory */
		base_block += SHIFTH(AMSDOS_NUM_DIRENT * 
				     sizeof(amsdos_dir_type), 10);
		amsdos->last_free_block = base_block;
	}
	LOG(LOG_TRACE, "Base search block is %u", amsdos->last_free_block);
	uint8_t block = amsdos->last_free_block;
	while (block < get_block_count(amsdos)) {
		if (!is_block_in_use(amsdos, block)) {
			LOG(LOG_TRACE, "Found free block %02x\n", block);
			/* A bit hacky, but we need to start searching
			 * in the next block to avoid reusing the same
			 * blocks, since the directory entry is only 
			 * updated at the end
			 */
			amsdos->last_free_block = block + 1;
			return block;
		}
		++block;
	}
	return -1;
}

static void read_sector(amsdos_type *amsdos, FILE *fd, 
			amsdos_header_type *header, uint8_t sector) {
	uint8_t buffer[AMSDOS_SECTOR_SIZE];
	if (header) {
		uint32_t header_size = sizeof(amsdos_header_type);
		memcpy(buffer, header, header_size);
		fread(buffer + header_size, AMSDOS_SECTOR_SIZE - header_size, 
		      1, fd);
		dsk_write_sector(amsdos->dsk, buffer, sector);
	} else {
		fread(buffer, AMSDOS_SECTOR_SIZE, 1, fd);
		dsk_write_sector(amsdos->dsk, buffer, sector);
	}
}

static off_t get_file_size(const char *filename) {
	struct stat buf;
	if (stat(filename, &buf)) {
		LOG(LOG_ERROR, "Unable to stat file %s", filename);
		return DSK_ERROR;
	} else {
		if (S_ISREG(buf.st_mode)) {
			return buf.st_size;
		} else {
			LOG(LOG_ERROR, "Not a regular file %s", filename);
			return DSK_ERROR;
		}
	}
}

static off_t add_file_checks(amsdos_type *amsdos, const char *source_file,
			     const char *target_name, uint8_t user) {
	LOG(LOG_DEBUG, "add_file_checks(source=%s, target=%s, user=%u)",
	    source_file, target_name, user);
	if (amsdos_exists_file(amsdos, target_name, user)) {
		error_add_error("File %s(%d) already exists in dsk",
			  target_name, user);
		return DSK_ERROR;
	}

	off_t size = get_file_size(source_file);
	LOG(LOG_TRACE, "File size is %d", size);
	
	if (size < 0) {
		LOG(LOG_ERROR, "Unable to determine file size %s", 
		    source_file);
		error_add_error("Unable to determine file size for %s",
			  source_file);
		return DSK_ERROR;
	}

	uint16_t free_dir_entries = get_free_dir_entry_count(amsdos);
	if (size > (free_dir_entries * AMSDOS_BLOCKS_DIRENT * 1024)) {
		error_add_error("Not enough free directory entries %d", 
			  free_dir_entries);
		return DSK_ERROR;
	}
	return size;
}	

static int add_file_internal(amsdos_type *amsdos, amsdos_header_type *header,
			     off_t size, FILE *stream, 
			     const char *name, uint8_t user) {
	LOG(LOG_DEBUG, "add_file_internal(size=%d, name=%s, user=%u)",
	    size, name, user);
	uint8_t extent = 0;
	for (off_t pos = 0; pos < size;) {
		int8_t dir_entry_index = get_next_free_dir_entry(amsdos);
		if (dir_entry_index < 0) {
			error_add_error("Exhausted directory entries");
			return DSK_ERROR;
		}
		amsdos_dir_type dir_entry;
		init_dir_entry(&dir_entry);
		dir_entry.user = user;
		dir_entry.extent_low = extent++;
		char buffer[AMSDOS_NAME_LEN + 1];
		get_amsdos_filename(name, buffer);
		memcpy(dir_entry.name, buffer, AMSDOS_NAME_LEN);
		get_amsdos_extension(name, buffer);
		memcpy(dir_entry.extension, buffer, AMSDOS_EXT_LEN);
		/* Records are of 128bytes 
		   Blocks are of 1 Kbyte (2 sectors)
		   The maximum blocks allocated per file are 16, what equals to
		   16 block * (1 kbyte/block) * (8 records/kbyte) = 128 records
		*/
		uint8_t records = MIN(AMSDOS_RECORDS_DIRENT, 
				      SHIFTH(size - pos, 7));
		LOG(LOG_TRACE, "Records to write %d", records);
		dir_entry.record_count = records;
		/* Blocks are of 1Kbyte */
		uint8_t blocks = SHIFTH(records, 3);
		for (int i = 0; i < blocks; i++) {
			int8_t block = get_free_block(amsdos);
			if (block >= 0) {
				dir_entry.blocks[i] = (uint8_t) block;
				uint8_t sector = block << 1;
				read_sector(amsdos, stream, header, sector);
				/* header is added only once */
				header = NULL;
				read_sector(amsdos, stream, NULL, sector + 1);
				pos += AMSDOS_SECTOR_SIZE << 1;
			} else {
				error_add_error("Free blocks exhausted");
				return DSK_ERROR;
			}
		}
		amsdos_update_dir(amsdos, &dir_entry, dir_entry_index);
	}
	return DSK_OK;	
}

bool amsdos_is_dir_deleted(amsdos_dir_type *dir_entry) {
	return dir_entry->user == AMSDOS_USER_DELETED;
}

char *amsdos_get_dir_name(amsdos_dir_type *dir_entry, char *buffer) {
	amsdos_get_dir_basename(dir_entry, buffer);
	buffer[AMSDOS_NAME_LEN] = '.';
	amsdos_get_dir_extension(dir_entry, buffer + AMSDOS_NAME_LEN + 1);
	return buffer;
}

uint32_t amsdos_get_dir_size(amsdos_dir_type* dir_entries, int index) {
	int blocks = 0;
	int file_user = dir_entries[index].user;
	int lookahead = index;
	do {
		if (dir_entries[lookahead].user == file_user) {
			blocks += dir_entries[lookahead].record_count;
		}
		lookahead++;
	} while (dir_entries[lookahead].extent_low 
		 && lookahead < AMSDOS_NUM_DIRENT);
	return blocks << 7;
}

amsdos_type *amsdos_new(const char *filename) {
	dsk_type *dsk = dsk_new(filename);
	if (dsk) {
		amsdos_type *amsdos = (amsdos_type*) calloc(1, sizeof(amsdos_type));
		amsdos->dsk = dsk;
		return amsdos;
	} else {
		LOG(LOG_ERROR, "Initializing DSK");
		return NULL;
	}
}

amsdos_type *amsdos_new_empty(uint8_t tracks, uint8_t sides, 
			      uint8_t sectors_per_track,
			      uint8_t sector_size,
			      uint16_t tracklen, 
			      amsdos_disk_type type) {
	dsk_type *dsk = dsk_new_empty(DSK, tracks, sides, tracklen);
	amsdos_type *amsdos = (amsdos_type*) calloc(1, sizeof(amsdos_type));
	amsdos->dsk = dsk;

	for (int i = 0; i < tracks; i++) {
		for (int j = 0; j < sides; j++) {
			track_header_type *track = 
				dsk_get_track_info(dsk, (i << (sides - 1)) + j, false);
			dsk_init_track_info(track, i, j, sector_size, 
					    sectors_per_track, 82);
			uint8_t first_sector = base_sector(type);
			for (int k = 0; k < sectors_per_track; k++) {
				track->sector_info[k].track = i;
				track->sector_info[k].side = j;
				track->sector_info[k].sector_id = 
					first_sector + k;
				track->sector_info[k].size = sector_size;
			}
		}
	}
	amsdos_dir_type dir_entry;
	for (int i = 0; i < AMSDOS_NUM_DIRENT; i++) {
		amsdos_get_dir(amsdos, &dir_entry, i);
		memset(&dir_entry, 0xe5, sizeof(amsdos_dir_type));
		dir_entry.user = AMSDOS_USER_DELETED;
		amsdos_update_dir(amsdos, &dir_entry, i);
	}
	return amsdos;
}

void amsdos_delete(amsdos_type *amsdos) {
	if (amsdos) {
		if (amsdos->dsk) {
			dsk_delete(amsdos->dsk);
		}
		free(amsdos);
	}
}

uint32_t amsdos_get_used_bytes(amsdos_type *amsdos) {
	int i;
	amsdos_dir_type dir_entry;
	uint32_t bytes = 0;
	for (i = 0; i < AMSDOS_NUM_DIRENT; i++) {
		amsdos_get_dir(amsdos, &dir_entry, i);
		if (!amsdos_is_dir_deleted(&dir_entry)) {
			bytes += (dir_entry.record_count << 7);
		}
	}
	LOG(LOG_TRACE, "Used bytes in disk %u", bytes);
	return bytes;
}

amsdos_info_type *amsdos_get_info(amsdos_type *amsdos, 
				  amsdos_info_type *info) {
	dsk_get_info(amsdos->dsk, &(info->dsk_info));
	info->used = amsdos_get_used_bytes(amsdos);
	switch (info->dsk_info.first_sector_id) {
	case BASE_SECTOR_DATA:
		info->type = DISK_TYPE_DATA;
		break;
	case BASE_SECTOR_IBM:
		info->type = DISK_TYPE_IBM;
		break;
	case BASE_SECTOR_SYS:
		info->type = DISK_TYPE_SYSTEM;
		break;
	default:
		info->type = DISK_TYPE_UNKNOWN;
	}
	return info;
}

amsdos_dir_type *amsdos_get_dir(amsdos_type *amsdos, 
				amsdos_dir_type *dir_entry, 
				int index) {
	LOG(LOG_DEBUG, "amsdos_get_dir(index=%d)", index);
	if (index < AMSDOS_NUM_DIRENT) {
		uint32_t offset = get_dir_entry_offset(amsdos, index);
		LOG(LOG_TRACE, "Copying from image offset %08x", offset);
		memcpy(dir_entry, amsdos->dsk->image + offset, 
		       sizeof(amsdos_dir_type));
		return dir_entry;
	} else {
		return 0;
	}
}

void amsdos_update_dir(amsdos_type *amsdos,
		       amsdos_dir_type *dir_entry,
		       int index) {
	LOG(LOG_DEBUG, "amsdos_update_dir(index=%d, user=%u)", index,
	    dir_entry->user);
	if (index < AMSDOS_NUM_DIRENT) {
		uint32_t offset = get_dir_entry_offset(amsdos, index);
		LOG(LOG_TRACE, "Copying to image offset %08x", offset);
		memcpy(amsdos->dsk->image + offset, dir_entry, 
		       sizeof(amsdos_dir_type));
	} else {
		LOG(LOG_WARN, "Directory entry index out of bounds");
	}
}


int amsdos_get_file(amsdos_type *amsdos, 
		    const char *name, 
		    uint8_t user,
		    const char *destination) {
	LOG(LOG_DEBUG, "amsdos_get_file(name=%s, destination=%s, user=%u)",
	    name, destination, user);
	amsdos_dir_type dir_entry, *dir_entry_p;

	int index = get_dir_entry_for_file(amsdos, name, user, &dir_entry);
	if (index >= 0) {
		FILE *fd = fopen(destination, "w");
		if (fd) {
			do {
				read_entry_blocks(amsdos, fd, &dir_entry);
				dir_entry_p = amsdos_get_dir(amsdos, 
							     &dir_entry, 
							     ++index);
			} while (dir_entry_p && 
				 dir_entry_p->extent_low && 
				 dir_entry_p->user == user);
			fclose(fd);
			return DSK_OK;
		} else {
			error_add_error("Unable to write to file %s: %s", 
				  destination,
				  strerror(errno));
			return DSK_ERROR;
		}
	} else {
		error_add_error("Unable to find file %s\n", name);
		return DSK_ERROR;
	}
}

int amsdos_remove_file(amsdos_type *amsdos,
		       const char *name,
		       uint8_t user) {
        LOG(LOG_DEBUG, "amsdos_remove_file(name=%s, user=%u)",
            name, user);
        amsdos_dir_type dir_entry, *dir_entry_p;

        int index = get_dir_entry_for_file(amsdos, name, user, &dir_entry);
        if (index >= 0) {
                do {
                        dir_entry.user = AMSDOS_USER_DELETED;
			amsdos_update_dir(amsdos, &dir_entry, index);
			dir_entry_p = amsdos_get_dir(amsdos, 
						     &dir_entry, 
						     ++index);
                } while (dir_entry_p &&
                         dir_entry_p->extent_low &&
                         dir_entry_p->user == user);
		return DSK_OK;
        } else {
                error_add_error("Unable to find file %s\n", name);
                return DSK_ERROR;
        }
}


bool amsdos_exists_file(amsdos_type *amsdos, const char *name, uint8_t user) {
	amsdos_dir_type dir_entry;
	return get_dir_entry_for_file(amsdos, name, user, &dir_entry) >= 0;
}


int amsdos_add_file(amsdos_type *amsdos, const char *source_file, 
		    const char *target_name,
		    uint8_t user) {

	LOG(LOG_DEBUG, "amsdos_add_file(source=%s, target=%s, user=%u)",
	    source_file, target_name, user);

	off_t size;
	if ((size = add_file_checks(amsdos, source_file, 
				    target_name, user)) < DSK_OK) {
		LOG(LOG_ERROR, "In file checks %s", error_get_error_message());
		return DSK_ERROR;
	}

	FILE *stream = fopen(source_file, "r");
	int retcode = add_file_internal(amsdos, NULL, size, stream, 
					target_name, user);
	fclose(stream);
	return retcode;
}

int amsdos_add_binary_file(amsdos_type *amsdos, const char *source_file,
			   const char *target_name, uint8_t user, 
			   uint16_t load_address, uint16_t entry_address) {

	LOG(LOG_DEBUG, "amsdos_add_binary_file(source=%s, target=%s, user=%u, load_address=0x%04x, entry_address=0x%04x)",
	    source_file, target_name, user,
	    load_address, entry_address);
	
	off_t size;
	if ((size = add_file_checks(amsdos, source_file, 
				    target_name, user)) < DSK_OK) {
		LOG(LOG_ERROR, "In file checks %s",error_get_error_message());
		return DSK_ERROR;
	}

	/* Check if we have an amsdos header and modify it according
	   to the provided addresses when they are defined
	   Otherwise, provide a new header
	*/
	FILE *stream = fopen(source_file, "r");
	amsdos_header_type header;
	size_t nread = fread(&header, sizeof(amsdos_header_type), 1, stream);
	if (nread < 1) {
		error_add_error("Unable to read header from file %s. %s",
			  source_file, strerror(errno));
		fclose(stream);
		return DSK_ERROR;
	}
	if (!is_amsdos_header(&header)) {
		LOG(LOG_DEBUG, "No AMSDOS header found. Creating a new one");
		size += sizeof(amsdos_header_type);
		rewind(stream);
	}
	init_amsdos_header(&header, target_name, size,
			   user, load_address, 
			   entry_address);
	int retcode = add_file_internal(amsdos, &header, size, stream, 
					target_name, user);
	fclose(stream);
	return retcode;
}

int amsdos_add_ascii_file(amsdos_type *amsdos, const char *source_file, 
			  const char *target_name, uint8_t user) {

	LOG(LOG_DEBUG, "amsdos_add_ascii_file(source=%s, target=%s, user=%u)",
	    source_file, target_name, user);

	off_t size;
	if ((size = add_file_checks(amsdos, source_file, 
				    target_name, user)) < DSK_OK) {
		LOG(LOG_ERROR, "In file checks %s", error_get_error_message());
		return DSK_ERROR;
	}

	/*
	 * Strip the AMSDOS header if it exists
	 */
	FILE *stream = fopen(source_file, "r");
	amsdos_header_type header;
	size_t nread = fread(&header, sizeof(amsdos_header_type), 1, stream);
	if (nread < 1) {
		error_add_error("Unable to read header from file %s. %s",
			  source_file, strerror(errno));
		fclose(stream);
		return DSK_ERROR;
	}
	if (!is_amsdos_header(&header)) {
		LOG(LOG_DEBUG, "No AMSDOS header found. Nothing stripped");
		rewind(stream);
	} else {
		LOG(LOG_WARN, "Stripping AMSDOS header from file");
		size -= sizeof(amsdos_header_type);
	}

	int retcode = add_file_internal(amsdos, &header, size, stream, 
					target_name, user);
	fclose(stream);
	return retcode;
}
