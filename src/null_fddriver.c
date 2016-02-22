/*
 * null_fddriver.c - Null Floppy disk driver
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

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/fd.h>
#include <linux/fdreg.h>
#include <sys/ioctl.h>
#include <time.h>
#include "fddriver.h"
#include "dsk.h"
#include "log.h"

fddriver_type *fddriver_new(const char *device) {
	LOG(LOG_ERROR, "Unable to instantiate a floppy driver. Unsupported platform");
	return NULL;
}

void fddriver_delete(fddriver_type *fddriver) {
}

int fddriver_reset(fddriver_type *fddriver) {
	LOG(LOG_ERROR, "Unsupported platform");
	return DSK_ERROR;
}

int fddriver_init(fddriver_type *fddriver) {
	LOG(LOG_ERROR, "Unsupported platform");
	return DSK_ERROR;
}

int fddriver_format_track(fddriver_type *fddriver, track_header_type *track) {
	LOG(LOG_ERROR, "Unsupported platform");
	return DSK_ERROR;
}

int fddriver_sector_write(fddriver_type *fddriver, track_header_type *track,
			  uint8_t sector, uint8_t *data) {
	LOG(LOG_ERROR, "Unsupported platform");
	return DSK_ERROR;
}

int fddriver_sector_read(fddriver_type *fddriver, 
			 track_header_type *track,
			 uint8_t sector,
			 uint8_t *buffer) {
	LOG(LOG_ERROR, "Unsupported platform");
	return DSK_ERROR;
}

int fddriver_recalibrate(fddriver_type *fddriver) {
	LOG(LOG_ERROR, "Unsupported platform");
	return DSK_ERROR;
}

int fddriver_track_seek(fddriver_type *fddriver, uint8_t track) {
	LOG(LOG_ERROR, "Unsupported platform");
	return DSK_ERROR;
}
	
int fddriver_sectorids_read(fddriver_type *fddriver, 
			    track_header_type *track) {
	LOG(LOG_ERROR, "Unsupported platform");
	return DSK_ERROR;
}


	