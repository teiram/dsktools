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

#include "dsk.h"
#include "log.h"

static void dsk_set_error(dsk_type *dsk, const char *fmt, ...) {
	if (dsk) {
		if (strlen(dsk->error)) {
			LOG(LOG_INFO, "Overwriting existing error");
		}
		va_list ap;
		va_start(ap, fmt);
		vsnprintf(dsk->error, DSK_ERROR_SIZE, fmt, ap);
	} else {
		LOG(LOG_ERROR, "Attempt to set error on uninitialized DSK");
	}
}

static void dsk_reset_error(dsk_type *dsk) {
	dsk->error[0] = 0;
}

char *dsk_get_error(dsk_type *dsk) {
	if (dsk) {
		return dsk->error;
	} else {
		LOG(LOG_WARN, "Trying to get error from an unitialized DSK");
		return NULL;
	}
}

static bool is_dsk_image(dsk_type *dsk) {
	if (dsk && dsk->dsk_info) {
		return !strncmp(dsk->dsk_info->magic, 
				DSK_HEADER, 8) ? true: false;
	} else {
		char header[35];
		snprintf(header, 34, "%s", dsk->dsk_info->magic);
		LOG(LOG_DEBUG, "Image header doesn't match the expected DSK header. Expected: %s, header: %s", DSK_HEADER, header);
		return false;
	}
}

static bool is_edsk_image(dsk_type *dsk) {
	if (dsk && dsk->dsk_info) {
		return !strncmp(dsk->dsk_info->magic,
				EDSK_HEADER, 21) ? true : false;
	} else {
		char header[35];
		snprintf(header, 34, "%s", dsk->dsk_info->magic);
		LOG(LOG_DEBUG, "Image header doesn't match the expected EDSK header. Expected %s, header %s", EDSK_HEADER, header);
		return false;
	}
}

static uint32_t get_track_size(dsk_type *dsk, uint8_t track) {
	if (is_dsk_image(dsk)) {
		return dsk->dsk_info->track_size[0] +
			(dsk->dsk_info->track_size[1] << 8);
	} else if (is_edsk_image(dsk)) {
		return dsk->dsk_info->track_size_high[track] << 8;
	} else {
		LOG(LOG_ERROR, "Unsupported image type");
		return 0;
	}
}


static uint32_t get_image_size(dsk_type *dsk) {
	if (is_dsk_image(dsk)) {
		return dsk->dsk_info->tracks * 
			get_track_size(dsk, 0);
	} else if (is_edsk_image(dsk)) {
		uint32_t size = 0;
		uint32_t track_count = dsk->dsk_info->tracks * 
			dsk->dsk_info->sides;
		for (int i = 0; i < track_count; i++) {
			size += get_track_size(dsk, i);
		}
		return size;
	} else {
		dsk_set_error(dsk, "Getting image size from unsupported image");
		return 0;
	}
}

static uint32_t get_track_offset(dsk_type *dsk, uint8_t track) {
	if (is_dsk_image(dsk)) {
		return get_track_size(dsk, 0) * track;
	} else if (is_edsk_image(dsk)) {
		uint32_t offset = 0;
		for (int i = 0; i < track; i++) {
			offset += get_track_size(dsk, i);
		}
		return offset;
	} else {
		dsk_set_error(dsk, "Getting track offset from unsupported image");
		return 0;
	}
}

static uint8_t sectors_per_track(dsk_type *dsk) {
	/* Assumes that all tracks have the same number of sectors*/
	
	track_info_type *track0 =
		dsk_get_track_info(dsk, 0);
	return track0->sector_count;
}

static uint8_t first_sector_id(dsk_type *dsk) {
	track_info_type *track0 = 
		dsk_get_track_info(dsk, 0);
	uint8_t min = ~0;
	for (int i = 0; i < track0->sector_count; i++) {
		if (track0->sector_info[i].sector_id < min) {
			min = track0->sector_info[i].sector_id;
		}
	}
	LOG(LOG_TRACE, "First sector id is %02x", min);
	return min;
}

static uint8_t base_track_index(dsk_type *dsk) {
	int min_sector_id = first_sector_id(dsk);
	switch (min_sector_id) {
	case BASE_SECTOR_IBM:
		return 1;
	case BASE_SECTOR_SYS:
		return 2;
	default:
		return 0;
	}
}	

static uint32_t get_track_usable_blocks(dsk_type *dsk, uint8_t track) {
	if (track < base_track_index(dsk)) {
		return 0;
	} else {
		return get_track_size(dsk, track) - sizeof(track_info_type);
	}
}


static uint32_t get_sector_offset_in_track(track_info_type *track, uint8_t sector_id) {
	uint32_t offset = 0;
	int i;
	LOG(LOG_DEBUG, "get_sector_offset_in_track(track=%u, sector=%02x)",
	    sector_id, track->track_number);
	for (i = 0; i < track->sector_count; i++) {
		sector_info_type *sinfo = &track->sector_info[i];
		LOG(LOG_TRACE, "Adding offset for sector %02x. Current %04x", 
		    sinfo->sector_id, offset);
		if (sinfo->sector_id == sector_id) {
			offset += sizeof(track_info_type);
			LOG(LOG_TRACE, " result: %04x", offset);
			return offset;
		} else {
			offset += 128 << sinfo->size;
		}
	}
	LOG(LOG_WARN, "Sector %02x not found in track %u", sector_id,
	    track->track_number);
	return 0;
}


static uint32_t get_block_offset(dsk_type *dsk,
				 uint8_t sector) {
	LOG(LOG_DEBUG, "get_block_offset(sector=%u)", sector);
	uint8_t track_index = base_track_index(dsk);
	uint8_t sectors_in_track = sectors_per_track(dsk);
	track_index += sector / sectors_in_track;
	uint8_t sector_id = first_sector_id(dsk);
	sector_id += sector % sectors_in_track;

	LOG(LOG_TRACE, "Calculated track_index %d, sector_id %02x",
	    track_index, sector_id);
	uint32_t offset = 0;
	for (int i = 0; i < track_index; i++) {
		offset += get_track_size(dsk, i);
	}
	LOG(LOG_TRACE, "Track offset %u", offset);
	track_info_type *track = dsk_get_track_info(dsk, track_index);
	if (track) {
		offset += get_sector_offset_in_track(track, sector_id);
	} else {
		LOG(LOG_INFO, "Unable to calculated offset for track %d",
		    track_index);
		offset = 0;
	}
	return offset;
}

static uint32_t get_sector_offset(dsk_type *dsk, 
				  uint8_t track_id, uint8_t side, 
				  uint8_t sector_id) {
	LOG(LOG_DEBUG, "get_sector_offset(track=%u, side=%u, sector=%02x)",
	    track_id, side, sector_id);
	uint32_t offset = 0;
	for (int i = 0; i < dsk->dsk_info->tracks; i++) {
		track_info_type *track = dsk_get_track_info(dsk, i);
		LOG(LOG_TRACE, "Searching in track %u", track->track_number);
		if (track->track_number == track_id &&
		    track->side_number == side) {
			offset += get_sector_offset_in_track(track, sector_id);
			break;
		} else {
			offset += get_track_size(dsk, i);
		}
		LOG(LOG_TRACE, " +current offset %04x", offset);
	}
	LOG(LOG_DEBUG, " +calculated offset. %04x", offset);
	return offset;
}

static bool is_track_in_offset(dsk_type *dsk, uint32_t offset) {
	return !strncmp((char*) dsk->image + offset, DSK_TRACK_HEADER, 13);
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
					      uint16_t size) {
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
	set_amsdos_checksum(header);
	return header;
}

static bool has_amsdos_header(uint8_t *stream) {
	amsdos_header_type *header = (amsdos_header_type *) stream;
	return header->checksum == get_amsdos_checksum(header);
}

static uint8_t get_free_dir_entry_count(dsk_type *dsk) {
	uint8_t free_entries = 0;
	dir_entry_type dir_entry;
	for (int i = 0; i < NUM_DIRENT; i++) {
		dsk_get_dir_entry(dsk, &dir_entry, i);
		if (is_dir_entry_deleted(&dir_entry)) {
			++free_entries;
		}
	}
	return free_entries;
}

static int8_t get_next_free_dir_entry(dsk_type *dsk) {
	dir_entry_type dir_entry;
	for (int i = 0; i < NUM_DIRENT; i++) {
		dsk_get_dir_entry(dsk, &dir_entry, i);
		if (is_dir_entry_deleted(&dir_entry)) {
			return i;
		}
	}
	LOG(LOG_WARN, "Exhausted directory entries");
	return -1;
}

static dir_entry_type *init_dir_entry(dir_entry_type *dir_entry) {
	memset(dir_entry, 0, sizeof(dir_entry_type));
	return dir_entry;
}

bool is_dir_entry_deleted(dir_entry_type *dir_entry) {
	return dir_entry->user == AMSDOS_USER_DELETED;
}

char *dir_entry_get_basename(dir_entry_type *dir_entry, char *buffer) {
	memcpy(buffer, dir_entry->name, AMSDOS_NAME_LEN);
	buffer[AMSDOS_NAME_LEN] = 0;
	return buffer;
}

char *dir_entry_get_extension(dir_entry_type *dir_entry, char *buffer) {
	memcpy(buffer, dir_entry->extension, AMSDOS_EXT_LEN);
	buffer[AMSDOS_EXT_LEN] = 0;
	/* Strip attributes */
	buffer[0] &= 0x7F;
	buffer[1] &= 0x7F;
	return buffer;
}

char *dir_entry_get_name(dir_entry_type *dir_entry, char *buffer) {
	dir_entry_get_basename(dir_entry, buffer);
	buffer[AMSDOS_NAME_LEN] = '.';
	dir_entry_get_extension(dir_entry, buffer + AMSDOS_NAME_LEN + 1);
	return buffer;
}

/* Probably broken for EDSK */
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
		if (track < dsk->dsk_info->tracks) {
			if (!dsk->track_info[track]) {
				uint32_t offset = get_track_offset(dsk, track);
				if (is_track_in_offset(dsk, offset)) {
					dsk->track_info[track] = (track_info_type*) (dsk->image + offset);
				} else {
					LOG(LOG_ERROR, 
					    "Invalid track offset %04x", 
					    offset);
					dsk_set_error(dsk, 
						      "Invalid track offset %04x", 
						      offset);
					return NULL;
				}
			}
			return dsk->track_info[track];
		} else {
			LOG(LOG_ERROR, "No such track in disk %u", track);
		}
	} 
	return NULL;
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
		if (dsk->error) {
			free(dsk->error);
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
		if (nread == 1) {
			uint32_t disk_size = get_image_size(dsk);
			dsk->image = (uint8_t*) malloc(disk_size);
			nread = fread(dsk->image,
				      disk_size, 1, fh);
			fclose(fh);
			if (nread == 1) {
				dsk->track_info = (track_info_type**) 
					calloc(dsk->dsk_info->tracks,
					       sizeof(track_info_type**));
				dsk->error = (char*) calloc(1, DSK_ERROR_SIZE);
				return dsk;
			}
		} else {
			LOG(LOG_ERROR, "Unable to read %d, was %d\n",
			    sizeof(dsk_info_type), nread);
		}
	}
	if (fh != 0) {
		fclose(fh);
	}
	dsk_delete(dsk);
	return 0;
}
	

static uint32_t get_dir_entry_offset(dsk_type *dsk, int index) {
	uint8_t sector_id = first_sector_id(dsk) + (index >> 4);
	uint8_t track = base_track_index(dsk);
	return get_sector_offset(dsk, track, 0, sector_id)
		+ ((index & 15) << 5);
}

dir_entry_type *dsk_get_dir_entry(dsk_type *dsk, 
				  dir_entry_type *dir_entry, 
				  int index) {
	LOG(LOG_DEBUG, "dsk_get_dir_entry(index=%d)", index);
	if (index < NUM_DIRENT) {
		uint32_t offset = get_dir_entry_offset(dsk, index);
		LOG(LOG_TRACE, "Copying from image offset %08x", offset);
		memcpy(dir_entry, dsk->image + offset, sizeof(dir_entry_type));
		return dir_entry;
	} else {
		return 0;
	}
}

void dsk_set_dir_entry(dsk_type *dsk,
		       dir_entry_type *dir_entry,
		       int index) {
	LOG(LOG_DEBUG, "dsk_set_dir_entry(index=%d, user=%u)", index,
	    dir_entry->user);
	if (index < NUM_DIRENT) {
		uint32_t offset = get_dir_entry_offset(dsk, index);
		LOG(LOG_TRACE, "Copying to image offset %08x", offset);
		memcpy(dsk->image + offset, dir_entry, sizeof(dir_entry_type));
	} else {
		LOG(LOG_WARN, "Directory entry index out of bounds");
	}
}

uint32_t dsk_get_total_blocks(dsk_type *dsk) {
	uint32_t blocks = 0;
	for (int i = 0; i < dsk->dsk_info->tracks; i++) {
		track_info_type *track = 
			dsk_get_track_info(dsk, i);
		blocks += track->sector_count * track->sector_size;
	}
	blocks <<= 7; /* Each block has 128bytes */
	LOG(LOG_TRACE, "Total blocks in disk %u", blocks);
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
	blocks <<= 7;
	LOG(LOG_TRACE, "Used blocks in disk %u", blocks);
	return blocks;
}

int8_t get_dir_entry_for_file(dsk_type *dsk, 
			      const char *name, 
			      uint8_t user,
			      dir_entry_type *entry) {
	char filename[AMSDOS_NAME_LEN + 1];
	char extension[AMSDOS_EXT_LEN + 1];
	char buffer[AMSDOS_NAME_LEN + 1];
	get_amsdos_filename(name, (char*) &filename);
	get_amsdos_extension(name, (char *)&extension);
	LOG(LOG_DEBUG,"get_dir_entry_for_file(amsdos(name=%s.%s, user %u))", filename, extension, user);
	int i = 0;
	for (i = 0; i < NUM_DIRENT; i++) {
		dsk_get_dir_entry(dsk, entry, i);
		if (strncmp(filename, dir_entry_get_basename(entry, buffer), 
			    AMSDOS_NAME_LEN) == 0 &&
		    strncmp(extension, dir_entry_get_extension(entry, buffer),
			    AMSDOS_EXT_LEN) == 0 &&
		    entry->user == user) {
			LOG(LOG_DEBUG, "Found matching directory entry %u", i);
			return i;
		}
	}
	LOG(LOG_DEBUG, "No matching entry for file found");
	return -1;
}

static void write_entry_sector(dsk_type *dsk, FILE *fd, uint8_t sector) {
	uint32_t offset = get_block_offset(dsk, sector);
	if (offset > 0) {
		LOG(LOG_DEBUG, "Writing block of size %d, offset %04x", SECTOR_SIZE, offset);
		uint8_t *src = dsk->image + offset; 
		fwrite(src, SECTOR_SIZE, 1, fd);
	} else {
		LOG(LOG_WARN, "Skipping sector %d", sector);
	}
}
	
void write_entry_blocks(dsk_type *dsk, FILE *fd, dir_entry_type *dir_entry) {
	int i;
	char name[16];
	LOG(LOG_DEBUG, "write_entry_blocks(entry=%s, extent_low=%u, records=%u)",
	    dir_entry_get_name(dir_entry, name),
	    dir_entry->extent_low,
	    dir_entry->record_count);
	    
	int block_count = (dir_entry->record_count + 7) >> 3;
	LOG(LOG_TRACE, "Block count is %d", block_count);
	for (i = 0; i < block_count; i++) {
		if (dir_entry->blocks[i] > 0) {
			uint8_t sector = dir_entry->blocks[i] << 1;
			write_entry_sector(dsk, fd, sector);
			write_entry_sector(dsk, fd, sector + 1);
		}
	}
}

int dsk_get_file(dsk_type *dsk, 
		  const char *name, 
		  const char *destination,
		  uint8_t user) {
	LOG(LOG_DEBUG, "dsk_get_file(name=%s, destination=%s, user=%u)",
	    name, destination, user);
	dsk_reset_error(dsk);
	dir_entry_type dir_entry, *dir_entry_p;

	int index = get_dir_entry_for_file(dsk, name, user, &dir_entry);
	if (index >= 0) {
		FILE *fd = fopen(destination, "w");
		if (fd) {
			do {
				write_entry_blocks(dsk, fd, &dir_entry);
				dir_entry_p = dsk_get_dir_entry(dsk, &dir_entry, 
							      ++index);
			} while (dir_entry_p && 
				 dir_entry_p->extent_low && 
				 dir_entry_p->user == user);
			fclose(fd);
			return DSK_OK;
		} else {
			dsk_set_error(dsk, "Unable to write to file %s: %s", 
				      destination,
				      strerror(errno));
			return DSK_ERROR;
		}
	} else {
		dsk_set_error(dsk, "Unable to find file %s\n", name);
		return DSK_ERROR;
	}
}

int dsk_dump_image(dsk_type *dsk, const char *destination) {
        FILE *fd = fopen(destination, "w");
        if (fd != NULL) {
                if (fwrite(dsk->dsk_info,
                           sizeof(dsk_info_type), 1, fd) < 1) {
                        dsk_set_error(dsk, "Writing image to %s. %s",
                                      destination,
                                      strerror(errno));
                        fclose(fd);
                        return DSK_ERROR;
                }
                if (fwrite(dsk->image,
                           get_image_size(dsk), 1, fd) < 1) {
                        dsk_set_error(dsk, "Writing image to %s. %s",
                                      destination,
                                      strerror(errno));
                        fclose(fd);
                        return DSK_ERROR;
                }
                fclose(fd);
                return DSK_OK;
        } else {
                dsk_set_error(dsk, "Opening destination file %s. %s",
                              destination,
                              strerror(errno));
                return DSK_ERROR;
        }
}

int dsk_remove_file(dsk_type *dsk,
                    const char *name,
                    uint8_t user) {
        LOG(LOG_DEBUG, "dsk_remove_file(name=%s, user=%u)",
            name, user);
        dsk_reset_error(dsk);

        dir_entry_type dir_entry, *dir_entry_p;

        int index = get_dir_entry_for_file(dsk, name, user, &dir_entry);
        if (index >= 0) {
                do {
                        dir_entry.user = AMSDOS_USER_DELETED;
			dsk_set_dir_entry(dsk, &dir_entry, index);
			dir_entry_p = dsk_get_dir_entry(dsk, &dir_entry, ++index);
                } while (dir_entry_p &&
                         dir_entry_p->extent_low &&
                         dir_entry_p->user == user);
		return DSK_OK;
        } else {
                dsk_set_error(dsk, "Unable to find file %s\n", name);
                return DSK_ERROR;
        }
}

static bool is_block_in_use(dsk_type *dsk, uint8_t block) {
	/* To be optimized */
	dir_entry_type dir_entry;
	for (int i = 0; i < NUM_DIRENT; i++) {
		dsk_get_dir_entry(dsk, &dir_entry, i);
		if (!is_dir_entry_deleted(&dir_entry)) {
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

static uint32_t get_blocks_in_disk(dsk_type *dsk) {
	if (dsk->total_blocks == 0) {
		for (int i = 0; i < dsk->dsk_info->tracks; i++) {
			dsk->total_blocks  += get_track_size(dsk, i) - 
				sizeof(track_info_type);
		}
	}
	return dsk->total_blocks;
}

static int8_t get_free_block(dsk_type *dsk) {
	if (dsk->last_free_block == 0) {
		uint8_t base_block = 0;
		for (int i = 0; i < base_track_index(dsk); i++) {
			base_block += get_track_size(dsk, i) - sizeof(track_info_type);
		}
		/* Skip directory */
		base_block += (NUM_DIRENT * sizeof(dir_entry_type)) >> 10;
		dsk->last_free_block = base_block;
	}
	LOG(LOG_TRACE, "Base search block is %u", dsk->last_free_block);
	uint8_t block = dsk->last_free_block;
	while (block < dsk_get_total_blocks(dsk)) {
		if (!is_block_in_use(dsk, block)) {
			LOG(LOG_TRACE, "Found free block %02x\n", block);
			/* A bit hacky, but we need to start searching
			 * in the next block to avoid reusing the same
			 * blocks, since the directory entry is only 
			 * updated at the end
			 */
			dsk->last_free_block = block + 1;
			return block;
		}
		++block;
	}
	return -1;
}

static void read_sector(dsk_type *dsk, FILE *fd, uint8_t sector) {
	uint32_t offset = get_block_offset(dsk, sector);
	if (offset > 0) {
		LOG(LOG_DEBUG, "Writing block of size %d, offset %04x", SECTOR_SIZE, offset);
		uint8_t *dst = dsk->image + offset;
		fread(dst, SECTOR_SIZE, 1, fd);
	} else {
		LOG(LOG_WARN, "Skipping sector %d", sector);
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

bool dsk_has_file(dsk_type *dsk, const char *name, uint8_t user) {
	dir_entry_type dir_entry;
	return get_dir_entry_for_file(dsk, name, user, &dir_entry) >= 0;
}

int dsk_add_file(dsk_type *dsk, const char *source_file, 
		 const char *target_name, amsdos_mode_type mode,
		 uint8_t user) {

	LOG(LOG_DEBUG, "dsk_add_file(source=%s, target=%s, mode=%d, user=%u)",
	    source_file, target_name, mode, user);
	dsk_reset_error(dsk);

	if (dsk_has_file(dsk, target_name, user)) {
		dsk_set_error(dsk, "File %s(%d) already exists in dsk",
			      target_name, user);
		return DSK_ERROR;
	}

	off_t size = get_file_size(source_file);
	LOG(LOG_TRACE, "File size is %d", size);

	if (size < 0) {
		LOG(LOG_ERROR, "Unable to determine file size %s", source_file);
		dsk_set_error(dsk, "Unable to determine file size for %s",
			      source_file);
		return DSK_ERROR;
	}

	uint16_t free_dir_entries = get_free_dir_entry_count(dsk);
	if (size > (free_dir_entries * SECTOR_SIZE * 2)) {
		dsk_set_error(dsk, 
			      "Not enough free directory entries %d", 
			      free_dir_entries);
		return DSK_ERROR;
	}

	/* TODO: Check if there are enough free blocks */
	FILE *stream = fopen(source_file, "r");
	uint8_t extent = 0;
	for (uint16_t pos = 0; pos < size;) {
		int8_t dir_entry_index = get_next_free_dir_entry(dsk);
		if (dir_entry_index < 0) {
			dsk_set_error(dsk, "Exhausted directory entries");
			return DSK_ERROR;
		}
		dir_entry_type dir_entry;
		init_dir_entry(&dir_entry);
		dir_entry.user = user;
		dir_entry.extent_low = extent++;
		char buffer[AMSDOS_NAME_LEN + 1];
		get_amsdos_filename(source_file, buffer);
		memcpy(dir_entry.name, buffer, AMSDOS_NAME_LEN);
		get_amsdos_extension(source_file, buffer);
		memcpy(dir_entry.extension, buffer, AMSDOS_EXT_LEN);
		/* Records are of 128bytes */
		uint8_t records = MIN(128, (size - pos + 127) >> 7);
		dir_entry.record_count = records;
		/* Blocks are of 1Kbyte */
		uint8_t blocks = (records + 7) >> 3;
		for (int i = 0; i < blocks; i++) {
			int8_t block = get_free_block(dsk);
			if (block >= 0) {
				dir_entry.blocks[i] = (uint8_t) block;
				uint8_t sector = block << 1;
				read_sector(dsk, stream, sector);
				read_sector(dsk, stream, sector + 1);
				pos += SECTOR_SIZE << 1;
			} else {
				dsk_set_error(dsk, "Free blocks exhausted");
				return DSK_ERROR;
			}
		}
		dsk_set_dir_entry(dsk, &dir_entry, dir_entry_index);
	}
	return DSK_OK;
}
