/* $Id: dskread.c,v 1.8 2008/06/29 21:36:26 nurgle Exp $
 *
 * dskread.c - Small utility to read CPC disk images from a floppy disk under
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

#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <linux/fd.h>
#include <linux/fdreg.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>

#include "common.h"
#include "log.h"

void rotateleft_sectorids(Trackinfo *trackinfo, int pos) {

	Sectorinfo sectorinfo[29];
	memcpy(sectorinfo, trackinfo->sectorinfo, sizeof(Sectorinfo) * 30);

	int spt = trackinfo->spt;
	for (int i = 0; i < spt; i++) {
		memcpy(&trackinfo->sectorinfo[i],
		       &sectorinfo[(i+pos)%spt], sizeof(Sectorinfo));
	}

}

void rotate_sectorids(Trackinfo *trackinfo) {
	int low = 0xFF;
	int pos = 0;

	/* Find lowest sector number */
	for (int i = 0; i < trackinfo->spt; i++) {
		int sector = trackinfo->sectorinfo[i].sector;
		if (sector < low) {
			low = sector;
			pos = i;
		}
	}

	/* Rotate sectorids in trackinfo left by pos positions */
	rotateleft_sectorids(trackinfo, pos);
}

void seek(int fd, int drive, int track) {

	struct floppy_raw_cmd raw_cmd;
	unsigned char mask = 0xFF;

	init_raw_cmd(&raw_cmd);
	raw_cmd.flags = FD_RAW_INTR;
	raw_cmd.track = track;
	raw_cmd.rate  = 0;
	raw_cmd.length= 0;

	raw_cmd.cmd[raw_cmd.cmd_count++] = FD_SEEK & mask;
	raw_cmd.cmd[raw_cmd.cmd_count++] = drive;
	raw_cmd.cmd[raw_cmd.cmd_count++] = track;

	if (ioctl(fd, FDRAWCMD, &raw_cmd) < 0) {
		LOG(LOG_ERROR, "Seek ioctl error: %s", strerror(errno));
	}
}

#define FD_READTRACK (2|0x040)
#define READ_ID 0x04a
#define READ_DATA 0x046
#define NSECTS 9

char buf[8*1024];
int read_ids(int fd, Trackinfo *trackinfo, int head, int drive) {

	int err;
	struct floppy_raw_cmd cmds[32];
	struct floppy_raw_cmd *cur_cmd;

	unsigned char mask = 0xFF;

	cur_cmd = cmds;

	/* --  detect unformatted track -- */
	/* attempt to read an id and compare the result information
	against what we are expecting for a unformatted track */

	/* initialise this cmd */
	init_raw_cmd(cur_cmd);
	cur_cmd->flags = /*FD_RAW_READ |*/ FD_RAW_INTR;
	cur_cmd->track = trackinfo->track;
	cur_cmd->rate  = 2;	/* SD */
	cur_cmd->length= /*(128<<(trackinfo->bps))*/ 0;
	cur_cmd->cmd[cur_cmd->cmd_count++] = READ_ID & mask;
	cur_cmd->cmd[cur_cmd->cmd_count++] = (head<<2) | drive;
			
	err = ioctl(fd, FDRAWCMD, cmds);

	if ((cur_cmd->reply[0] & 0x0c0)==0x040) {
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

/*		int i;
		for (i=0; i<7; i++)
		{
			printf("%02x ",cur_cmd->reply[i]);
		}
		printf("\r\n");
*/
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

	cur_cmd=cmds;
	init_raw_cmd(cur_cmd);
	cur_cmd->flags = FD_RAW_READ | FD_RAW_INTR;
	cur_cmd->flags |= FD_RAW_MORE;
//	cur_cmd->flags |= FD_RAW_SPIN;

	cur_cmd->data = buf;
	cur_cmd->track = trackinfo->track;
	cur_cmd->rate  = 2;	/* SD */
	cur_cmd->length= 6500;
	cur_cmd->cmd[cur_cmd->cmd_count++] = FD_READTRACK & mask;
	cur_cmd->cmd[cur_cmd->cmd_count++] = (head<<2) | drive;
	cur_cmd->cmd[cur_cmd->cmd_count++] = 0;
	cur_cmd->cmd[cur_cmd->cmd_count++] = 0;
	cur_cmd->cmd[cur_cmd->cmd_count++] = 0;
	cur_cmd->cmd[cur_cmd->cmd_count++] = 0;
	cur_cmd->cmd[cur_cmd->cmd_count++] = 7;
	cur_cmd->cmd[cur_cmd->cmd_count++] = 0x02a;
	cur_cmd->cmd[cur_cmd->cmd_count++] = 0x0ff;

#if 0
	/* synchronises with 2nd sector id on track */
	cur_cmd=cmds;
	init_raw_cmd(cur_cmd);
	cur_cmd->flags = FD_RAW_READ | FD_RAW_INTR;
	cur_cmd->flags |= FD_RAW_MORE;
	cur_cmd->data = buf;
	cur_cmd->track = trackinfo->track;
	cur_cmd->rate  = 2;	/* SD */
	cur_cmd->length= (128<<(trackinfo->bps));
	cur_cmd->cmd[cur_cmd->cmd_count++] = FD_READTRACK & mask;
	cur_cmd->cmd[cur_cmd->cmd_count++] = (head<<2) | drive;
	cur_cmd->cmd[cur_cmd->cmd_count++] = 0;
	cur_cmd->cmd[cur_cmd->cmd_count++] = 0;
	cur_cmd->cmd[cur_cmd->cmd_count++] = 0;
	cur_cmd->cmd[cur_cmd->cmd_count++] = 0;
	cur_cmd->cmd[cur_cmd->cmd_count++] = 1;
	cur_cmd->cmd[cur_cmd->cmd_count++] = 0x02a;
	cur_cmd->cmd[cur_cmd->cmd_count++] = 0x0ff;
#endif
#if 0
	/* synchronises with 1st sector id on track */
	/* attempt to read a non-existant sector */
	cur_cmd=cmds;
	init_raw_cmd(cur_cmd);
	cur_cmd->flags = FD_RAW_READ | FD_RAW_INTR;
	cur_cmd->flags |= FD_RAW_SPIN;
	cur_cmd->flags |= FD_RAW_MORE;
	cur_cmd->data = buf;
	cur_cmd->track = trackinfo->track;
	cur_cmd->rate  = 2;	/* SD */
	cur_cmd->length= (128<<(trackinfo->bps));
	cur_cmd->cmd[cur_cmd->cmd_count++] = FD_READ & mask;
	cur_cmd->cmd[cur_cmd->cmd_count++] = (head<<2) | drive;
	cur_cmd->cmd[cur_cmd->cmd_count++] = 0;
	cur_cmd->cmd[cur_cmd->cmd_count++] = 0;
	cur_cmd->cmd[cur_cmd->cmd_count++] = 0x0ca;
	cur_cmd->cmd[cur_cmd->cmd_count++] = 2;
	cur_cmd->cmd[cur_cmd->cmd_count++] = 0x0ca;
	cur_cmd->cmd[cur_cmd->cmd_count++] = 0x02a;
	cur_cmd->cmd[cur_cmd->cmd_count++] = 0x0ff;
#endif

	/* initialise the read id command list */
	for (int i = 1; i < 32; i++) {
		cur_cmd = &cmds[i];

		/* initialise this cmd */
		init_raw_cmd(cur_cmd);
		cur_cmd->flags = /*FD_RAW_READ |*/ FD_RAW_INTR;
		if (i != (32 - 1)) {
			cur_cmd->flags |= FD_RAW_MORE;
		}
		cur_cmd->track = trackinfo->track;
		cur_cmd->rate  = 2;	/* SD */
		cur_cmd->length= 0; /*(128<<(trackinfo->bps));*/
		cur_cmd->cmd[cur_cmd->cmd_count++] = READ_ID & mask;
		cur_cmd->cmd[cur_cmd->cmd_count++] = (head<<2) | drive;
	}		
	
	err = ioctl(fd, FDRAWCMD, cmds);

	if (err < 0) {
		LOG(LOG_ERROR, "Error reading id: %s", strerror(errno));
		exit(1);
	}

/*	cur_cmd = cmds;
	for (i=0; i<7; i++)
	{
		printf("%02x\r\n",cur_cmd->reply[i]);
	}
*/	

	trackinfo->spt = NSECTS;
	for (int i = 1; i < NSECTS +1; i++) {
		cur_cmd = &cmds[i];
		trackinfo->sectorinfo[i - 1].track = cur_cmd->reply[3];
		trackinfo->sectorinfo[i - 1].head = cur_cmd->reply[4];
		trackinfo->sectorinfo[i - 1].sector = cur_cmd->reply[5];
		trackinfo->sectorinfo[i - 1].bps = cur_cmd->reply[6];
	}

	

//	rotate_sectorids( trackinfo );

	/* need to calculate number of sectors differently */	
	return NSECTS;
}

/* standard FD_READ causes problems and is slower! */

void read_sect(int fd, Trackinfo *trackinfo, Sectorinfo *sectorinfo,
	       unsigned char *data, int track, int head, int drive) {

	int err, retry=0, ok=0;
	struct floppy_raw_cmd raw_cmd;
	unsigned char mask = 0xFF;

//	reset(fd);

	do {
		init_raw_cmd(&raw_cmd);
		raw_cmd.flags = FD_RAW_READ | FD_RAW_INTR;
		raw_cmd.track = track;
		raw_cmd.rate  = 2;	/* SD */
		raw_cmd.length= (128<<(sectorinfo->bps));
		raw_cmd.data  = data;
		raw_cmd.cmd_count = 0;
		raw_cmd.cmd[raw_cmd.cmd_count++] = READ_DATA & mask;
		raw_cmd.cmd[raw_cmd.cmd_count++] = (head<<2) | drive;	/* head */
		raw_cmd.cmd[raw_cmd.cmd_count++] = sectorinfo->track;	/* track */
		raw_cmd.cmd[raw_cmd.cmd_count++] = sectorinfo->head;	/* head */	
		raw_cmd.cmd[raw_cmd.cmd_count++] = sectorinfo->sector;	/* sector */
		raw_cmd.cmd[raw_cmd.cmd_count++] = sectorinfo->bps;	/* sectorsize */
		raw_cmd.cmd[raw_cmd.cmd_count++] = sectorinfo->sector;	/* sector */
		raw_cmd.cmd[raw_cmd.cmd_count++] = trackinfo->gap;	/* GPL */
		raw_cmd.cmd[raw_cmd.cmd_count++] = 0xFF;		/* DTL */
	
		err = ioctl(fd, FDRAWCMD, &raw_cmd);
		if (err < 0) {
			LOG(LOG_ERROR, "Error reading: %s", strerror(errno));
			exit(1);
		}

		if (((raw_cmd.reply[0] & 0x0f8) == 0x040) && 
		    (raw_cmd.reply[1] == 0x080)) {
			/* end of cylinder */
			return;
		}

		if (raw_cmd.reply[0] & 0x40) {
			recalibrate(fd, drive);
			retry++;
			fprintf(stderr,"TRY %d \n", retry);
		}
		else ok = 1; // Read ok, go to next
	} while ((retry < 10) && (ok == 0));

	if (!ok) {
		printf("\n%02x %02x %02x\r\n",raw_cmd.reply[0],raw_cmd.reply[1], raw_cmd.reply[2]);
		LOG(LOG_ERROR, "Could not read sector %0X\n",
		    sectorinfo->sector);
	}
}

void init_trackinfo( Trackinfo *trackinfo, int track, int side ) {

	memset(trackinfo, 0, sizeof(*trackinfo));

	strncpy(trackinfo->magic, MAGIC_TRACK, sizeof(trackinfo->magic));
	//unsigned char unused1[0x03];
	trackinfo->track = track;
	trackinfo->head = side;
	//unsigned char unused2[0x02];
	trackinfo->bps = 2;
	trackinfo->spt = 0;
	trackinfo->gap = 82;
	trackinfo->fill = 0xFF;
	//trackinfo->sectorinfo[29];
//	for ( i=0; i<9; i++ ) {
//		init_sectorinfo( &trackinfo->sectorinfo[i], track, 0, 0xC1+i );
//	}

}

void init_diskinfo( Diskinfo *diskinfo, int tracks, int heads, int tracklen ) {

	memset(diskinfo, 0, sizeof(*diskinfo));

	strncpy(diskinfo->magic, MAGIC_DISK_WRITE, sizeof(diskinfo->magic));
	diskinfo->tracks = tracks;
	diskinfo->heads = heads;
	diskinfo->tracklen[0] = (char) tracklen;
	diskinfo->tracklen[1] = (char) (tracklen >> 8);
	//unsigned char tracklenhigh[0xCC];

}

void timestamp_diskinfo( Diskinfo *diskinfo ) {

	time_t t;
	struct tm *ltime;

	t = time(NULL);
	ltime = localtime(&t);
	/* FIXME: Can the formatting be messed up by locale settings? */
	strftime(diskinfo->magic+14, 16, "%d %b %g %H:%M", ltime);

}

void readdsk(char *filename, int drv, int startside, 
	     int nsides, int ntracks) {

	/* Variable declarations */
	int fd;
	char drive[32];
	Diskinfo diskinfo;
	Trackinfo trackinfo[MAX_TRACKS*MAX_SIDES];
	Sectorinfo *sectorinfo;
	unsigned char data[TRACKLEN*TRACKS], *sect, *track;
	int tracklen;
	FILE *file;
	int count;

	/* initialization */
	sprintf(drive,"/dev/fd%01d",drv);

	/* open drive */
	fd = open(drive, O_ACCMODE | O_NDELAY);
	if (fd < 0) {
		LOG(LOG_ERROR, "Error opening floppy device: %s",
		    strerror(errno));
		exit(1);
	}

	LOG(LOG_DEBUG, "Reading DSK to %s", filename);

	/* open file */
	file = fopen(filename, "w");
	if (file == NULL) {
		LOG(LOG_ERROR, "Error opening image file: %s",
		    strerror(errno));
		exit(1);
	}

	init( fd, drv);

	sect = data;
	for (int i=0; i < ntracks; i++) {
		for (int k = 0; k < nsides; k++) {
			int spt;
			int side;
			int ntrk;

			ntrk = (i * nsides) + k;
			side = (startside + k) % MAX_SIDES;

			init_trackinfo(&trackinfo[ntrk], i, k);
			printtrackinfo(stderr, &trackinfo[ntrk]);
			fprintf(stderr, "\n");
			fprintf(stderr, " [");

			seek(fd, drv, i);
			spt = read_ids(fd, &trackinfo[ntrk],side,drv);
			/* Slow version: Read sectors in order */
		
			trackinfo->spt = spt;
			for (int j = 0; j < spt; j++) {
				sectorinfo = &trackinfo[ntrk].sectorinfo[j];
				fprintf(stderr, "%02X ", sectorinfo->sector);
				read_sect(fd, &trackinfo[ntrk], sectorinfo,
					  sect, i, side, drv);
				sect += (128 << trackinfo[ntrk].bps);
			}
#if 0
			trackinfo->spt = spt;
			/* Fast version: Read sectors interleaved in two passes */
			for (int j = 0; j < spt; j += 2 ) {
				sectorinfo = &trackinfo[i].sectorinfo[j];
				fprintf(stderr, "%02X ", sectorinfo->sector);
				read_sect(fd, &trackinfo[i], sectorinfo, 
					  sect, side, drv);
				sect += 0x400;
			}
			sect = sect - (0x400 * j / 2) + 0x200;
			for (int j = 1; j < spt; j += 2) {
				sectorinfo = &trackinfo[i].sectorinfo[j];
				fprintf(stderr, "%02X ", sectorinfo->sector);
				read_sect(fd, &trackinfo[i], sectorinfo, 
					  sect, side, drv);
				sect += 0x400;
			}
#endif
			fprintf(stderr, "]\n");
		}
	}

	init_diskinfo(&diskinfo, ntracks, nsides, TRACKLEN_INFO);
	timestamp_diskinfo(&diskinfo);
	printdiskinfo(stderr, &diskinfo);

	count = fwrite(&diskinfo, 1, sizeof(diskinfo), file);
	if (count != sizeof(diskinfo)) {
		myabort("Error writing Disk-Info: File too short\n");
	}

	track = data;
	tracklen = TRACKLEN;
	for (int i=0; i < diskinfo.tracks; i++)	{
		for (int j=0; j<diskinfo.heads; j++) {
			int ninfo = (i * diskinfo.heads) + j;
			count = fwrite(&trackinfo[ninfo], 1, 
				       sizeof(trackinfo[ninfo]), file);
			if (count != sizeof(trackinfo[ninfo])) {
				myabort("Error writing Track-Info: File too short\n");
			}
			count = fwrite(track, 1, tracklen, file);
			if (count != tracklen) {
				myabort("Error writing Track: File too short\n");
			}
		}
		track += tracklen;
	}
	fclose(file);
}

void help_exit(int exitcode) {
	fprintf(stderr, "usage: dskread [options] <filename>\n");
	fprintf(stderr, "options: -d | --drive <drive>    select drive\n");
	fprintf(stderr, "         -s | --side <side>      select side\n");
	fprintf(stderr, "         -S | --sides <sides>    number of sides\n");
	fprintf(stderr, "         -t | --tracks <tracks>  number of tracks\n");
	fprintf(stderr, "         -h                      this help\n");
	exit(exitcode);
}

int main(int argc, char **argv) {

	static struct option long_options[] = {
		{"drive", 1, 0, 'd'},
		{"side", 1, 0, 's'},
		{"sides", 1, 0, 'S'},
		{"tracks", 1, 0, 't'},
		{"help", 0, 0, 'h'},
		{0, 0, 0, 0}
	};
	int c;
	char *drive_string = NULL;
	char *side_string = NULL;
	char *sides_string = NULL;
	char *tracks_string = NULL;
	int drive = 0;
	char side = 0;
	char sides = 1;
	char tracks = 40;

	do {
		int option_index = 0;
		c = getopt_long(argc, argv, "d:s:S:t:h",
				long_options, &option_index);
		switch(c) {
		case 'h':
		case '?':
			help_exit(0);
			break;
		case 'd':
			drive_string = optarg;
			break;
		case 's':
			side_string = optarg;
			break;
		case 'S':
			sides_string = optarg;
			break;
		case 't':
			tracks_string = optarg;
			break;
		}
	} while (c != -1);

	if (argc - optind != 1) {
		help_exit(1);
	}

	if (drive_string != NULL) drive = atoi(drive_string);
	if (side_string != NULL) side = atoi(side_string);
	if (sides_string != NULL) sides = atoi(sides_string);
	if (tracks_string != NULL) tracks = atoi(tracks_string);

	readdsk(argv[optind], drive, side, sides, tracks);

	return 0;

}

