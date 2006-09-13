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
#include <os/pcd_enumerate.h>
#include <os/usb_handler.h>
#include <os/req_ctx.h>
#include "../openpcd.h"

static struct req_ctx dummy_rctx;
static struct req_ctx empty_rctx;

static void usbtest_tx_transfer(unsigned int num_pkts)
{
	unsigned int i;

	for (i = 0; i < num_pkts; i++) {
		/* send 16 packets of 64byte */
		while (udp_refill_ep(2, &dummy_rctx) < 0) 
			;
	}
	/* send one packet of 0 byte */
	while (udp_refill_ep(2, &empty_rctx) < 0) 
		;
}

static int usbtest_rx(struct req_ctx *rctx)
{
	struct openpcd_hdr *poh = (struct openpcd_hdr *) &rctx->rx.data[0];
	int i;

	switch (poh->cmd) {
	case OPENPCD_CMD_USBTEST_IN:
		/* test bulk in pipe */
		for (i = 0; i < poh->reg; i++) {
			usbtest_tx_transfer(poh->val);
			led_toggle(2);
		}
		break;
	case OPENPCD_CMD_USBTEST_OUT:
		/* test bulk out pipe */
		break;
	}

	req_ctx_put(rctx);
	return 1;
}

void usbtest_init(void)
{
	dummy_rctx.tx.tot_len = 64;
	memset(dummy_rctx.tx.data, 0x23, 64);

	empty_rctx.tx.tot_len = 0;

	usb_hdlr_register(&usbtest_rx, OPENPCD_CMD_CLS_USBTEST);
}
