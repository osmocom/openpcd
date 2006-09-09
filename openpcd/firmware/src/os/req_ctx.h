#ifndef _REQ_CTX_H
#define _REQ_CTX_H

#define MAX_HDRSIZE	sizeof(struct openpcd_hdr)
#define MAX_REQSIZE	(64-MAX_HDRSIZE)

#define req_buf_payload(x)	(x->data[x->hdr_len])
#define req_buf_hdr(x)		(x->data[0])

#include <sys/types.h>

struct req_buf {
	u_int16_t hdr_len;
	u_int16_t tot_len;
	u_int8_t data[64];
};

struct req_ctx {
	u_int16_t seq;		/* request sequence number */
	u_int16_t flags;
	volatile u_int32_t state;
	struct req_buf rx;
	struct req_buf tx;
};

#define RCTX_STATE_FREE			0x00
#define RCTX_STATE_UDP_RCV_BUSY		0x01
#define RCTX_STATE_UDP_RCV_DONE		0x02
#define RCTX_STATE_MAIN_PROCESSING	0x03
#define RCTX_STATE_RC632IRQ_BUSY	0x04

#define RCTX_STATE_UDP_EP2_PENDING	0x10
#define RCTX_STATE_UDP_EP2_BUSY		0x11

#define RCTX_STATE_UDP_EP3_PENDING	0x12
#define RCTX_STATE_UDP_EP3_BUSY		0x13

#define RCTX_STATE_SSC_RX_BUSY		0x20

#define NUM_REQ_CTX	8
extern struct req_ctx *req_ctx_find_get(unsigned long old_state, unsigned long new_state);
extern struct req_ctx *req_ctx_find_busy(void);
extern void req_ctx_set_state(struct req_ctx *ctx, unsigned long new_state);
extern void req_ctx_put(struct req_ctx *ctx);
extern u_int8_t req_ctx_num(struct req_ctx *ctx);

#endif /* _REQ_CTX_H */
