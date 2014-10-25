#ifndef _REQ_CTX_H
#define _REQ_CTX_H

#define RCTX_SIZE_LARGE	1000
#define RCTX_SIZE_SMALL	270

#define MAX_HDRSIZE	sizeof(struct openpcd_hdr)
#define MAX_REQSIZE	(64-MAX_HDRSIZE)

#define req_buf_payload(x)	(x->data[x->hdr_len])
#define req_buf_hdr(x)		(x->data[0])

#include <sys/types.h>
#include <lib_AT91SAM7.h>

struct req_ctx {
	volatile u_int32_t state;
	u_int16_t size;
	u_int16_t tot_len;
	u_int8_t *data;
};

#define RCTX_STATE_FREE                 0
#define RCTX_STATE_UDP_RCV_BUSY         1
#define RCTX_STATE_UDP_RCV_DONE         2
#define RCTX_STATE_MAIN_PROCESSING      3
#define RCTX_STATE_RC632IRQ_BUSY        4
#define RCTX_STATE_UDP_EP2_PENDING      5
#define RCTX_STATE_UDP_EP2_BUSY         6
#define RCTX_STATE_UDP_EP3_PENDING      7
#define RCTX_STATE_UDP_EP3_BUSY         8
#define RCTX_STATE_SSC_RX_BUSY          9
#define RCTX_STATE_LIBRFID_BUSY        10
#define RCTX_STATE_PIOIRQ_BUSY         11
#define RCTX_STATE_INVALID             12
// Nominally UNUSED states
#define RCTX_STATE_UDP_EP0_PENDING     13
#define RCTX_STATE_UDP_EP0_BUSY        14
#define RCTX_STATE_UDP_EP1_PENDING     15
#define RCTX_STATE_UDP_EP1_BUSY        16
// Count of the number of STATES
#define RCTX_STATE_COUNT               17

extern struct req_ctx __ramfunc *req_ctx_find_get(int large, unsigned long old_state, unsigned long new_state);
extern struct req_ctx *req_ctx_find_busy(void);
extern void req_ctx_set_state(struct req_ctx *ctx, unsigned long new_state);
extern void req_ctx_put(struct req_ctx *ctx);
extern u_int8_t req_ctx_num(struct req_ctx *ctx);

#endif /* _REQ_CTX_H */
