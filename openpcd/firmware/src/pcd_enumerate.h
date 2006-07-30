#ifndef _OPCD_USB_H
#define _OPCD_USB_H

#include <sys/types.h>
#include "src/openpcd.h"

extern void udp_init(void);
extern int udp_refill_ep(int ep, struct req_ctx *rctx);
extern void udp_unthrottle(void);

#endif 

