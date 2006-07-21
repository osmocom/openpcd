#ifndef _OPENPCD_H
#define _OPENPCD_H

#ifdef OLIMEX
#define OPENPCD_LED1	AT91C_PIO_PA18
#define OPENPCD_LED2	AT91C_PIO_PA17
#else
#define OPENPCD_LED1	AT91C_PIO_PA25
#define OPENPCD_LED2	AT91C_PIO_PA26
#endif

#define OPENPCD_RC632_IRQ	AT91C_ID_IRQ1
#define OPENPCD_RC632_RESET	AT91C_PIO_PA29

#define MAX_REQSIZE	256
#define MAX_HDRSIZE	8

#define req_buf_payload(x)	(x->data[x->hdr_len])
#define req_buf_hdr(x)		(x->data[0])

#include <include/types.h>

struct req_buf {
	u_int16_t hdr_len;
	u_int16_t tot_len;
	u_int8_t data[MAX_REQSIZE+MAX_HDRSIZE];
};

struct req_ctx {
	u_int16_t seq;		/* request sequence number */

	u_int32_t flags;

	struct req_buf rx;
	struct req_buf tx;
};

#endif /* _OPENPCD_H */
