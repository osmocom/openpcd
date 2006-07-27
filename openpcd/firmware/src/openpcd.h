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

#define OPENPCD_PIO_MFIN	AT91C_PIO_PA17
#define OPENPCD_PIO_MFOUT	AT91C_PIO_PA18
#define OPENPCD_PIO_RC632_RESET	AT91C_PIO_PA29
#define OPENPCD_PIO_TRIGGER	AT91C_PIO_PA31

#define OPENPCD_IRQ_PRIO_SPI	AT91C_AIC_PRIOR_HIGHEST
#define OPENPCD_IRQ_PRIO_UDP	(AT91C_AIC_PRIOR_LOWEST+1)
#define OPENPCD_IRQ_PRIO_RC632	AT91C_AIC_PRIOR_LOWEST

#define MAX_REQSIZE	256
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
	//u_int16_t seq;		/* request sequence number */

	u_int32_t flags;

	struct req_buf rx;
	struct req_buf tx;
};

#define NUM_REQ_CTX	8
extern struct req_ctx *req_ctx_find_get(void);
extern struct req_ctx *req_ctx_find_busy(void);
extern void req_ctx_put(struct req_ctx *ctx);
extern u_int8_t req_ctx_num(struct req_ctx *ctx);

extern void _init_func(void);
extern void _main_func(void);

#endif /* _OPENPCD_H */
