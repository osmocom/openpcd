/* OpenPCD USB handler - handle incoming USB requests on OUT pipe
 * (C) 2006 by Harald Welte
 */

#include <sys/types.h>
#include <errno.h>
#include <string.h>

#include <openpcd.h>

#include "pcd_enumerate.h"
#include "usb_handler.h"
#include "openpcd.h"
#include "rc632.h"
#include "led.h"
#include "dbgu.h"

static usb_cmd_fn *cmd_hdlrs[16];

int usb_hdlr_register(usb_cmd_fn *hdlr, u_int8_t class)
{
	cmd_hdlrs[class] = hdlr;
}

void usb_hdlr_unregister(u_int8_t class)
{
	cmd_hdlrs[class] = NULL;
}

static int usb_in(struct req_ctx *rctx)
{
	struct openpcd_hdr *poh = (struct openpcd_hdr *) &rctx->rx.data[0];
	struct openpcd_hdr *pih = (struct openpcd_hdr *) &rctx->tx.data[0];
	usb_cmd_fn *hdlr;

	DEBUGP("usb_in(cls=%d) ", OPENPCD_CMD_CLS(poh->cmd));

	if (rctx->rx.tot_len < sizeof(*poh))
		return -EINVAL;
	
	memcpy(pih, poh, sizeof(*poh));
	rctx->tx.tot_len = sizeof(*poh);

	hdlr = cmd_hdlrs[OPENPCD_CMD_CLS(poh->cmd)];
	if (hdlr) 
		return (hdlr)(rctx);
	else
		DEBUGPCR("no handler for this class\n");
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

