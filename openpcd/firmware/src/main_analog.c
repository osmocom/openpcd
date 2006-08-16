/* main_reqa - OpenPCD firmware for generating an endless loop of
 * ISO 14443-A REQA packets.
 *
 * If a response is received from the PICC, LED1 (Red) will be switched
 * on.  If no valid response has been received within the timeout of the
 * receiver, LED1 (Red) will be switched off.
 *
 */

#include <errno.h>
#include <string.h>
#include <librfid/rfid_layer2_iso14443a.h>
#include "rc632.h"
#include "dbgu.h"
#include "led.h"
#include "trigger.h"
#include "pcd_enumerate.h"
#include "main.h"

void _init_func(void)
{
	//udp_init();
	trigger_init();
	rc632_init();
	DEBUGPCRF("turning on RF");
	rc632_turn_on_rf(RAH);
	/* FIXME: do we need this? */
	DEBUGPCRF("initializing 14443A operation");
	rc632_iso14443a_init(RAH);
	/* Switch to 848kBps (1subcp / bit) */
	//rc632_clear_bits(RAH, RC632_REG_RX_CONTROL1, RC632_RXCTRL1_SUBCP_MASK);
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
		rc632_reg_write(RAH, RC632_REG_TEST_ANA_SELECT, ana_out_sel);
	}

	return ret;
}

void _main_func(void)
{
#if 1
	struct iso14443a_atqa atqa;

	memset(&atqa, 0, sizeof(atqa));

	trigger_pulse();

	if (rc632_iso14443a_transceive_sf(RAH, ISO14443A_SF_CMD_WUPA, &atqa) < 0) {
		DEBUGPCRF("error during transceive_sf");
		led_switch(1, 0);
	} else {
		DEBUGPCRF("received ATQA: %s\n", hexdump((char *)&atqa, sizeof(atqa)));
		led_switch(1, 1);
	}
#endif	
	led_toggle(2);
}
