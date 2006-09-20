#ifndef _OPCD_USB_H
#define _OPCD_USB_H

#include <lib_AT91SAM7.h>
#include <sys/types.h>
#include <asm/atomic.h>
#include "openpcd.h"
#include <dfu/dfu.h>

struct req_ctx;

extern void udp_open(void);
extern int udp_refill_ep(int ep);
extern void udp_unthrottle(void);
extern void udp_reset(void);

struct ep_ctx {
	atomic_t pkts_in_transit;
	struct {
		struct req_ctx *rctx;
		unsigned int bytes_sent;
	} incomplete;
};

struct udp_pcd {
	AT91PS_UDP pUdp;
	unsigned char cur_config;
	unsigned int  cur_rcv_bank;
	struct ep_ctx ep[4];
};

/* USB standard request code */

#define STD_GET_STATUS_ZERO           0x0080
#define STD_GET_STATUS_INTERFACE      0x0081
#define STD_GET_STATUS_ENDPOINT       0x0082

#define STD_CLEAR_FEATURE_ZERO        0x0100
#define STD_CLEAR_FEATURE_INTERFACE   0x0101
#define STD_CLEAR_FEATURE_ENDPOINT    0x0102

#define STD_SET_FEATURE_ZERO          0x0300
#define STD_SET_FEATURE_INTERFACE     0x0301
#define STD_SET_FEATURE_ENDPOINT      0x0302

#define STD_SET_ADDRESS               0x0500
#define STD_GET_DESCRIPTOR            0x0680
#define STD_SET_DESCRIPTOR            0x0700
#define STD_GET_CONFIGURATION         0x0880
#define STD_SET_CONFIGURATION         0x0900
#define STD_GET_INTERFACE             0x0A81
#define STD_SET_INTERFACE             0x0B01
#define STD_SYNCH_FRAME               0x0C82

#define MIN(a, b) (((a) < (b)) ? (a) : (b))


#endif 

