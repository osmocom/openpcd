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
#include <lib_AT91SAM7.h>
#include <librfid/rfid_layer2_iso14443a.h>
#include "rc632.h"
#include "dbgu.h"
#include "led.h"
#include "pcd_enumerate.h"
#include "trigger.h"

void _init_func(void)
{
	//udp_init();
	trigger_init();
	rc632_init();
	DEBUGPCRF("turning on RF");
	rc632_turn_on_rf(RAH);
	DEBUGPCRF("initializing 14443A operation");
	rc632_iso14443a_init(RAH);
}

#define MODE_REQA	0x01
#define MODE_WUPA	0x02
#define MODE_ANTICOL	0x03

static volatile int mode = MODE_REQA;

static void reg_inc(u_int8_t reg)
{
	u_int8_t val;
	rc632_reg_read(RAH, reg, &val);
	rc632_reg_write(RAH, reg, val++);
	DEBUGPCRF("reg 0x%02x = 0x%02x", reg, val);
}

static void reg_dec(u_int8_t reg)
{
	u_int8_t val;
	rc632_reg_read(RAH, reg, &val);
	rc632_reg_write(RAH, reg, val--);
	DEBUGPCRF("reg 0x%02x = 0x%02x", reg, val);
}

static u_int8_t ana_out_sel;
static u_int8_t mfout_sel;

static void help(void)
{
	DEBUGPCR("r: REQA         w: WUPA        a: ANTICOL\r\n"
		 "y: inc cw cond  x: dec cond    c: inc mod cond");
	DEBUGPCR("v: dec mod cond o: dec ana_out p: dec ana_out\r\n"
		 "h: trigger high l: trigger low");
}

int _main_dbgu(char key)
{
	int ret = 0;

	switch (key) {
	case '?':
		help();
		break;
	case 'r':
		mode = MODE_REQA;
		break;
	case 'w':
		mode = MODE_WUPA;
		break;
	case 'a':
		mode = MODE_ANTICOL;
		break;
		/* Those below don't work as long as 
		 * iso14443a_init() is called before
		 * every cycle */

	case 'y':
		reg_inc(RC632_REG_CW_CONDUCTANCE);
		break;
	case 'x':
		reg_dec(RC632_REG_CW_CONDUCTANCE);
		break;
	case 'c':
		reg_inc(RC632_REG_MOD_CONDUCTANCE);
		break;
	case 'v':
		reg_dec(RC632_REG_MOD_CONDUCTANCE);
		break;
	case 'o':
		if (ana_out_sel > 0) {
			ana_out_sel--;
			DEBUGPCR("switching to analog output mode 0x%x\n", ana_out_sel);
			rc632_reg_write(RAH, RC632_REG_TEST_ANA_SELECT, ana_out_sel);
		}
		ret = 1;
		break;
	case 'p':
		if (ana_out_sel < 0xc) {
			ana_out_sel++;
			DEBUGPCR("switching to analog output mode 0x%x\n", ana_out_sel);
			rc632_reg_write(RAH, RC632_REG_TEST_ANA_SELECT, ana_out_sel);
		}
		ret = 1;
		break;
	case 'u':
		if (mfout_sel > 0) {
			mfout_sel--;
			DEBUGPCR("switching to MFOUT mode 0x%x\n", mfout_sel);
			rc632_reg_write(RAH, RC632_REG_MFOUT_SELECT, mfout_sel);
		}
		ret = 1;
		break;
	case 'i':
		if (mfout_sel < 5) {
			mfout_sel++;
			DEBUGPCR("switching to MFOUT mode 0x%x\n", mfout_sel);
			rc632_reg_write(RAH, RC632_REG_MFOUT_SELECT, mfout_sel);
		}
		ret = 1;
		break;
	case 'h':
		AT91F_PIO_SetOutput(AT91C_BASE_PIOA, OPENPCD_PIO_TRIGGER);
		break;
	case 'l':
		AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, OPENPCD_PIO_TRIGGER);
		break;
	default:
		return -EINVAL;
	}
	
	return ret;
}

void _main_func(void)
{
	int status;
	struct iso14443a_atqa atqa;
	struct rfid_layer2_handle l2h;
	volatile int i;

	memset(&atqa, 0, sizeof(atqa));

	/* fake layer2 handle initialization */
	memset(&l2h, 0, sizeof(l2h));
	l2h.l2 = &rfid_layer2_iso14443a;
	l2h.priv.iso14443a.state = ISO14443A_STATE_NONE;
	l2h.priv.iso14443a.level = ISO14443A_LEVEL_NONE;

	/* FIXME: why does this only work every second attempt without reset or
	 * power-cycle? */
	rc632_turn_off_rf();
	//rc632_reset();
	rc632_turn_on_rf();

	rc632_iso14443a_init(RAH);
	rc632_reg_write(RAH, RC632_REG_TEST_ANA_SELECT, ana_out_sel);
	rc632_reg_write(RAH, RC632_REG_MFOUT_SELECT, mfout_sel);
	for (i = 0; i < 0x3ffff; i++) {}
	//rc632_dump();

	switch (mode) {
	case MODE_REQA:
		status = rc632_iso14443a_transceive_sf(RAH, ISO14443A_SF_CMD_REQA, &atqa);
		if (status < 0)
			DEBUGPCRF("error during transceive_sf REQA");
		else 
			DEBUGPCRF("received ATQA: %s", hexdump((char *)&atqa, sizeof(atqa)));
		break;
	case MODE_WUPA:
		status = rc632_iso14443a_transceive_sf(RAH, ISO14443A_SF_CMD_WUPA, &atqa);
		if (status < 0)
			DEBUGPCRF("error during transceive_sf WUPA");
		else 
			DEBUGPCRF("received WUPA: %s", hexdump((char *)&atqa, sizeof(atqa)));
		break;
	case MODE_ANTICOL:
		status = iso14443a_anticol(&l2h);
		if (status < 0)
			DEBUGPCR("error during anticol");
		else
			DEBUGPCR("Anticol OK");
		break;
	}

	if (status < 0)
		led_switch(1, 0);
	else 
		led_switch(1, 1);

	led_toggle(2);

}
