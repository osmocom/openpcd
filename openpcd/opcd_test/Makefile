#!/usr/bin/make
LDFLAGS=-lusb

all: opcd_test

clean:
	-rm -f *.o opcd_test

opcd_test: opcd_test.o
	$(CC) $(LDFLAGS) -o $@ $<

opcd_test.o: opcd_test.c
	$(CC) $(CFLAGS) -o $@ -c $<

.PHONEY: all clean
