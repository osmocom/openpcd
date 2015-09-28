#ifndef _USB_HANDLER_H
#define _USB_HANDLER_H

#include "openpcd.h"
#include <os/req_ctx.h>

#define MAX_PAYLOAD_LEN	(64 - sizeof(struct openpcd_hdr))

#define USB_RET_RESPOND		(1 << 8)
#define USB_RET_ERR		(2 << 8)
#define USB_ERR(x)	(USB_RET_RESPOND|USB_RET_ERR|(x & 0xff))

enum usbapi_err {
	USB_ERR_NONE,
	USB_ERR_CMD_UNKNOWN,
	USB_ERR_CMD_NOT_IMPL,
};

typedef int usb_cmd_fn(struct req_ctx *rctx);

extern int usb_hdlr_register(usb_cmd_fn *hdlr, uint8_t class);
extern void usb_hdlr_unregister(uint8_t class);

extern void usb_in_process(void);
extern void usb_out_process(void);

#endif
