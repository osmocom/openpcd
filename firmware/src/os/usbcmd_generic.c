/* Some generel USB API commands, common between OpenPCD and OpenPICC
 * (C) 2006 by Harald Welte <hwelte@hmw-consulting.de>
 */

#include <string.h>
#include <sys/types.h>
#include <openpcd.h>
#include <os/req_ctx.h>
#include <os/usb_handler.h>
#include <os/led.h>
#include <os/dbgu.h>
#include <os/main.h>

static int gen_usb_rx(struct req_ctx *rctx)
{
	struct openpcd_hdr *poh = (struct openpcd_hdr *) rctx->data;
	struct openpcd_compile_version *ver = 
			(struct openpcd_compile_version *) poh->data;
	int ret = 1;

	rctx->tot_len = sizeof(*poh);

	switch (poh->cmd) {
	case OPENPCD_CMD_GET_VERSION:
		DEBUGP("GET_VERSION ");
		memcpy(ver, &opcd_version, sizeof(*ver));
		rctx->tot_len += sizeof(*ver);
		poh->flags |= OPENPCD_FLAG_RESPOND;
		break;
	case OPENPCD_CMD_SET_LED:
		DEBUGP("SET LED(%u,%u) ", poh->reg, poh->val);
		led_switch(poh->reg, poh->val);
		break;
	default:
		DEBUGP("UNKNOWN ");
		return USB_ERR(USB_ERR_CMD_UNKNOWN);
		break;
	}

	if (poh->flags & OPENPCD_FLAG_RESPOND)
		return USB_RET_RESPOND;
	return 0;
}

void usbcmd_gen_init(void)
{
	usb_hdlr_register(&gen_usb_rx, OPENPCD_CMD_CLS_GENERIC);
}

