#$Id: Makefile,v 1.7 2008/06/25 08:19:10 pulkomandy Exp $

# build targets

all:	dskwrite dskread dskmgmt

clean:
	rm -f dskread dskwrite *.o *~

# edit and debug targets

gvim:
	gvim *.c *.h Makefile ChangeLog TODO README AUTHORS

tr:
	time ./dskread x.dsk

tw:
	time ./dskwrite x.dsk

# dependencies

dskread: dskread.c common.o
	gcc -g -o dskread dskread.c common.o

dskwrite: dskwrite.c common.o
	gcc -g -o dskwrite dskwrite.c common.o

common.o: common.c
	gcc -g -c common.c

dsk.o: dsk.c dsk.h
	gcc -g -c dsk.c

dskmgmt: dskmgmt.c dsk.o
	gcc -g -o dskmgmt dskmgmt.c dsk.o

# installation
install:
	cp dskwrite dskread dskmgmt /usr/local/bin
