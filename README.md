README for dsktools Version 0.3
=================================

Introduction
------------

dsktools is a set of tools for reading and writing DSK and EDSK images to real
floppy disks for use with the Amstrad/Schneider CPC range of homecomputers.
The main target platform for dsktools is GNU/Linux.

Current State
-------------

dskwrite is able to write images in standard formats (SYSTEM,DATA) as well as
some special formats with unusual sector numbering and sector sizes and
deleted data (untested). 

dskread is much more complete than in older revisions. It understand some copy
protected formats and takes command line options for selecting drive, side,
number of sides and number of tracks. See dskread -h for a list of known
options.

dskmgmt is aimed to work with CPC DSK images, allowing to get info, list, export and import files (not yet implemented) in a given DSK image file. 

Compiling and Installing
------------------------
Use the typical autotools approach. If no configure script is provided in your tar, generate it with:
  autoreconf
Then, proceed to configure the build system with:
 ./configure
To build the binaries from sources, just type in:
 make
To install the utilities to the /usr/local (by default) location, just type:
 make install

Usage
-----

Straight-forward, really.

./dskread [-d| --drive <drivenum>] [-s|--side <side>] [-t|--tracks <tracks>] [-S|--sides <sides>] <filename>

will read the disk in drive /dev/fd<drivenum> (by default /dev/fd0 if no -d|--drive parameter is provided), assuming that the disk has one side (unless modified with the -S|--sides parameter) and 40 tracks (unless the -t|--tracks parameter is provided). It's also possible to read only one side of a DS disk with the -s|--side parameter.

./dskwrite [-d|--drive <drivenum>] [-s|--side <side>] <filename>

will write the contents of a DSK image stored under <filename> to the disk in the drive /dev/fd<drivenum> (by default /dev/fd0 if no -d|--drive parameter is provided) to the first side of the disk (unless the parameter -s|--side is provided) 

Future
------

Future versions of dskwrite will handle different copy protection schemes,
faithfully reproducing the original disk from the image. I hope to get this
support as complete as it is in CPDWrite under DOS. I will also implement some
command line options to make dsktools more flexible and user friendly.

