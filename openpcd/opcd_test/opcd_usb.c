#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>

#include "ausb/ausb.h"
#include "openpcd.h"

#include "opcd_usb.h"

const char *
opcd_hexdump(const void *data, unsigned int len)
{
	static char string[65535];
	unsigned char *d = (unsigned char *) data;
	unsigned int i, left, ofs;

	string[0] = '\0';
	ofs = snprintf(string, sizeof(string)-1, "(%u): ", len);
	
	left = sizeof(string) - ofs;
	for (i = 0; len--; i += 3) {
		if (i >= sizeof(string) -4)
			break;
		snprintf(string+ofs+i, 4, " %02x", *d++);
	}
	string[sizeof(string)-1] = '\0';
	return string;
}

#define OPCD_VENDOR_ID	0x2342
#define OPCD_PRODUCT_ID	0x0001
#define OPCD_OUT_EP	0x01
#define OPCD_IN_EP	0x82
#define OPCD_INT_EP	0x83

static struct usb_device *find_opcd_handle(void)
{
	struct usb_bus *bus;

	for (bus = usb_busses; bus; bus = bus->next) {
		struct usb_device *dev;
		for (dev = bus->devices; dev; dev = dev->next) {
			if (dev->descriptor.idVendor == OPCD_VENDOR_ID
			    && dev->descriptor.idProduct == OPCD_PRODUCT_ID
			    && dev->descriptor.iManufacturer == 0
			    && dev->descriptor.iProduct == 0
			    && dev->descriptor.bNumConfigurations == 1
			    && dev->config->bNumInterfaces == 1
			    && dev->config->iConfiguration == 0)
				return dev;
		}
	}
	return NULL;
}

static void opcd_dump_hdr(struct openpcd_hdr *hdr)
{
	printf("IRQ: cmd=0x%02x, flags=0x%02x, reg=0x%02x, val=0x%02x "
		"len=%d, res=%d\n", hdr->cmd, hdr->flags, hdr->reg,
		hdr->val, hdr->len);
}

static void handle_interrupt(struct usbdevfs_urb *uurb, void *userdata)
{
	struct opcd_handle *od = userdata;
	ausb_dev_handle *ah;
	struct openpcd_hdr *opcdh;

	if (!uurb) {
		fprintf(stderr, "interrupt with no URB?!?\n");
		return;
	}

	ah = uurb->usercontext;

	opcdh = (struct openpcd_hdr *)uurb->buffer;
	opcd_dump_hdr(opcdh);

	if (ausb_submit_urb(ah, uurb))
		fprintf(stderr, "unable to resubmit interupt urb\n");
}

struct opcd_handle *opcd_init(void)
{
	struct opcd_handle *oh;
	struct usb_device *dev;

	oh = malloc(sizeof(*oh));
	if (!oh)
		return NULL;

	memset(oh, 0, sizeof(*oh));

	ausb_init();

	dev = find_opcd_handle();
	if (!dev) {
		fprintf(stderr, "Cannot find OpenPCD device. "
			"Are you sure it is connected?\n");
		exit(1);
	}

	oh->hdl = ausb_open(dev);
	if (!oh->hdl) {
		fprintf(stderr, "Unable to open usb device: %s\n",
			usb_strerror());
		exit(1);
	}

	if (ausb_claim_interface(oh->hdl, 0) < 0) {
		fprintf(stderr, "Unable to claim usb interface "
			"1 of device: %s\n", usb_strerror());
		exit(1);
	}

	ausb_fill_int_urb(&oh->int_urb, OPCD_INT_EP, oh->int_buf, 
			  sizeof(oh->int_buf));
	if (ausb_register_callback(oh->hdl, USBDEVFS_URB_TYPE_INTERRUPT, 
				   handle_interrupt, oh)) {
		fprintf(stderr, "unable to submit interrupt urb");
		exit(1);
	}

	return oh;
}

void opcd_fini(struct opcd_handle *od)
{
	ausb_discard_urb(od->hdl, &od->int_urb);
	ausb_close(od->hdl);
}

int opcd_recv_reply(struct opcd_handle *od, char *buf, int len)
{
	int ret, i;

	memset(buf, 0, sizeof(buf));

	ret = ausb_bulk_read(od->hdl, OPCD_IN_EP, buf, len, 1000);

	if (ret < 0) {
		fprintf(stderr, "bulk_read returns %d(%s)\n", ret,
			usb_strerror());
		return ret;
	}

	printf("RX: %s\n", opcd_hexdump(buf, ret));
	opcd_dump_hdr((struct openpcd_hdr *)buf);

	return ret;
}


int opcd_send_command(struct opcd_handle *od, u_int8_t cmd, 
		     u_int8_t reg, u_int8_t val, u_int16_t len,
		     const unsigned char *data)
{
	unsigned char buf[128];
	struct openpcd_hdr *ohdr = (struct openpcd_hdr *)buf;
	int cur = 0;
	int ret;

	memset(buf, 0, sizeof(buf));

	ohdr->cmd = cmd;
	ohdr->reg = reg;
	ohdr->val = val;
	ohdr->len = len;
	if (data && len)
		memcpy(ohdr->data, data, len);
	
	cur = sizeof(*ohdr) + len;

	printf("TX: %s\n", opcd_hexdump(buf, cur));

	ret = ausb_bulk_write(od->hdl, OPCD_OUT_EP, buf, cur, 0);
	if (ret < 0) {
		fprintf(stderr, "bulk_write returns %d(%s)\n", ret,
			usb_strerror());
		return ret;
	}
}
