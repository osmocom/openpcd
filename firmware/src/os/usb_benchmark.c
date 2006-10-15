/* AT91SAM7 USB benchmark routines for OpenPCD / OpenPICC
 * (C) 2006 by Harald Welte <hwelte@hmw-consulting.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by 
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <errno.h>
#include <string.h>
#include <lib_AT91SAM7.h>
#include <os/led.h>
#include <os/dbgu.h>
#include <os/pcd_enumerate.h>
#include <os/usb_handler.h>
#include <os/req_ctx.h>
#include "../openpcd.h"

static struct req_ctx dummy_rctx;
static struct req_ctx empty_rctx;

static int usbtest_rx(struct req_ctx *rctx)
{
	struct openpcd_hdr *poh = (struct openpcd_hdr *) rctx->data;
	struct req_ctx *rctx_new;
	int i;

	switch (poh->cmd) {
	case OPENPCD_CMD_USBTEST_IN:
		DEBUGP("USBTEST_IN ");
		/* test bulk in pipe */
		if (poh->val > RCTX_SIZE_LARGE/AT91C_EP_OUT_SIZE)
			poh->val = RCTX_SIZE_LARGE/AT91C_EP_OUT_SIZE;
		rctx_new = req_ctx_find_get(1, RCTX_STATE_FREE,
					    RCTX_STATE_MAIN_PROCESSING);
		if (!rctx_new) {
			DEBUGP("NO RCTX ");
			return USB_ERR(0);
		}

		rctx_new->tot_len = poh->val * AT91C_EP_OUT_SIZE;
		req_ctx_set_state(rctx_new, RCTX_STATE_UDP_EP2_PENDING);
		led_toggle(2);
		break;
	case OPENPCD_CMD_USBTEST_OUT:
		DEBUGP("USBTEST_IN ");
		/* test bulk out pipe */
		return USB_ERR(USB_ERR_CMD_NOT_IMPL);
		break;
	}

	req_ctx_put(rctx);
	return 1;
}

void usbtest_init(void)
{
	dummy_rctx.tot_len = 64;
	memset(dummy_rctx.data, 0x23, 64);

	empty_rctx.tot_len = 0;

	usb_hdlr_register(&usbtest_rx, OPENPCD_CMD_CLS_USBTEST);
}
