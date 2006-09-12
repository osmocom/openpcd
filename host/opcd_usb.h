#ifndef _OPCD_USB_H
#define _OPCD_USB_H

#include "ausb/ausb.h"

#define OPCD_INTBUF_SIZE 64
struct opcd_handle {
	struct ausb_dev_handle *hdl;
	struct usbdevfs_urb int_urb;
	u_int8_t int_buf[OPCD_INTBUF_SIZE];
};

extern const char *opcd_hexdump(const void *data, unsigned int len);

extern struct opcd_handle *opcd_init(void);
extern void opcd_fini(struct opcd_handle *od);

extern int opcd_recv_reply(struct opcd_handle *od, char *buf, int len);
extern int opcd_send_command(struct opcd_handle *od, u_int8_t cmd, 
			     u_int8_t reg, u_int8_t val, u_int16_t len,
			     const unsigned char *data);
extern int opcd_usbperf(struct opcd_handle *od, unsigned int frames);

#endif
