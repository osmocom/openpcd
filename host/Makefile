#!/usr/bin/make
LDFLAGS=-lusb -lcrypt #-lzebvty -Lzebvty/
CFLAGS=-Wall -I../firmware/include

all: opcd_presence opcd_test opcd_sh

clean:
	-rm -f *.o opcd_test
	$(MAKE) -C ausb clean

ausb/libausb.a:
	$(MAKE) -C ausb libausb.a

opcd_presence: opcd_presence.o opcd_usb.o ausb/libausb.a
	$(CC) $(LDFLAGS) -L/usr/lib -lcurl -lidn -lssl -lcrypto -ldl -lz -o $@ $^

opcd_test: opcd_test.o opcd_usb.o ausb/libausb.a
	$(CC) $(LDFLAGS) -o $@ $^

opcd_sh: opcd_sh.o opcd_usb.o ausb/libausb.a zebvty/libzebvty.a
	$(CC) $(LDFLAGS) -o $@ $^
	

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

.PHONEY: all clean
