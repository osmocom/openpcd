/* main_librfid - OpenPCD firmware using in-firmware librfid
 *
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
#include <librfid/rfid.h>
#include <librfid/rfid_scan.h>
#include <librfid/rfid_reader.h>
#include <librfid/rfid_layer2.h>
#include <librfid/rfid_protocol.h>
//#include "rc632.h"
#include <os/dbgu.h>
#include <os/led.h>
#include <os/pcd_enumerate.h>
#include <os/trigger.h>
#include <os/req_ctx.h>

#include "../openpcd.h"

static struct rfid_reader_handle *rh;
static struct rfid_layer2_handle *l2h;
static struct rfid_protocol_handle *ph;

void _init_func(void)
{
	trigger_init();
	rc632_init();
	rc632_test();
	DEBUGP("opening reader ");
#if 1
	rh = rfid_reader_open(NULL, RFID_READER_OPENPCD);
	DEBUGP("rh=%p ", rh);
#endif
	led_switch(2, 1);
}

int _main_dbgu(char key)
{
	int ret = -EINVAL;
	return ret;
}

struct openpcd_l2_connectinfo {
	u_int32_t proto_supported;	

	u_int8_t speed_rx;
	u_int8_t speed_tx;

	u_int8_t uid_len;
	u_int8_t uid[10];
} __attribute__ ((packed));

struct openpcd_proto_connectinfo {
} __attribute__ ((packed));

struct openpcd_proto_tcl_connectinfo {
	u_int8_t fsc;
	u_int8_t fsd;
	u_int8_t ta;
	u_int8_t sfgt;

	u_int8_t flags;
	u_int8_t cid;
	u_int8_t nad;

	u_int8_t ats_tot_len;
	u_int8_t ats_snippet[0];
} __attribute__ ((packed));

static int init_proto(void)
{
	struct req_ctx *detect_rctx;
	struct openpcd_hdr *opcdh;
	struct openpcd_l2_connectinfo *l2c;
	struct openpcd_proto_connectinfo *pc;
	unsigned int size;

	l2h = rfid_layer2_scan(rh);
	if (!l2h)
		return 0;

	DEBUGP("l2='%s' ", rfid_layer2_name(l2h));

	detect_rctx = req_ctx_find_get(0, RCTX_STATE_FREE,
					RCTX_STATE_LIBRFID_BUSY);
	if (detect_rctx) {
		unsigned int uid_len;
		opcdh = (struct openpcd_hdr *) detect_rctx->data;
		l2c = (struct openpcd_l2_connectinfo *) 
				(char *) opcdh + sizeof(opcdh);
		l2c->uid_len = sizeof(l2c->uid);
		opcdh->cmd = OPENPCD_CMD_LRFID_DETECT_IRQ;
		opcdh->flags = 0x00;
		opcdh->reg = 0x03;
		opcdh->val = l2h->l2->id;

#if 0
		/* copy UID / PUPI into data section */
		rfid_layer2_getopt(l2h, RFID_OPT_LAYER2_UID, (void *)l2c->uid, 
					&uid_len);
		l2c->uid_len = uid_len & 0xff;
		
		size = sizeof(l2c->proto_supported);
		rfid_layer2_getopt(l2h, RFID_OPT_LAYER2_PROTO_SUPP,
					&l2c->proto_supported, &size);

		detect_rctx->tot_len = sizeof(*opcdh) + sizeof(*l2c);
		
		switch (l2h->l2->id) {
		case RFID_LAYER2_ISO14443A:
			break;
		case RFID_LAYER2_ISO14443B:
			break;
		case RFID_LAYER2_ISO15693:
			break;
		}
#endif
		req_ctx_set_state(detect_rctx, RCTX_STATE_UDP_EP3_PENDING);
	}
	ph = rfid_protocol_scan(l2h);
	if (!ph)
		return 3;

	DEBUGP("p='%s' ", rfid_protocol_name(ph));
	detect_rctx = req_ctx_find_get(0, RCTX_STATE_FREE,
					RCTX_STATE_LIBRFID_BUSY);
	if (detect_rctx) {
		opcdh = (struct openpcd_hdr *) detect_rctx->data;
		pc = (struct openpcd_proto_connectinfo *)
				((char *) opcdh + sizeof(*opcdh));
		detect_rctx->tot_len = sizeof(*opcdh) + sizeof(*pc);
		opcdh->cmd = OPENPCD_CMD_LRFID_DETECT_IRQ;
		opcdh->flags = 0x00;
		opcdh->reg = 0x04;
		opcdh->val = ph->proto->id;
		/* copy L4 info into data section */

#if 0
		switch (ph->proto->id) {
		case RFID_PROTOCOL_TCL: {
			struct openpcd_proto_tcl_connectinfo *ptc
				= (struct openpcd_proto_tcl_connectinfo *)
						((char *) ph + sizeof(*ph));
			unsigned int space;
			detect_rctx->tot_len += sizeof(*ptc);
			space = detect_rctx->size - sizeof(*opcdh)-sizeof(*pc);
			size = space;
			rfid_protocol_getopt(ph, RFID_OPT_P_TCL_ATS,
					     &ptc->ats_snippet, &size);
			if (size == space) {
				/* we've only copied part of the ATS */
				size = sizeof(ptc->ats_tot_len);
				rfid_protocol_getopt(ph, 
						     RFID_OPT_P_TCL_ATS_LEN,
						     &ptc->ats_tot_len, &size);
			} else {
				ptc->ats_tot_len = size;
			}

			} break;
		}
#endif
		req_ctx_set_state(detect_rctx, RCTX_STATE_UDP_EP3_PENDING);
	}
	led_switch(1, 1);

	return 4;
}

static int opcd_lrfid_usb_in(struct req_ctx *rctx)
{
	struct openpcd_hdr *poh = (struct openpcd_hdr *) rctx->data;
	return 0;
}


void _main_func(void)
{
	int ret;

	usb_out_process();
	usb_in_process();
	
	ret = init_proto();

	if (ret >= 4)
		rfid_protocol_close(ph);
	if (ret >= 3)
		rfid_layer2_close(l2h);

	rc632_turn_off_rf(NULL);
	{ volatile int i; for (i = 0; i < 0x3ffff; i++) ; }
	rc632_turn_on_rf(NULL);

	led_switch(1, 0);
	led_toggle(2);
}
