/*
 * fddriver.h - Floppy disk driver
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

#ifndef FDDRIVER_H
#define FDDRIVER_H

#include "dsk.h"

#define CMD_ADD(_cmd, value) \
_cmd.cmd[_cmd.cmd_count++] = value & 0xff

#define FM_RATE 2
/* These raw floppy commands are missing in fdreg.h. Use with caution.*/
#define FD_READ_DEL             0xCC    /* read deleted with MT, MFM */
#define FD_WRITE_DEL            0xC9    /* write deleted with MT, MFM */
#define FD_READTRACK (2|0x040)
#define READ_ID 0x04a
#define READ_DATA 0x046
#define NSECTS 9 /* Hardcoded so far */

typedef struct {
	int fd;
	uint8_t retries;
	uint8_t drive_number;
} fddriver_type;

fddriver_type *fddriver_new(const char *device);
void fddriver_delete(fddriver_type *fddriver);
int fddriver_reset(fddriver_type *fddriver);
int fddriver_init(fddriver_type *fddriver);
int fddriver_format_track(fddriver_type *fddriver, track_header_type *track);
int fddriver_sector_write(fddriver_type *fddriver, track_header_type *track,
			  uint8_t sector, uint8_t *data);
int fddriver_sector_read(fddriver_type *fddriver, 
			 track_header_type *track,
			 uint8_t sector,
			 uint8_t *buffer);
int fddriver_recalibrate(fddriver_type *fddriver);
int fddriver_sectorids_read(fddriver_type *fddriver, track_header_type *track);

#endif //FDDRIVER_H
