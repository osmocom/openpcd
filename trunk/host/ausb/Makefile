#include ../../makevars

OBJS=ausb.o usb.o
CFLAGS+=-fPIC

all: libausb.a libausb.so

libausb.a: $(OBJS)
	$(AR) r $@ $^

libausb.so: $(OBJS)
	$(LD) -x --shared -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $^

ausb_test: ausb_test.o ausb.o
	$(CC) $(CFLAGS) -lusb -o $@ $^

clean:
	@rm -f *.o ausb_test libusb.a libausb.a libausb.so
