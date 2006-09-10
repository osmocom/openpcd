#include <errno.h>
#include <include/lib_AT91SAM7.h>
#include <include/openpcd.h>
#include <os/dbgu.h>
#include "ssc_picc.h"
#include <os/led.h>
#include <os/pcd_enumerate.h>
#include <os/usb_handler.h>
#include "../openpcd.h"
#include <os/main.h>
#include <picc/pll.h>

void _init_func(void)
{
	pll_init();
	poti_init();
	tc_cdiv_init();
	//adc_init();
	//ssc_rx_init();
	// ssc_tx_init();
}

int _main_dbgu(char key)
{
	unsigned char value;
	static u_int8_t poti = 64;
	static u_int8_t pll_inh = 1;

	DEBUGPCRF("main_dbgu");

	switch (key) {
	case 'q':
		if (poti > 0)
			poti--;
		poti_comp_carr(poti);
		DEBUGPCRF("Poti: %u", poti);
		break;
	case 'w':
		if (poti < 126)
			poti++;
		poti_comp_carr(poti);
		DEBUGPCRF("Poti: %u", poti);
		break;
	case 'e':
		poti_comp_carr(poti);
		DEBUGPCRF("Poti: %u", poti);
		break;
	case 'p':
		pll_inh++;
		pll_inh &= 0x01;
		pll_inhibit(pll_inh);
		DEBUGPCRF("PLL Inhibit: %u\n", pll_inh);
		break;
	case '>':
		break;
	}

	return -EINVAL;
}

void _main_func(void)
{
	/* first we try to get rid of pending to-be-sent stuff */
	usb_out_process();

	/* next we deal with incoming reqyests from USB EP1 (OUT) */
	usb_in_process();

	//ssc_rx_unthrottle();
}
