/*
 * linux_fddriver.c - Floppy disk linux driver
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
	fddriver_type *fddriver = calloc(1, sizeof(fddriver_type));
	char *ptr = strstr(device, "fd");
	if (ptr) {
		char *endp;
		fddriver->drive_number = strtol(ptr + 1, &endp, 10);
		if ((endp - ptr) > 1) {
			LOG(LOG_ERROR, 
			    "Unable to extract drive number from device %s", 
			    device);
			return NULL;
		} else {
			LOG(LOG_TRACE, "Drive Number is %u", 
			    fddriver->drive_number);
		}
	} else {
		LOG(LOG_ERROR, 
		    "Unable to extract drive number from device %s", device);
		return NULL;
	}
	fddriver->fd = open(device, O_ACCMODE | O_NDELAY);
	if (fddriver->fd < 0) {
		LOG(LOG_ERROR, "Error opening floppy device: %s", 
		    strerror(errno));
		return NULL;
	}
	return fddriver;
}

static void init_raw_cmd(struct floppy_raw_cmd *raw_cmd) {
	raw_cmd->flags = 0;
	raw_cmd->track = 0;
	raw_cmd->data  = NULL;
	raw_cmd->kernel_data = NULL;
	raw_cmd->next  = NULL;
	raw_cmd->length = 0;
	raw_cmd->phys_length = 0;
	raw_cmd->buffer_length = 0;
	raw_cmd->cmd_count = 0;
	raw_cmd->reply_count = 0;
	raw_cmd->resultcode = 0;	
}

void fddriver_delete(fddriver_type *fddriver) {
	if (fddriver->fd) {
		close(fddriver->fd);
	}
	free(fddriver);
}

int fddriver_reset(fddriver_type *fddriver) {

	if (ioctl(fddriver->fd, FDRESET) < 0) {
		LOG(LOG_ERROR, "Error resetting fdc: %s", strerror(errno));
		return DSK_ERROR;
	}
	return DSK_OK;
}

int fddriver_init(fddriver_type *fddriver) {
	struct timespec delay = {0, 100000}; /* 100ms */
	if (fddriver_reset(fddriver) != DSK_OK) {
		return DSK_ERROR;
	}
	nanosleep(&delay, NULL);
	if (fddriver_recalibrate(fddriver) != DSK_OK) {
		return DSK_ERROR;
	}
	nanosleep(&delay, NULL);
	return DSK_OK;
}

int fddriver_format_track(fddriver_type *fddriver, track_header_type *track) {
	struct {
		uint8_t cylinder;
		uint8_t head;
		uint8_t sector;
		uint8_t size;
	} data[track->sector_count];

	for (int i = 0; i < track->sector_count; i++) {
		data[i].sector = track->sector_info[i].sector_id;
		data[i].size = track->sector_info[i].size;
		data[i].cylinder = track->sector_info[i].track;
		data[i].head = track->sector_info[i].side;
	}

	struct floppy_raw_cmd raw_cmd;
	init_raw_cmd(&raw_cmd);
	raw_cmd.flags = FD_RAW_WRITE | FD_RAW_INTR | FD_RAW_NEED_SEEK;
	raw_cmd.track = track->track_number;
	raw_cmd.rate = FM_RATE;
	raw_cmd.length = 128 << track->sector_size;
	raw_cmd.data = data;

	raw_cmd.cmd[raw_cmd.cmd_count++] = FD_FORMAT & 0xff;
	raw_cmd.cmd[raw_cmd.cmd_count++] = (track->side_number << 2) | fddriver->drive_number;
	raw_cmd.cmd[raw_cmd.cmd_count++] = track->sector_size;
	raw_cmd.cmd[raw_cmd.cmd_count++] = track->sector_count;
	raw_cmd.cmd[raw_cmd.cmd_count++] = track->gap3_length;
	raw_cmd.cmd[raw_cmd.cmd_count++] = 0xE5;

	int err = ioctl(fddriver->fd, FDRAWCMD, &raw_cmd);
	if (err < 0) {
		LOG(LOG_ERROR, "Error formatting: %s", strerror(errno));
		return DSK_ERROR;
	}
	if (raw_cmd.reply[0] & 0x40) {
		LOG(LOG_ERROR, "Could not format track %i", 
		    track->track_number);
		return DSK_ERROR;
	}
	return DSK_OK;
}

int fddriver_sector_write(fddriver_type *fddriver, track_header_type *track,
			  uint8_t sector, uint8_t *data) {

	struct floppy_raw_cmd raw_cmd;
	unsigned char mask = 0xFF;

	sector_info_type *sector_info = &track->sector_info[sector];

	init_raw_cmd(&raw_cmd);
	raw_cmd.flags = FD_RAW_WRITE | FD_RAW_INTR | FD_RAW_NEED_SEEK;

	raw_cmd.track = sector_info->track;
	raw_cmd.rate = FM_RATE;
	raw_cmd.length = 128 << sector_info->size;
	raw_cmd.data = data;

	if (sector_info->unused1 & 0x040) {
		/* "write deleted data" (totally untested!) */
		raw_cmd.cmd[raw_cmd.cmd_count++] = FD_WRITE_DEL & mask;
	} else {
		/* "write data" */
		raw_cmd.cmd[raw_cmd.cmd_count++] = FD_WRITE & mask;
	}

	// these parameters are same for "write data" and "write deleted data".
	raw_cmd.cmd[raw_cmd.cmd_count++] = (track->side_number << 2) |
		fddriver->drive_number;
	raw_cmd.cmd[raw_cmd.cmd_count++] = sector_info->track; /* track */
	raw_cmd.cmd[raw_cmd.cmd_count++] = sector_info->side; /* head */
	raw_cmd.cmd[raw_cmd.cmd_count++] = sector_info->sector_id; /* sector */
	raw_cmd.cmd[raw_cmd.cmd_count++] = sector_info->size; /* sectorsize */
	raw_cmd.cmd[raw_cmd.cmd_count++] = sector_info->sector_id; /* sector */
	raw_cmd.cmd[raw_cmd.cmd_count++] = track->gap3_length; /* GPL */
	raw_cmd.cmd[raw_cmd.cmd_count++] = 0xFF; /* DTL */

	uint8_t finish = 0, retry = 0;

	do {
		int status = ioctl(fddriver->fd, FDRAWCMD, &raw_cmd);
		if (status < 0) {
			LOG(LOG_ERROR, "Error writing: %s", strerror(errno));
			return DSK_ERROR;
		}
		if (raw_cmd.reply[0] & 0x40) {
			retry++;
			if (retry > fddriver->retries) {
				finish = 1;
			}
			fddriver_recalibrate(fddriver); //Force the head to move again
		} else {
			finish = 1;
		}
	} while (finish == 0);

	if (retry > fddriver->retries) {
		LOG(LOG_ERROR, "Could not write sector %0X", sector);
		return DSK_ERROR;
	}
	return DSK_OK;
}

int fddriver_sector_read(fddriver_type *fddriver, 
			 track_header_type *track,
			 uint8_t sector,
			 uint8_t *buffer) {
	
	struct floppy_raw_cmd raw_cmd;
	sector_info_type *sector_info = &track->sector_info[sector];

	uint8_t ok = 0, retry = 0;
	do {
		init_raw_cmd(&raw_cmd);
		raw_cmd.flags = FD_RAW_READ | FD_RAW_INTR;
		raw_cmd.track = track->track_number;
		raw_cmd.rate = FM_RATE;
		raw_cmd.length = 128 << sector_info->size;
		raw_cmd.data = buffer;
		raw_cmd.cmd_count = 0;
		raw_cmd.cmd[raw_cmd.cmd_count++] = READ_DATA & 0xFF;
		raw_cmd.cmd[raw_cmd.cmd_count++] = (track->side_number << 2) |
			fddriver->drive_number; /* head */
		raw_cmd.cmd[raw_cmd.cmd_count++] = sector_info->track;	/* track */
		raw_cmd.cmd[raw_cmd.cmd_count++] = sector_info->side;	/* head */	
		raw_cmd.cmd[raw_cmd.cmd_count++] = sector_info->sector_id;	/* sector */
		raw_cmd.cmd[raw_cmd.cmd_count++] = sector_info->size;	/* sectorsize */
		raw_cmd.cmd[raw_cmd.cmd_count++] = sector_info->sector_id; /* sector */
		raw_cmd.cmd[raw_cmd.cmd_count++] = track->gap3_length; /* GPL */
		raw_cmd.cmd[raw_cmd.cmd_count++] = 0xFF; /* DTL */
	
		int err = ioctl(fddriver->fd, FDRAWCMD, &raw_cmd);
		if (err < 0) {
			LOG(LOG_ERROR, "Error reading: %s", strerror(errno));
			return DSK_ERROR;
		}

		if (((raw_cmd.reply[0] & 0x0f8) == 0x040) && 
		    (raw_cmd.reply[1] == 0x080)) {
			/* end of cylinder */
			return DSK_OK;
		}

		if (raw_cmd.reply[0] & 0x40) {
			fddriver_recalibrate(fddriver);
			retry++;
			LOG(LOG_WARN, "Retry %d for read command", retry);
		}
		else ok = 1; // Read ok, go to next
	} while ((retry < fddriver->retries) && (ok == 0));

	if (!ok) {
		LOG(LOG_ERROR, "Error reading sector %u: {%02x %02x %02x}",
		    sector_info->sector_id,
		    raw_cmd.reply[0],raw_cmd.reply[1], raw_cmd.reply[2]);
		return DSK_ERROR;
	}
	return DSK_OK;
}

int fddriver_recalibrate(fddriver_type *fddriver) {
	int err;
	struct floppy_raw_cmd raw_cmd;
	unsigned char mask = 0xFF;

	/* some floppy disc controllers will seek a maximum of 77 tracks
	   for a reclibrate command. This is not sufficient if the 
	   position of the read/write head is on track 78 or above as is
	   possible with a 80 track drive.
 
	   other floppy disc controllers will seek 80 tracks for a 
	   recalibrate command.

	   1) perform a recalibrate command.
              the result will either be a successfull seek, and the
              read/write head will be over track 0. (seek end,
              interrupt code = 0). Or there will be an error (seek end,
	      equipment check, and interrupt code!=0).
	   2) test if the read/write head is over track 0 using
	      get drive status.
           3) if the read/write head is not over track 0 (possible with
	      floppy disc controllers that seek up to 77 tracks), then do
              a second recalibrate. 
           4) test if the read/write head is over track 0 using
              get drive status.
           5) if the read/write head is not over track 0 then drive may
              be broken, or may not be connected.
	*/

	/* first recalibrate */
	init_raw_cmd(&raw_cmd);
	raw_cmd.flags = FD_RAW_INTR;
	raw_cmd.length = 0;
	raw_cmd.cmd[raw_cmd.cmd_count++] = FD_RECALIBRATE & mask;
	raw_cmd.cmd[raw_cmd.cmd_count++] = fddriver->drive_number;
	err = ioctl(fddriver->fd, FDRAWCMD, &raw_cmd);
	if (err < 0) {
		LOG(LOG_ERROR, "Error recalibrating %s", strerror(errno));
		return DSK_ERROR;
	}

	/* if read/write head was at track > 77 and floppy disc controller
	can only seek 77 tracks using recalibrate command:
	- seek end
	- seek complete
	- track0 is not set

	perform a second recalibrate to seek the remaining tracks */	
	
	/* get drive status */
	init_raw_cmd(&raw_cmd);
	raw_cmd.flags = 0;
	raw_cmd.length = 0;
	raw_cmd.cmd[raw_cmd.cmd_count++] = FD_GETSTATUS & mask;
	raw_cmd.cmd[raw_cmd.cmd_count++] = fddriver->drive_number;
	err = ioctl(fddriver->fd, FDRAWCMD, &raw_cmd);
	if (err < 0) {
		LOG(LOG_ERROR, "Error recalibrating %s", strerror(errno));
		return DSK_ERROR;
	}
	/* at track 0? */
	if (raw_cmd.reply[0] & ST3_TZ) {
		return DSK_OK;
	}

	/* recalibrate a second time */
	init_raw_cmd(&raw_cmd);
	raw_cmd.flags = FD_RAW_INTR;
	raw_cmd.length = 0;
	raw_cmd.cmd[raw_cmd.cmd_count++] = FD_RECALIBRATE & mask;
	raw_cmd.cmd[raw_cmd.cmd_count++] = fddriver->drive_number;
	err = ioctl(fddriver->fd, FDRAWCMD, &raw_cmd);
	if (err < 0) {
		LOG(LOG_ERROR, "Error recalibrating %s", strerror(errno));
		return DSK_ERROR;
	}

	/* get drive status */
	init_raw_cmd(&raw_cmd);
	raw_cmd.flags = 0;
	raw_cmd.length = 0;
	raw_cmd.cmd[raw_cmd.cmd_count++] = FD_GETSTATUS & mask;
	raw_cmd.cmd[raw_cmd.cmd_count++] = fddriver->drive_number;
	err = ioctl(fddriver->fd, FDRAWCMD, &raw_cmd);
	if (err < 0) {
		LOG(LOG_ERROR, "Error getting drive status %s", 
		    strerror(errno));
		return DSK_ERROR;
	}

	/* at track 0? */
	if (raw_cmd.reply[0] & ST3_TZ) {
		return DSK_OK;
	}

	/* if recalibrate failed a second time:
	- disc drive is broken
	- disc drive doesn't exist
	*/
	LOG(LOG_ERROR, "Disc drive malfunction, or disc drive not connected");
	return DSK_ERROR;
}

int fddriver_track_seek(fddriver_type *fddriver, uint8_t track) {
	struct floppy_raw_cmd raw_cmd;
	
	init_raw_cmd(&raw_cmd);
	raw_cmd.flags = FD_RAW_INTR;
	raw_cmd.track = track;
	raw_cmd.rate = 0;
	raw_cmd.length = 0;
	raw_cmd.cmd[raw_cmd.cmd_count++] = FD_SEEK & 0xff;
	raw_cmd.cmd[raw_cmd.cmd_count++] = fddriver->drive_number;
	raw_cmd.cmd[raw_cmd.cmd_count++] = track;
	if (ioctl(fddriver->fd, FDRAWCMD, &raw_cmd) < 0) {
                LOG(LOG_ERROR, "Seek ioctl error: %s", strerror(errno));
		return DSK_ERROR;
        }
	return DSK_OK;
}
	
int fddriver_sectorids_read(fddriver_type *fddriver, 
			    track_header_type *track) {
	uint8_t mask = 0xff;

	struct floppy_raw_cmd cmds[32];
	struct floppy_raw_cmd *cur_cmd = cmds;

	/* --  detect unformatted track -- */
	/* attempt to read an id and compare the result information
	against what we are expecting for a unformatted track */

	/* initialise this cmd */
	init_raw_cmd(cur_cmd);
	cur_cmd->flags = FD_RAW_INTR;
	cur_cmd->track = track->track_number;
	cur_cmd->rate = FM_RATE;
	cur_cmd->length = 0;
	cur_cmd->cmd[cur_cmd->cmd_count++] = READ_ID & mask;
	cur_cmd->cmd[cur_cmd->cmd_count++] = track->side_number << 2 |
		fddriver->drive_number;
			
	int err = ioctl(fddriver->fd, FDRAWCMD, cmds);
	if (err < 0) {
		LOG(LOG_ERROR, "reading ids %s", strerror(errno));
		return DSK_ERROR;
	}

	if ((cur_cmd->reply[0] & 0x0c0) == 0x040) {
		/* check for specific command response which indicates
		   a unformatted track */
		if ((cur_cmd->reply[1] == 1) && /* ST1 */
		    (cur_cmd->reply[2] == 0) && /* ST2 */
		    (cur_cmd->reply[4] == 0) && /* H */
		    (cur_cmd->reply[5] == 1) && /* R */
		    (cur_cmd->reply[6] == 0) /* N */
		    ) {
			return 0;
		}
	}


	/* setup a list of 32 read id commands:
	- if each read id command is done seperatly then
		some id's will be skipped. (the time between reading a id and
		the next using seperate reads is too long for small sectors of
		256 bytes in size!
		- I've only seen up to 32 sectors on copyprotections,
		I don't think there are copyprotections that use more.
		- don't use seek flag; this seems to cause id's to be missed.
		
	   problems:
		- need to calculate number of sectors per track
		- need to find the first sector id
	*/
	/* synchronises with 2nd sector id on track */
	uint8_t *buffer = malloc(8 * 1024);
	cur_cmd = cmds;
	init_raw_cmd(cur_cmd);
	cur_cmd->flags = FD_RAW_READ | FD_RAW_INTR | FD_RAW_MORE;
	cur_cmd->data = buffer;
	cur_cmd->track = track->track_number;
	cur_cmd->rate = FM_RATE;
	cur_cmd->length = 6500;
	cur_cmd->cmd[cur_cmd->cmd_count++] = FD_READTRACK & mask;
	cur_cmd->cmd[cur_cmd->cmd_count++] = track->side_number << 2 |
		fddriver->drive_number;
	cur_cmd->cmd[cur_cmd->cmd_count++] = 0;
	cur_cmd->cmd[cur_cmd->cmd_count++] = 0;
	cur_cmd->cmd[cur_cmd->cmd_count++] = 0;
	cur_cmd->cmd[cur_cmd->cmd_count++] = 0;
	cur_cmd->cmd[cur_cmd->cmd_count++] = 7;
	cur_cmd->cmd[cur_cmd->cmd_count++] = 0x02a;
	cur_cmd->cmd[cur_cmd->cmd_count++] = 0x0ff;

	/* initialise the read id command list */
	for (int i = 1; i < 32; i++) {
		cur_cmd = &cmds[i];

		/* initialise this cmd */
		init_raw_cmd(cur_cmd);
		cur_cmd->flags = FD_RAW_INTR;
		if (i != (32 - 1)) {
			cur_cmd->flags |= FD_RAW_MORE;
		}
		cur_cmd->track = track->track_number;
		cur_cmd->rate = FM_RATE;
		cur_cmd->length = 0; 
		cur_cmd->cmd[cur_cmd->cmd_count++] = READ_ID & mask;
		cur_cmd->cmd[cur_cmd->cmd_count++] = track->side_number << 2 |
			fddriver->drive_number;
	}		
	
	err = ioctl(fddriver->fd, FDRAWCMD, cmds);

	if (err < 0) {
		free(buffer);
		LOG(LOG_ERROR, "Error reading id: %s", strerror(errno));
		return DSK_ERROR;
	}

	track->sector_count = NSECTS;
	for (int i = 1; i < NSECTS +1; i++) {
		cur_cmd = &cmds[i];
		track->sector_info[i - 1].track = cur_cmd->reply[3];
		track->sector_info[i - 1].side = cur_cmd->reply[4];
		track->sector_info[i - 1].sector_id = cur_cmd->reply[5];
		track->sector_info[i - 1].size = cur_cmd->reply[6];
	}
	free(buffer);
	/* need to calculate number of sectors differently */	
	return NSECTS;
}


	