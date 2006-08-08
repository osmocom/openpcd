/* OpenPCD USB handler - handle incoming USB requests on OUT pipe
 * (C) 2006 by Harald Welte
 */

#include <sys/types.h>
#include <errno.h>
#include <string.h>

#include <openpcd.h>

#include "pcd_enumerate.h"
#include "openpcd.h"
#include "rc632.h"
#include "led.h"
#include "dbgu.h"

#define MAX_PAYLOAD_LEN	(64 - sizeof(struct openpcd_hdr))

static int usb_in(struct req_ctx *rctx)
{
	struct openpcd_hdr *poh = (struct openpcd_hdr *) &rctx->rx.data[0];
	struct openpcd_hdr *pih = (struct openpcd_hdr *) &rctx->tx.data[0];
	u_int16_t len = rctx->rx.tot_len;

	DEBUGP("usb_in ");

	if (len < sizeof(*poh))
		return -EINVAL;

	memcpy(pih, poh, sizeof(*poh));
	rctx->tx.tot_len = sizeof(*poh);

	switch (poh->cmd) {
	case OPENPCD_CMD_READ_REG:
		rc632_reg_read(RAH, poh->reg, &pih->val);
		DEBUGP("READ REG(0x%02x)=0x%02x ", poh->reg, pih->val);
		goto respond;
		break;
	case OPENPCD_CMD_READ_FIFO:
		{
		u_int16_t tot_len = poh->val, remain_len;
		if (tot_len > MAX_PAYLOAD_LEN) {
			pih->len = MAX_PAYLOAD_LEN;
			remain_len -= pih->len;
			rc632_fifo_read(RAH, pih->len, pih->data);
			rctx->tx.tot_len += pih->len;
			DEBUGP("READ FIFO(len=%u)=%s ", pih->len,
				hexdump(pih->data, poh->val));
			req_ctx_set_state(rctx, RCTX_STATE_UDP_EP2_PENDING);
			udp_refill_ep(2, rctx);

			/* get and initialize second rctx */
			rctx = req_ctx_find_get(RCTX_STATE_FREE,
						RCTX_STATE_MAIN_PROCESSING);
			if (!rctx) {
				DEBUGPCRF("FATAL_NO_RCTX!!!\n");
				break;
			}
			poh = (struct openpcd_hdr *) &rctx->rx.data[0];
			pih = (struct openpcd_hdr *) &rctx->tx.data[0];
			memcpy(pih, poh, sizeof(*poh));
			rctx->tx.tot_len = sizeof(*poh);

			pih->len = remain_len;
			rc632_fifo_read(RAH, pih->len, pih->data);
			rctx->tx.tot_len += pih->len;
			DEBUGP("READ FIFO(len=%u)=%s ", pih->len,
				hexdump(pih->data, poh->val));
			/* don't set state of second rctx, main function
			 * body will do this after switch statement */
		} else {
			pih->len = poh->val;
			rc632_fifo_read(RAH, poh->val, pih->data);
			rctx->tx.tot_len += pih->len;
			DEBUGP("READ FIFO(len=%u)=%s ", poh->val,
				hexdump(pih->data, poh->val));
		}
		goto respond;
		break;
		}
	case OPENPCD_CMD_WRITE_REG:
		DEBUGP("WRITE_REG(0x%02x, 0x%02x) ", poh->reg, poh->val);
		rc632_reg_write(RAH, poh->reg, poh->val);
		break;
	case OPENPCD_CMD_WRITE_FIFO:
		DEBUGP("WRITE FIFO(len=%u) ", poh->len);
		if (len - sizeof(*poh) < poh->len)
			return -EINVAL;
		rc632_fifo_write(RAH, poh->len, poh->data, 0);
		break;
	case OPENPCD_CMD_READ_VFIFO:
		DEBUGP("READ VFIFO ");
		DEBUGP("NOT IMPLEMENTED YET ");
		goto respond;
		break;
	case OPENPCD_CMD_WRITE_VFIFO:
		DEBUGP("WRITE VFIFO ");
		DEBUGP("NOT IMPLEMENTED YET ");
		break;
	case OPENPCD_CMD_REG_BITS_CLEAR:
		DEBUGP("CLEAR BITS ");
		pih->val = rc632_clear_bits(RAH, poh->reg, poh->val);
		break;
	case OPENPCD_CMD_REG_BITS_SET:
		DEBUGP("SET BITS ");
		pih->val = rc632_set_bits(RAH, poh->reg, poh->val);
		break;
	case OPENPCD_CMD_SET_LED:
		DEBUGP("SET LED(%u,%u) ", poh->reg, poh->val);
		led_switch(poh->reg, poh->val);
		break;
	case OPENPCD_CMD_DUMP_REGS:
		DEBUGP("DUMP REGS ");
		DEBUGP("NOT IMPLEMENTED YET ");
		goto respond;
		break;
	default:
		/* FIXME: add a method how other firmware modules can
		 * register callback functions for additional commands */
		DEBUGP("UNKNOWN ");
		return -EINVAL;
		break;
	}
	req_ctx_put(rctx);
	return 0;

respond:
	req_ctx_set_state(rctx, RCTX_STATE_UDP_EP2_PENDING);
	/* FIXME: we could try to send this immediately */
	udp_refill_ep(2, rctx);
	DEBUGPCR("");

	return 1;
}

void usb_out_process(void)
{
	struct req_ctx *rctx;

	while (rctx = req_ctx_find_get(RCTX_STATE_UDP_EP3_PENDING,
				       RCTX_STATE_UDP_EP3_BUSY)) {
		DEBUGPCRF("EP3_BUSY for ctx %u", req_ctx_num(rctx));
		if (udp_refill_ep(3, rctx) < 0)
			req_ctx_set_state(rctx, RCTX_STATE_UDP_EP3_PENDING);
	}

	while (rctx = req_ctx_find_get(RCTX_STATE_UDP_EP2_PENDING,
				       RCTX_STATE_UDP_EP2_BUSY)) {
		DEBUGPCRF("EP2_BUSY for ctx %u", req_ctx_num(rctx));
		if (udp_refill_ep(2, rctx) < 0)
			req_ctx_set_state(rctx, RCTX_STATE_UDP_EP2_PENDING);
	}
}

void usb_in_process(void)
{
	struct req_ctx *rctx;

	while (rctx = req_ctx_find_get(RCTX_STATE_UDP_RCV_DONE,
				       RCTX_STATE_MAIN_PROCESSING)) {
	     	DEBUGPCRF("found used ctx %u: len=%u", 
			req_ctx_num(rctx), rctx->rx.tot_len);
		usb_in(rctx);
	}
	udp_unthrottle();
}

