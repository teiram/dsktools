AM_CFLAGS = -Wall -std=c99 -D_POSIX_C_SOURCE=200809L -O2
AM_LDFLAGS = 

if ENABLE_GUI
SUBDIRS = gui
endif

lib_LTLIBRARIES = libdsk.la
libdsk_la_SOURCES = dsk.c amsdos.c log.c error.c
if LINUX
libdsk_la_SOURCES += linux_fddriver.c 
else 
libdsk_la_SOURCES += null_fddriver.c
endif
include_HEADERS = dsk.h amsdos.h log.h fddriver.h error.h

bin_PROGRAMS = dskread dskwrite dskmgmt
dskread_SOURCES = common.c dskread.c
dskwrite_SOURCES = common.c dskwrite.c
dskmgmt_SOURCES = dskmgmt.c
dskmgmt_LDADD = libdsk.la
dskread_LDADD = libdsk.la
dskwrite_LDADD = libdsk.la

