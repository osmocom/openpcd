/* Main state machine and register implementation for OpenPICC
 * (C) 2006 by Harald Welte <hwelte@hmw-consulting.de> */

#include <sys/types.h>
#include <openpcd.h>
#include <openpicc_regs.h>
#include <openpicc.h>
#include <os/req_ctx.h>
#include <os/usb_handler.h>

#include "opicc_reg.h"

/********************************************************************
 * OpenPICC Register set
 ********************************************************************/

/* Our registers, including their power-up default values */
static u_int16_t opicc_regs[_OPICC_NUM_REGS] = {
	[OPICC_REG_14443A_UIDLEN]	= 4,
	[OPICC_REG_14443A_FDT0]		= 1236,
	[OPICC_REG_14443A_FDT1]		= 1172,
	[OPICC_REG_14443A_STATE]	= ISO14443A_ST_POWEROFF,
	[OPICC_REG_RX_CLK_DIV]		= 32,
	[OPICC_REG_RX_CLK_PHASE]	= 0,
	[OPICC_REG_RX_CONTROL]		= 0,
	[OPICC_REG_TX_CLK_DIV]		= 16,
	[OPICC_REG_TX_CONTROL]		= 0,
	[OPICC_REG_RX_COMP_LEVEL]	= 0,
};

u_int16_t opicc_reg_read(enum opicc_reg reg)
{
	if (reg < _OPICC_NUM_REGS)
		return opicc_regs[reg];
	return 0;
}

void opicc_reg_write(enum opicc_reg reg, u_int16_t val)
{
	if (reg < _OPICC_NUM_REGS)
		opicc_regs[reg] = val;
	return;
}

/********************************************************************
 * OpenPICC USB Commandset (access to register set, ...)
 ********************************************************************/

static int opicc_reg_usb_in(struct req_ctx *rctx)
{
	struct openpcd_hdr *poh = (struct openpcd_hdr *) &rctx->data[0];
	u_int16_t *val16 = (u_int16_t *) poh->data;
	
	poh->val = 0;
	rctx->tot_len = sizeof(*poh);

	switch (poh->cmd) {
	case OPENPCD_CMD_PICC_REG_READ:
		*val16 = opicc_reg_read(poh->reg);
		rctx->tot_len += sizeof(u_int16_t);
		poh->flags |= OPENPCD_FLAG_RESPOND;
		break;
	case OPENPCD_CMD_PICC_REG_WRITE:
		if (rctx->tot_len < sizeof(*poh) + sizeof(u_int16_t)) {
			poh->flags = OPENPCD_FLAG_ERROR;
		}
		opicc_reg_write(poh->reg, *val16);
		break;
	default:
		return USB_ERR(USB_ERR_CMD_UNKNOWN);
	}
	
	if (poh->flags & OPENPCD_FLAG_RESPOND)
		return USB_RET_RESPOND;

	return 0;
}

void opicc_usbapi_init(void)
{
	usb_hdlr_register(&opicc_reg_usb_in, OPENPCD_CMD_CLS_PICC);
}
