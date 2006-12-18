/* main_reqa - OpenPCD firmware for generating an endless loop of
 * ISO 14443-A REQA packets.
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


/* If a response is received from the PICC, LED2 (green) will be switched
 * on.  If no valid response has been received within the timeout of the
 * receiver, LED1 (Red) will be toggled.
 *
 */

#include <errno.h>
#include <string.h>
#include <librfid/rfid_layer2_iso14443a.h>
#include "rc632.h"
#include <os/dbgu.h>
#include <os/led.h>
#include <os/trigger.h>
#include <os/pcd_enumerate.h>
#include <os/main.h>
#include <pcd/rc632_highlevel.h>

#include <librfid/rfid_reader.h>
#include <librfid/rfid_layer2.h>
#include <librfid/rfid_protocol.h>

#define RAH NULL

static struct rfid_reader_handle *rh;
static struct rfid_layer2_handle *l2h;

void _init_func(void)
{
	trigger_init();
	rc632_init();
	DEBUGPCRF("turning on RF");
	rc632_turn_on_rf(RAH);
	/* FIXME: do we need this? */
	DEBUGPCRF("initializing 14443A operation");
	rh = rfid_reader_open(NULL, RFID_READER_OPENPCD);
	l2h = rfid_layer2_init(rh, RFID_LAYER2_ISO14443A);

	/* Switch to 848kBps (1subcp / bit) */
	//opcd_rc632_clear_bits(RAH, RC632_REG_RX_CONTROL1, RC632_RXCTRL1_SUBCP_MASK);
}

int _main_dbgu(char key)
{
	static char ana_out_sel;
	int ret = -EINVAL;

	switch (key) {
	case 'q':
		ana_out_sel--;
		ret = 1;
		break;
	case 'w':
		ana_out_sel++;
		ret = 1;
		break;
	case 'c':
		rc632_turn_on_rf(RAH);
		break;
	case 'o':
		rc632_turn_off_rf(RAH);
		break;
	}

	if (ana_out_sel >= 0xd)
		ana_out_sel = 0;

	if (ret == 1) {
		ana_out_sel &= 0x0f;
		DEBUGPCR("switching to analog output mode 0x%x\n", ana_out_sel);
		opcd_rc632_reg_write(RAH, RC632_REG_TEST_ANA_SELECT, ana_out_sel);
	}

	return ret;
}

void _main_func(void)
{
#if 1
	struct iso14443a_atqa atqa;

	memset(&atqa, 0, sizeof(atqa));

	trigger_pulse();

	if (iso14443a_transceive_sf(l2h, ISO14443A_SF_CMD_WUPA, &atqa) < 0) {
		DEBUGPCRF("error during transceive_sf");
		led_switch(1, 0);
	} else {
		DEBUGPCRF("received ATQA: %s\n", hexdump((char *)&atqa, sizeof(atqa)));
		led_switch(1, 1);
	}
#endif	
	led_toggle(2);
}
