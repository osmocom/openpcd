#!/usr/bin/make
LDFLAGS=-lusb

all: opcd_test

clean:
	-rm -f *.o opcd_test
	$(MAKE) -C ausb clean

ausb/libausb.a:
	$(MAKE) -C ausb libausb.a

opcd_test: opcd_test.o opcd_usb.o ausb/libausb.a
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

.PHONEY: all clean
