#include <errno.h>
#include <string.h>
#include <include/lib_AT91SAM7.h>
#include <include/openpcd.h>
#include "dbgu.h"
#include "rc632.h"
#include "led.h"
#include "pcd_enumerate.h"
#include "openpcd.h"

static int usb_in(struct req_ctx *rctx)
{
	struct openpcd_hdr *poh = (struct openpcd_hdr *) &rctx->rx.data[0];
	struct openpcd_hdr *pih = (struct openpcd_hdr *) &rctx->tx.data[0];
	u_int16_t len = rctx->rx.tot_len;

	DEBUGP("usb_in ");

	if (len < sizeof(*poh))
		return -EINVAL;

	//data_len = ntohs(poh->len);

	memcpy(pih, poh, sizeof(*poh));
	rctx->tx.tot_len = sizeof(*poh);

	switch (poh->cmd) {
	case OPENPCD_CMD_READ_REG:
		DEBUGP("READ REG(0x%02x) ", poh->reg);
		pih->val = rc632_reg_read(poh->reg);
		break;
	case OPENPCD_CMD_READ_FIFO:
		DEBUGP("READ FIFO(len=%u) ", poh->val);
		pih->len = rc632_fifo_read(poh->val, pih->data);
		rctx->tx.tot_len += pih->len;
		break;
	case OPENPCD_CMD_WRITE_REG:
		DEBUGP("WRITE_REG(0x%02x, 0x%02x) ", poh->reg, poh->val);
		rc632_reg_write(poh->reg, poh->val);
		break;
	case OPENPCD_CMD_WRITE_FIFO:
		DEBUGP("WRITE FIFO(len=%u) ", poh->len);
		if (len - sizeof(*poh) < poh->len)
			return -EINVAL;
		rc632_fifo_write(poh->len, poh->data);
		break;
	case OPENPCD_CMD_READ_VFIFO:
		DEBUGP("READ VFIFO ");
		DEBUGP("NOT IMPLEMENTED YET ");
		break;
	case OPENPCD_CMD_WRITE_VFIFO:
		DEBUGP("WRITE VFIFO ");
		DEBUGP("NOT IMPLEMENTED YET ");
		break;
	case OPENPCD_CMD_REG_BITS_CLEAR:
		DEBUGP("CLEAR BITS ");
		pih->val = rc632_clear_bits(poh->reg, poh->val);
		break;
	case OPENPCD_CMD_REG_BITS_SET:
		DEBUGP("SET BITS ");
		pih->val = rc632_set_bits(poh->reg, poh->val);
		break;
	case OPENPCD_CMD_SET_LED:
		DEBUGP("SET LED(%u,%u) ", poh->reg, poh->val);
		led_switch(poh->reg, poh->val);
		break;
	default:
		return -EINVAL;
	}
	DEBUGPCRF("calling UDP_Write");
	AT91F_UDP_Write(0, &rctx->tx.data[0], rctx->tx.tot_len);
	DEBUGPCRF("usb_in: returning to main");
	
	return 0;
}

void _init_func(void)
{
	rc632_init();
	udp_init();
}

void _main_func(void)
{
	struct req_ctx *rctx;

	for (rctx = req_ctx_find_busy(); rctx; 
	     rctx = req_ctx_find_busy()) {
	     	DEBUGPCRF("found used ctx %u: len=%u", 
			req_ctx_num(rctx), rctx->rx.tot_len);
		usb_in(rctx);
		req_ctx_put(rctx);
	}
}
