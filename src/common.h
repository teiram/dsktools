/* $Id: common.h,v 1.3 2003/08/24 19:40:37 nurgle Exp $
 *
 * common.h - Common functions for dsktools.
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

#ifndef COMMON_H
#define COMMON_H

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

/* These raw floppy commands are missing in fdreg.h. Use with caution.
 */
#define FD_READ_DEL		0xCC	/* read deleted with MT, MFM */
#define FD_WRITE_DEL		0xC9	/* write deleted with MT, MFM */

/* Boolean values
 */
#define	TRUE -1
#define	FALSE 0

/* Various DSK image file and actual disk parameters
 */
#define MAGIC_DISK "MV - CPC"
#define MAGIC_DISK_WRITE "MV - CPCEMU / 27 Dec 01 01:11"
#define MAGIC_EDISK "EXTENDED"
#define	TRACKS 40
#define MAX_TRACKS 82
#define MAX_SIDES 2
#define HEADS 1
#define TRACKLEN_INFO (TRACKLEN + 0x100)

#define MAGIC_TRACK "Track-Info"
#define HEAD 0
#define BPS 2
#define SPT 9
#define GAP 0x4E
#define FILL 0xE5
#define ERR1 0
#define ERR2 0

#define TRACKLEN 0x1200

#define MAX_TRACKLEN 0x2000

#define OFF_IBM 0x01
#define OFF_SYS 0x41
#define OFF_DAT 0xC1


typedef struct diskinfo_t {
	char magic[0x22];
	unsigned char unused1[0x0E];
	unsigned char tracks;
	unsigned char heads;
	unsigned char tracklen[0x02];
	unsigned char tracklenhigh[0xCC];
} Diskinfo;

typedef struct sectorinfo_t {
	unsigned char track;
	unsigned char head;
	unsigned char sector;
	unsigned char bps;
	unsigned char err1;
	unsigned char err2;
	unsigned char unused1;
	unsigned char unused2;
} Sectorinfo;

typedef struct trackinfo_t {
	char magic[0x0D];
	unsigned char unused1[0x03];
	unsigned char track;
	unsigned char head;
	unsigned char unused2[0x02];
	unsigned char bps;
	unsigned char spt;
	unsigned char gap;
	unsigned char fill;
	Sectorinfo sectorinfo[29];
} Trackinfo;

/* format map */
typedef	struct format_map {
	unsigned char cylinder;
	unsigned char head;
	unsigned char sector;
	unsigned char size;
} format_map_t;

void myabort(char *s);

void printdiskinfo(FILE *out, Diskinfo *diskinfo);

void printsectorinfo(FILE *out, Sectorinfo *sectorinfo);

void printtrackinfo(FILE *out, Trackinfo *trackinfo);

void init_sectorinfo(Sectorinfo *sectorinfo, int track, int head, int sector);

/* Initialise a raw FDC command */
void init_raw_cmd(struct floppy_raw_cmd *raw_cmd);

/* Reset FDD */
void reset(int fd);

void init(int fd, int drive);

/* Recalibrate FDD to track 0 */
void recalibrate(int fd, int drive);

#endif /* COMMON_H */

