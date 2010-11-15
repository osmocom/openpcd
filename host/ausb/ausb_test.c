#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "ausb.h"

#define CJPPA_USB_VENDOR_ID	0x0c4b
#define CJPPA_USB_DEVICE_ID	0x0100

static struct usb_device *find_cj_usbdev(int num)
{
	struct usb_bus *busses, *bus;
	struct usb_device *dev;
	int found = 0;

	busses = usb_get_busses();

	for (bus = busses; bus; bus = bus->next) {
		for (dev = bus->devices; dev; dev = dev->next) {
			if (dev->descriptor.idVendor == CJPPA_USB_VENDOR_ID &&
			    dev->descriptor.idProduct == CJPPA_USB_DEVICE_ID) {
				found++;
				if (found == num)
					return dev;
			}
		}
	}
	return NULL;
}

static void int_cb(struct usbdevfs_urb *uurb)
{
	struct ausb_dev_handle *ah = uurb->usercontext;

	fprintf(stdout, "int_cb() called, ");

	ausb_dump_urb(uurb);

	if (ausb_submit_urb(ah, uurb))
		fprintf(stderr, "unable to resubmit urb\n");
}

int main(int argc, char **argv)
{
	struct usb_device *dev;
	struct ausb_dev_handle *ah;
	struct usbdevfs_urb *uurb;
	char buffer[280];
	ausb_init();
	
	uurb = malloc(sizeof(*uurb));

	dev = find_cj_usbdev(1);

	if (!dev) {
		fprintf(stderr, "unable to find matching usb device\n");
		exit(1);
	}

	ah = ausb_open(dev);
	if (!ah) {
		fprintf(stderr, "error while opening usb device\n");
		exit(1);
	}

	if (ausb_claim_interface(ah, 0)) {
		fprintf(stderr, "unable to claim interface\n");
		ausb_close(ah);
		exit(1);
	}

	if (usb_set_configuration(ah->uh, 1)) {
		fprintf(stderr, "unable to set configuration 1\n");
		ausb_close(ah);
		exit(1);
	}

#if 1
	ausb_fill_int_urb(uurb, 0x81, buffer, sizeof(buffer));
	if (ausb_submit_urb(ah, uurb)) {
		fprintf(stderr, "unable to submit urb\n");
		ausb_close(ah);
		exit(1);
	}

	while (1) {
		sleep(10);
	}
#endif

	ausb_close(ah);

	exit(0);
}
