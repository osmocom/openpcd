/* main_reqa - OpenPCD firmware for generating an endless loop of
 * ISO 14443-A REQA packets.  Alternatively we can send WUPA, or
 * perform a full ISO14443A anti-collision loop.
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
#include <os/dbgu.h>
#include <os/led.h>
#include <os/pcd_enumerate.h>
#include <os/trigger.h>

#include "../openpcd.h"

#ifdef WITH_TC
#include "tc.h"
#endif

void _init_func(void)
{
	trigger_init();
	DEBUGPCRF("enabling RC632");
	rc632_init();
#ifdef WITH_TC
	DEBUGPCRF("enabling TC");
	tc_cdiv_init();
#endif
	DEBUGPCRF("turning on RF");
	rc632_turn_on_rf(RAH);
	DEBUGPCRF("initializing 14443A operation");
	rc632_iso14443a_init(RAH);
}

#define MODE_REQA	0x01
#define MODE_WUPA	0x02
#define MODE_ANTICOL	0x03
#define MODE_14443A	0x04

static volatile int mode = MODE_REQA;

static const char frame_14443a[] = { 0x00, 0xff, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 };

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
static u_int8_t speed_idx;

static void help(void)
{
	DEBUGPCR("r: REQA         w: WUPA        a: ANTICOL\r\n"
		 "A: 14443A       +: inc speed   -: dec speed\r\n"
		 "y: inc cw cond  x: dec cond    c: inc mod cond");
	DEBUGPCR("v: dec mod cond o: dec ana_out p: dec ana_out\r\n"
		 "h: trigger high l: trigger low u: dec MFOUT mode");
	DEBUGPCR("i: inc MFOUT md <: dec cdiv ph >: inc cdiv phase\r\n"
		 "{: dev cdiv     }: inc cdiv");
}

static u_int16_t cdivs[] = { 128, 64, 32, 16 };

int _main_dbgu(char key)
{
	int ret = 0;
	static int cdiv_idx = 0;

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
	case 'A':
		mode = MODE_14443A;
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
#ifdef WITH_TC
	case '<':
		tc_cdiv_phase_inc();
		break;
	case '>':
		tc_cdiv_phase_dec();
		break;
	case '{':
		if (cdiv_idx > 0)
			cdiv_idx--;
		tc_cdiv_set_divider(cdivs[cdiv_idx]);
		break;
	case '}':
		if (cdiv_idx < ARRAY_SIZE(cdivs)-1)
			cdiv_idx++;
		tc_cdiv_set_divider(cdivs[cdiv_idx]);
		break;
#endif
	case '-':
		if (speed_idx > 0)
			speed_idx--;
		break;
	case '+':
		if (speed_idx < 3)
			speed_idx++;
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
#ifdef WITH_TC
	tc_cdiv_print();
#endif

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
		status = rfid_layer2_iso14443a.fn.open(&l2h);
		if (status < 0)
			DEBUGPCR("error during anticol");
		else
			DEBUGPCR("Anticol OK");
		break;
	case MODE_14443A:
		{
			char rx_buf[4];
			int rx_len = sizeof(rx_buf);
			rfid_layer2_iso14443a.fn.setopt(&l2h, RFID_OPT_14443A_SPEED_RX,
					 &speed_idx, sizeof(speed_idx));
			rfid_layer2_iso14443a.fn.setopt(&l2h, RFID_OPT_14443A_SPEED_TX,
					 &speed_idx, sizeof(speed_idx));
			rfid_layer2_iso14443a.fn.transceive(&l2h, RFID_14443A_FRAME_REGULAR, 
					    &frame_14443a, sizeof(frame_14443a),
					    &rx_buf, &rx_len, 1, 0);
		}
		break;
	}

	if (status < 0)
		led_switch(1, 0);
	else 
		led_switch(1, 1);

	led_toggle(2);

}
