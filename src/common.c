/* $Id: common.c,v 1.3 2003/08/24 19:40:37 nurgle Exp $
 *
 * common.c - Common functions for dsktools.
 * Copyright (C)2001 Andreas Micklei <nurgle@gmx.de>
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

#include "common.h"

void myabort(char *s)
{
	fprintf(stderr,s);
	exit(1);
}

void printdiskinfo(FILE *out, Diskinfo *diskinfo)
{
	char *magic = diskinfo->magic;
	int tracks = diskinfo->tracks;
	int heads = diskinfo->heads;
	int tracklen = diskinfo->tracklen[0] + (diskinfo->tracklen[1] * 256);
	fprintf(out, "MAGIC:\t%s\nTRACKS:\t%i\nHEADS:\t%i\nTRACKL:\t%X\n",
		magic, tracks, heads, tracklen);
}

void printsectorinfo(FILE *out, Sectorinfo *sectorinfo)
{
	fprintf(out, "%X:%i ", sectorinfo->sector, sectorinfo->bps);
}

void printtrackinfo(FILE *out, Trackinfo *trackinfo)
{
	int i;
	char *magic = trackinfo->magic;
	int track = trackinfo->track;
	int head = trackinfo->head;
	int bps = trackinfo->bps;
	int spt = trackinfo->spt;
	int gap = trackinfo->gap;
	int fill = trackinfo->fill;
	/*fprintf(out, "MAGIC:\t%sTRACK: %2.2i HEAD: %i BPS: %i SPT: %i GAP: 0x%X FILL: 0x%X\n",
		magic, track, head, bps, spt, gap, fill);*/
	/*fprintf(out, "TRACK: %2.2i HEAD: %i BPS: %i SPT: %i GAP: 0x%X FILL: 0x%X\n",
		track, head, bps, spt, gap, fill);*/
	fprintf(out, "%2.2i: %i-%i-%i-%X-%X",
		track, head, bps, spt, gap, fill);
	/*for (i=0; i<trackinfo->spt; i++) {
		printsectorinfo(out, trackinfo->sectorinfo+i);
	}
	fprintf(out,"\n");*/
}

void init_sectorinfo(Sectorinfo *sectorinfo, int track, int head, int sector)
{
	sectorinfo->track = track;
	sectorinfo->head = head;
	sectorinfo->sector = sector;
	sectorinfo->bps = BPS;
	sectorinfo->err1 = 0;
	sectorinfo->err2 = 0;
	sectorinfo->unused1 = 0;
	sectorinfo->unused2 = 0;
}

/* Initialise a raw FDC command */
void init_raw_cmd(struct floppy_raw_cmd *raw_cmd)
{
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

void reset(int fd) {

	int err;

	err = ioctl(fd, FDRESET);
	if (err < 0) {
		perror("Error resetting fdc");
		exit(1);
	}

}

void recalibrate(int fd, int drive) {

	int i, err;
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
	raw_cmd.cmd[raw_cmd.cmd_count++] = drive;			
	err = ioctl(fd, FDRAWCMD, &raw_cmd);
	if (err < 0) {
		perror("Error recalibrating");
		exit(1);
	}

	/* if read/write head was at track>77 and floppy disc controller
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
	raw_cmd.cmd[raw_cmd.cmd_count++] = drive;
	err = ioctl(fd, FDRAWCMD, &raw_cmd);
	if (err<0)
	{
		perror("Error recalibrating");
		exit(1);
	}
	/* at track 0? */
	if (raw_cmd.reply[0] & ST3_TZ)
		return;


	/* no */

	/* recalibrate a second time */
	init_raw_cmd(&raw_cmd);
	raw_cmd.flags = FD_RAW_INTR;
	raw_cmd.length = 0;
	raw_cmd.cmd[raw_cmd.cmd_count++] = FD_RECALIBRATE & mask;
	raw_cmd.cmd[raw_cmd.cmd_count++] = drive;			
	err = ioctl(fd, FDRAWCMD, &raw_cmd);
	if (err < 0) {
		perror("Error recalibrating");
		exit(1);
	}

	/* get drive status */
	init_raw_cmd(&raw_cmd);
	raw_cmd.flags = 0;
	raw_cmd.length = 0;
	raw_cmd.cmd[raw_cmd.cmd_count++] = FD_GETSTATUS & mask;
	raw_cmd.cmd[raw_cmd.cmd_count++] = drive;
	err = ioctl(fd, FDRAWCMD, &raw_cmd);
	if (err<0)
	{
		perror("Error recalibrating");
		exit(1);
	}

	/* at track 0? */
	if (raw_cmd.reply[0] & ST3_TZ)
		return;

	/* if recalibrate failed a second time:
	- disc drive is broken
	- disc drive doesn't exist
	*/

	printf("Disc drive malfunction, or disc drive not connected");
	exit(1);
}

void init(int fd, int drive) {

	reset( fd );
	usleep( 100 );
	recalibrate( fd,drive);
	usleep( 100 );
}

