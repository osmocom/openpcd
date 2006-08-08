#ifndef _OPENPCD_H
#define _OPENPCD_H

#ifdef OLIMEX
#define OPENPCD_PIO_LED2	AT91C_PIO_PA17
#define OPENPCD_PIO_LED1	AT91C_PIO_PA18
#define OPENPCD_PIO_UDP_PUP	AT91C_PIO_PA25
#else
#define OPENPCD_PIO_UDP_CNX	AT91C_PIO_PA15
#define OPENPCD_PIO_UDP_PUP	AT91C_PIO_PA16
#define OPENPCD_PIO_LED1	AT91C_PIO_PA25
#define OPENPCD_PIO_LED2	AT91C_PIO_PA26
#endif

#define OPENPCD_IRQ_RC632	AT91C_ID_IRQ1

#define OPENPCD_PIO_CDIV_HELP_OUT	AT91C_PA0_TIOA0
#define OPENPCD_PIO_CDIV_HELP_IN	AT91C_PA29_TCLK2
#define OPENPCD_PIO_MFIN_PWM		AT91C_PA23_PWM0
#define OPENPCD_PIO_CARRIER_DIV_OUT	AT91C_PA1_TIOB0
#define OPENPCD_PIO_MFIN_SSC_TX		AT91C_PA17_TD
#define OPENPCD_PIO_MFOUT_SSC_RX	AT91C_PA18_RD
#define OPENPCD_PIO_SSP_CKIN		AT91C_PA19_RK
#define OPENPCD_PIO_RC632_RESET		AT91C_PIO_PA5
#define OPENPCD_PIO_TRIGGER		AT91C_PIO_PA31
#define OPENPCD_PIO_CARRIER_IN		AT91C_PA28_TCLK1

#define OPENPCD_IRQ_PRIO_SPI	AT91C_AIC_PRIOR_HIGHEST
#define OPENPCD_IRQ_PRIO_SSC	(AT91C_AIC_PRIOR_HIGHEST-1)
#define OPENPCD_IRQ_PRIO_UDP	(AT91C_AIC_PRIOR_LOWEST+1)
#define OPENPCD_IRQ_PRIO_RC632	AT91C_AIC_PRIOR_LOWEST

#define MAX_REQSIZE	64
#define MAX_HDRSIZE	8

#define req_buf_payload(x)	(x->data[x->hdr_len])
#define req_buf_hdr(x)		(x->data[0])

#include <sys/types.h>

struct req_buf {
	u_int16_t hdr_len;
	u_int16_t tot_len;
	u_int8_t data[MAX_REQSIZE+MAX_HDRSIZE];
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

extern void _init_func(void);
extern void _main_func(void);

#endif /* _OPENPCD_H */
