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
#include "fddriver.h"

static void dsk_error_set(dsk_type *dsk, const char *fmt, ...) {
	if (dsk) {
		va_list ap;
		va_start(ap, fmt);
		vsnprintf(dsk->error, DSK_ERROR_SIZE, fmt, ap);
	} else {
		LOG(LOG_ERROR, "Attempt to set error on uninitialized DSK");
	}
}

const char *dsk_error_get(dsk_type *dsk) {
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

uint32_t dsk_track_size_get(dsk_type *dsk, uint8_t track) {
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
			dsk_track_size_get(dsk, 0);
	} else if (is_edsk_image(dsk)) {
		uint32_t size = 0;
		uint32_t track_count = dsk->dsk_info->tracks * 
			dsk->dsk_info->sides;
		for (int i = 0; i < track_count; i++) {
			size += dsk_track_size_get(dsk, i);
		}
		return size;
	} else {
		dsk_error_set(dsk, "Getting image size from unsupported image");
		return 0;
	}
}

static uint32_t get_track_offset(dsk_type *dsk, uint8_t track) {
	if (is_dsk_image(dsk)) {
		return dsk_track_size_get(dsk, 0) * track;
	} else if (is_edsk_image(dsk)) {
		uint32_t offset = 0;
		for (int i = 0; i < track; i++) {
			offset += dsk_track_size_get(dsk, i);
		}
		return offset;
	} else {
		dsk_error_set(dsk, "Getting track offset from unsupported image");
		return 0;
	}
}

static uint8_t sectors_per_track(dsk_type *dsk) {
	/* Assumes that all tracks have the same number of sectors*/
	
	track_header_type *track0 =
		dsk_track_info_get(dsk, 0);
	return track0->sector_count;
}

static uint8_t first_sector_id(dsk_type *dsk) {
	track_header_type *track0 = 
		dsk_track_info_get(dsk, 0);
	uint8_t min = ~0;
	for (int i = 0; i < track0->sector_count; i++) {
		if (track0->sector_info[i].sector_id < min) {
			min = track0->sector_info[i].sector_id;
		}
	}
	LOG(LOG_TRACE, "First sector id is %02x", min);
	return min;
}

uint32_t dsk_track_start_sector_get(dsk_type *dsk,
				    uint8_t track_index) {
	uint32_t startup_sector = 0;

	for (int i = 0; i < track_index; i++) {
		track_header_type *track_info = dsk_track_info_get(dsk, i);
		startup_sector += track_info->sector_count;
	}
	return startup_sector;
}

static track_header_type *get_sector_track_info(dsk_type *dsk, uint8_t sector) {
	uint32_t current_sector = 0;
	for (int i = 0; i < dsk->dsk_info->tracks; i++) {
		track_header_type *track_info = dsk_track_info_get(dsk, i);
		current_sector += track_info->sector_count;
		if (current_sector > sector) {
			return track_info;
		}
	}
	LOG(LOG_ERROR, "Unable to get track for sector %u", sector);
	return NULL;
}

static uint32_t get_sector_offset_in_track(track_header_type *track, uint8_t sector_id) {
	uint32_t offset = sizeof(track_header_type);
	LOG(LOG_DEBUG, "get_sector_offset_in_track(track=%u, sector=%02x)",
	    sector_id, track->track_number);
	for (int i = 0; i < track->sector_count; i++) {
		sector_info_type *sinfo = &track->sector_info[i];
		LOG(LOG_TRACE, "Adding offset for sector %02x. Current %04x", 
		    sinfo->sector_id, offset);
		if (sinfo->sector_id == sector_id) {
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
		
static int32_t get_sector_offset(dsk_type *dsk,
				uint8_t sector) {
	LOG(LOG_DEBUG, "get_sector_offset(sector=%u)", sector);

	uint8_t current_sector = 0;
	uint32_t offset = 0;
	uint8_t sector_id = first_sector_id(dsk);
	for (uint8_t track = 0; track < dsk->dsk_info->tracks; track++) {
		track_header_type *track_info = dsk_track_info_get(dsk, 
								   track);
		current_sector += track_info->sector_count;
		if (current_sector < sector) {
			offset += dsk_track_size_get(dsk, track);
		} else {
			sector_id += current_sector - sector;
			offset += get_sector_offset_in_track(track_info,
							     sector_id);
			break;
		}
	}
	LOG(LOG_TRACE, "Track offset %u", offset);
	return offset;
}

uint32_t dsk_sector_offset_get(dsk_type *dsk, 
			       uint8_t track_id, uint8_t side, 
			       uint8_t sector_id) {
	LOG(LOG_DEBUG, "dsk_sector_offset_get(track=%u, side=%u, sector=%02x)",
	    track_id, side, sector_id);
	uint32_t offset = 0;
	for (int i = 0; i < dsk->dsk_info->tracks; i++) {
		track_header_type *track = dsk_track_info_get(dsk, i);
		LOG(LOG_TRACE, "Searching in track %u", track->track_number);
		if (track->track_number == track_id &&
		    track->side_number == side) {
			offset += get_sector_offset_in_track(track, sector_id);
			break;
		} else {
			offset += dsk_track_size_get(dsk, i);
		}
		LOG(LOG_TRACE, " +current offset %04x", offset);
	}
	LOG(LOG_DEBUG, " +calculated offset. %04x", offset);
	return offset;
}

static bool is_valid_track_in_offset(dsk_type *dsk, uint32_t offset) {
	return !strncmp((char*) dsk->image + offset, DSK_TRACK_HEADER, 13);
}

track_header_type *dsk_track_info_get(dsk_type *dsk, uint8_t track) {
	if (dsk && dsk->image) {
		if (track < dsk->dsk_info->tracks) {
			if (!dsk->track_info[track]) {
				uint32_t offset = get_track_offset(dsk, track);
				if (is_valid_track_in_offset(dsk, offset)) {
					dsk->track_info[track] = (track_header_type*) (dsk->image + offset);
				} else {
					LOG(LOG_ERROR, 
					    "Invalid track offset %04x", 
					    offset);
					dsk_error_set(dsk, 
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
		dsk->dsk_info = (dsk_header_type *) malloc(sizeof(dsk_header_type));
		int nread = fread(dsk->dsk_info, 
				  sizeof(dsk_header_type), 1, fh);
		if (nread == 1) {
			uint32_t disk_size = get_image_size(dsk);
			dsk->image = (uint8_t*) malloc(disk_size);
			nread = fread(dsk->image,
				      disk_size, 1, fh);
			fclose(fh);
			if (nread == 1) {
				dsk->track_info = (track_header_type**) 
					calloc(dsk->dsk_info->tracks,
					       sizeof(track_header_type**));
				dsk->error = (char*) calloc(1, DSK_ERROR_SIZE);
				return dsk;
			}
		} else {
			LOG(LOG_ERROR, "Unable to read %d, was %d\n",
			    sizeof(dsk_header_type), nread);
		}
	}
	if (fh != 0) {
		fclose(fh);
	}
	dsk_delete(dsk);
	return 0;
}
	
static uint32_t get_capacity(dsk_type *dsk) {
	uint32_t bytes = 0;
	for (int i = 0; i < dsk->dsk_info->tracks; i++) {
		track_header_type *track = 
			dsk_track_info_get(dsk, i);
		bytes += track->sector_count * 
			(BASE_SECTOR_SIZE << track->sector_size);
	}
	LOG(LOG_TRACE, "Total capacity of dsk %u", bytes);
	return bytes;
}

static uint32_t get_sector_count(dsk_type *dsk) {
	uint32_t sectors = 0;
	for (int i = 0; i < dsk->dsk_info->tracks; i++) {
		track_header_type *track = 
			dsk_track_info_get(dsk, i);
		sectors += track->sector_count;
	}
	LOG(LOG_TRACE, "Total sectors in dsk: %u", sectors);
	return sectors;
}

dsk_info_type *dsk_info_get(dsk_type *dsk, dsk_info_type *info) {
	if (dsk && dsk->dsk_info) {
		memset(info, 0, sizeof(dsk_info_type));
		strncpy(info->magic, dsk->dsk_info->magic, 34);
		strncpy(info->creator, dsk->dsk_info->creator, 14);
		info->type = is_dsk_image(dsk) ? DSK : EDSK;
		info->tracks = dsk->dsk_info->tracks;
		info->sides = dsk->dsk_info->sides;
		info->capacity = get_capacity(dsk);
		info->sectors = get_sector_count(dsk);
		info->first_sector_id = first_sector_id(dsk);
		return info;
	} else {
		LOG(LOG_ERROR, "Unable to get info from unitialized dsk");
		return NULL;
	}
}

int dsk_sector_write(dsk_type *dsk, const uint8_t *src, uint8_t sector) {
	track_header_type *track = get_sector_track_info(dsk, sector);
	uint32_t sector_size = 128 << track->sector_size;
	int32_t offset = get_sector_offset(dsk, sector);
	if (offset >= 0) {
		LOG(LOG_DEBUG, "Writing block of size %d, offset %04x", 
		    sector_size, offset);
		uint8_t *dst = dsk->image + offset;
		memcpy(dst, src, sector_size);
		return DSK_OK;
	} else {
		dsk_error_set(dsk, "Unable to calculate offset for sector %u",
			      sector);
		return DSK_ERROR;
	}
}

int dsk_sector_read(dsk_type *dsk, uint8_t *dst, uint8_t sector) {
	track_header_type *track = get_sector_track_info(dsk, sector);
	uint32_t sector_size = 128 << track->sector_size;
	int32_t offset = get_sector_offset(dsk, sector);
	if (offset >= 0) {
		LOG(LOG_DEBUG, "Reading block of size %d, offset %04x", 
		    sector_size, offset);
		uint8_t *src = dsk->image + offset;
		memcpy(dst, src, sector_size);
		return DSK_OK;
	} else {
		dsk_error_set(dsk, "Unable to calculate offset for sector %u",
			      sector);
		return DSK_ERROR;
	}
}

int dsk_image_dump(dsk_type *dsk, const char *destination) {
        FILE *fd = fopen(destination, "w");
        if (fd != NULL) {
                if (fwrite(dsk->dsk_info,
                           sizeof(dsk_header_type), 1, fd) < 1) {
                        dsk_error_set(dsk, "Writing image to %s. %s",
                                      destination,
                                      strerror(errno));
                        fclose(fd);
                        return DSK_ERROR;
                }
                if (fwrite(dsk->image,
                           get_image_size(dsk), 1, fd) < 1) {
                        dsk_error_set(dsk, "Writing image to %s. %s",
                                      destination,
                                      strerror(errno));
                        fclose(fd);
                        return DSK_ERROR;
                }
                fclose(fd);
                return DSK_OK;
        } else {
                dsk_error_set(dsk, "Opening destination file %s. %s",
                              destination,
                              strerror(errno));
                return DSK_ERROR;
        }
}

int dsk_disk_write(dsk_type *dsk, const char *device) {

	fddriver_type *fddriver = fddriver_new(device);
	if (fddriver == NULL) {
		dsk_error_set(dsk, "Unable to instantiate fd driver");
		return DSK_ERROR;
	}

	for (int i = 0; i < dsk->dsk_info->tracks; i++) {
		for (int j = 0; j < dsk->dsk_info->sides; j++) {
			track_header_type *track = dsk_track_info_get(dsk, i + j);
			if (fddriver_format_track(fddriver, track) != DSK_OK) {
				LOG(LOG_ERROR, "Formatting track");
				fddriver_delete(fddriver);
				return DSK_ERROR;
			}
			for (int k = 0; k < track->sector_count; k++) {
				sector_info_type *sector_info = 
					&track->sector_info[k];
				LOG(LOG_TRACE, "Writing side/track/sector %02x/%02x/%02x", 
				    track->side_number,
				    track->track_number,
				    sector_info->sector_id);
				uint8_t *data = dsk->image + 
					dsk_sector_offset_get(dsk, i, j, 
							      sector_info->sector_id);
							      
				if (fddriver_sector_write(fddriver, track, k,
							  data) != DSK_OK) {
					LOG(LOG_ERROR, "Writing sector to disk");
					dsk_error_set(dsk, "Writing sector to disk");
					fddriver_delete(fddriver);
					return DSK_ERROR;
				}
			}
		}
	}
	fddriver_delete(fddriver);
	return DSK_OK;
}
			

