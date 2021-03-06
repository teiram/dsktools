/* $Id: dskwrite.c,v 1.10 2012-02-08 15:23:44 nurgle Exp $
 *
 * dskwrite.c - Small utility to write CPC disk images to a floppy disk under
 * Linux with a standard PC FDC.
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

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/fd.h>
#include <linux/fdreg.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <fcntl.h>

#define MAX_RETRY 20

/* notes:
 *
 * the C (track),H (head),R (sector id),N (sector size) parameters in the
 * sector id field do not need to be the same as the physical track and
 * physical side.
 */

void format_track(int fd, int track, Trackinfo *trackinfo, unsigned char side) {

	int i, err;
	struct floppy_raw_cmd raw_cmd;
	format_map_t data[20];		//FIXME
	unsigned char mask = 0xFF;
	Sectorinfo *sectorinfo;

	sectorinfo = trackinfo->sectorinfo;
	for (i=0; i<trackinfo->spt; i++) {
		//data[i].sector = 0xC1+i;
		//data[i].size = 2;	/* 0=128, 1=256, 2=512,... */
		data[i].sector = sectorinfo->sector;
		data[i].size = sectorinfo->bps;
		data[i].cylinder = sectorinfo->track;
		data[i].head = sectorinfo->head;
		sectorinfo++;
	}
	//fprintf(stderr, "Formatting Track %i\n", track);
	init_raw_cmd(&raw_cmd);
	raw_cmd.flags = FD_RAW_WRITE | FD_RAW_INTR;
	raw_cmd.flags |= FD_RAW_NEED_SEEK;
	raw_cmd.track = track;
	raw_cmd.rate  = 2;	/* SD */
	//raw_cmd.length= 512;	/* Sectorsize */
	raw_cmd.length= (128<<(trackinfo->bps));
	raw_cmd.data  = data;

	raw_cmd.cmd[raw_cmd.cmd_count++] = FD_FORMAT & mask;
	raw_cmd.cmd[raw_cmd.cmd_count++] = side;	/* head: 4 or 0 */
	//raw_cmd.cmd[raw_cmd.cmd_count++] = 2;	/* sectorsize */
	//raw_cmd.cmd[raw_cmd.cmd_count++] = 9;	/* sectors */
	//raw_cmd.cmd[raw_cmd.cmd_count++] = 82;/* GAP */
	//raw_cmd.cmd[raw_cmd.cmd_count++] = 0;	/* filler */
	raw_cmd.cmd[raw_cmd.cmd_count++] = trackinfo->bps;	/* sectorsize */
	raw_cmd.cmd[raw_cmd.cmd_count++] = trackinfo->spt;	/* sectors */
	raw_cmd.cmd[raw_cmd.cmd_count++] = trackinfo->gap;	/* GAP */
	raw_cmd.cmd[raw_cmd.cmd_count++] = trackinfo->fill;	/* filler */
	err = ioctl(fd, FDRAWCMD, &raw_cmd);
	if (err < 0) {
		perror("Error formatting");
		exit(1);
	}
	if (raw_cmd.reply[0] & 0x40) {
		fprintf(stderr, "Could not format track %i\n", track);
		exit(1);
	}
}

/* notes:
 *
 * when writing, you must specify the sector c,h,r,n exactly, otherwise fdc
 * will fail to write data to sector.
 */

//void write_sect(int fd, int track, unsigned char sector, unsigned char *data) {
void write_sect(int fd, Trackinfo *trackinfo, Sectorinfo *sectorinfo,
	unsigned char *data, unsigned char side) {

	int i, err;
	struct floppy_raw_cmd raw_cmd;
	//format_map_t data[9];
	unsigned char mask = 0xFF;

	init_raw_cmd(&raw_cmd);
	raw_cmd.flags = FD_RAW_WRITE | FD_RAW_INTR;
	raw_cmd.flags |= FD_RAW_NEED_SEEK;

	raw_cmd.track = sectorinfo->track;
	raw_cmd.rate  = 2;	/* SD */
	raw_cmd.length= (128<<(sectorinfo->bps)); /* Sectorsize */
	raw_cmd.data  = data;

	if (sectorinfo->unused1 & 0x040)
	{
		/* "write deleted data" (totally untested!) */
		raw_cmd.cmd[raw_cmd.cmd_count++] = FD_WRITE_DEL & mask;
	}
	else
	{
		/* "write data" */
		raw_cmd.cmd[raw_cmd.cmd_count++] = FD_WRITE & mask;
	}

	// these parameters are same for "write data" and "write deleted data".
	raw_cmd.cmd[raw_cmd.cmd_count++] = side;		/* head */
	raw_cmd.cmd[raw_cmd.cmd_count++] = sectorinfo->track;	/* track */
	raw_cmd.cmd[raw_cmd.cmd_count++] = sectorinfo->head;	/* head */
	raw_cmd.cmd[raw_cmd.cmd_count++] = sectorinfo->sector;	/* sector */
	raw_cmd.cmd[raw_cmd.cmd_count++] = sectorinfo->bps;	/* sectorsize */
	raw_cmd.cmd[raw_cmd.cmd_count++] = sectorinfo->sector;	/* sector */
	raw_cmd.cmd[raw_cmd.cmd_count++] = trackinfo->gap;	/* GPL */
	raw_cmd.cmd[raw_cmd.cmd_count++] = 0xFF;		/* DTL */

	char ok=0, retry=0;

	do {
		err = ioctl(fd, FDRAWCMD, &raw_cmd);
		if (err < 0) {
			perror("Error writing");
			exit(1);
		}
		if (raw_cmd.reply[0] & 0x40) {
			retry++;
			if (retry>MAX_RETRY) ok=1;
			recalibrate(fd, 0); //Force the head to move again
		}
		else ok=1;
	} while (ok==0);

	if (retry>MAX_RETRY)
		fprintf(stderr, "Could not write sector %0X\n",
			sectorinfo->sector);
}

void writedsk(char *filename, unsigned char side) {

	/* Variable declarations */
	int fd, tmp, err;
	char *drive;
	struct floppy_raw_cmd raw_cmd;
	//char buffer[ 512 * 2 * 24 ];
	//char buffer[ 512 * 9 ];
	char buffer[ 9 * sizeof(format_map_t) ];
	format_map_t *data;

	Diskinfo diskinfo;
	Trackinfo trackinfo;
	Sectorinfo *sectorinfo, **sectorinfos;
	unsigned char track[MAX_TRACKLEN], *sect;
	int tracklen;
	FILE *in;
	int i, j, count;
	char *magic_disk = MAGIC_DISK;
	char *magic_edisk = MAGIC_EDISK;
	char *magic_track = MAGIC_TRACK;
	char flag_edisk = FALSE;	// indicates extended disk image format

	/* initialization */
	drive = "/dev/fd0";

	/* open drive */
	fd = open( drive, O_ACCMODE | O_NDELAY);
	if ( fd < 0 ){
		perror("Error opening floppy device");
		exit(1);
	}

	/* open file */
	in = fopen(filename, "r");
	if (in == NULL) {
		perror("Error opening image file");
		exit(1);
	}

	init( fd, 0 );

	/* read disk info, detect extended image */
	count = fread(&diskinfo, 1, sizeof(diskinfo), in);
	if (count != sizeof(diskinfo)) {
		myabort("Error reading Disk-Info: File to short\n");
	}
	if (strncmp(diskinfo.magic, magic_disk, strlen(magic_disk))) {
		if (strncmp(diskinfo.magic, magic_edisk, strlen(magic_edisk))) {
			myabort("Error reading Disk-Info: Invalid Disk-Info\n");
		}
		flag_edisk = TRUE;
	}
	printdiskinfo(stderr, &diskinfo);

	/* Get tracklen for normal disk images */
	tracklen = (diskinfo.tracklen[0] + diskinfo.tracklen[1]*256) - 0x100;

	/*fprintf(stderr, "writing Track: ");*/
	for (i=0; i<diskinfo.tracks * diskinfo.heads; i++) {
		/* read in track */
		/*fprintf(stderr, "%2.2i ",i);
		fflush(stderr);*/
		if (flag_edisk) tracklen = diskinfo.tracklenhigh[i]*256 - 0x100;
		if (tracklen > MAX_TRACKLEN) {
			myabort("Error: Track to long.\n");
		}

		/* read trackinfo */
		memset(&trackinfo, 0, sizeof(trackinfo));
		count = fread(&trackinfo, 1, sizeof(trackinfo), in);
		if (count != sizeof(trackinfo)) {
			myabort("Error reading Track-Info: File to short\n");
		}
		if (strncmp(trackinfo.magic, magic_track, strlen(magic_track)))
			myabort("Error reading Track-Info: Invalid Track-Info\n");

		if (diskinfo.heads == 2) {
			side = (trackinfo.head == 0) ? 0 : 4;
		}
		printtrackinfo(stderr, &trackinfo);

		/* read track */
		count = fread(track, 1, tracklen, in);
		if (count != tracklen)
			myabort("Error reading Track: File to short\n");

		/* format track */
		format_track(fd, i/diskinfo.heads, &trackinfo, side);

		/* write track */
		sect = track;
		sectorinfo = trackinfo.sectorinfo;
		fprintf(stderr, " [");
		for (j=0; j<trackinfo.spt; j++) {
			fprintf(stderr, "%0X ", sectorinfo->sector);
			write_sect(fd, &trackinfo, sectorinfo, sect, side);
			sectorinfo++;
			sect += (128<<trackinfo.bps);
		}
		fprintf(stderr, "]\n");
	}
	fprintf(stderr,"\n");

}

int main(int argc, char **argv) {

	if (argc == 2) {
		writedsk(argv[1],0);
	} else if( (argc==3) && (strcmp(argv[1],"b")==0) ) {
		writedsk(argv[2],4); //Write on side B
	} else {
		fprintf(stderr, "usage: dskwrite [b] <filename>\n");
	}
	return 0;

}

