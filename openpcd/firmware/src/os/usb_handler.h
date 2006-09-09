#ifndef _USB_HANDLER_H
#define _USB_HANDLER_H

#include "openpcd.h"
#include <os/req_ctx.h>

#define MAX_PAYLOAD_LEN	(64 - sizeof(struct openpcd_hdr))

typedef int usb_cmd_fn(struct req_ctx *rctx);

extern int usb_hdlr_register(usb_cmd_fn *hdlr, u_int8_t class);
extern void usb_hdlr_unregister(u_int8_t class);

extern void usb_in_process(void);
extern void usb_out_process(void);

#endif
