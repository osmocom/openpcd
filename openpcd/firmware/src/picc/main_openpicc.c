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

void _init_func(void)
{
	//tc_cdiv_init();
	//adc_init();
	//ssc_rx_init();
	//poti_init();
	// ssc_tx_init();
}

int _main_dbgu(char key)
{
	unsigned char value;
	static u_int8_t poti = 64;

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
		poti_init();
		break;
#if 0
	case '4':
		AT91F_DBGU_Printk("Testing RC632 : ");
		if (rc632_test(RAH) == 0)
			AT91F_DBGU_Printk("SUCCESS!\n\r");
		else
			AT91F_DBGU_Printk("ERROR!\n\r");
			
		break;
	case '5':
		rc632_reg_read(RAH, RC632_REG_RX_WAIT, &value);
		DEBUGPCR("Reading RC632 Reg RxWait: 0x%02xr", value);

		break;
	case '6':
		DEBUGPCR("Writing RC632 Reg RxWait: 0x55");
		rc632_reg_write(RAH, RC632_REG_RX_WAIT, 0x55);
		break;
	case '7':
		rc632_dump();
		break;
	case 'P':
		rc632_power(1);
		break;
	case 'p':
		rc632_power(0);
		break;
#endif
	}

	return -EINVAL;
}

void _main_func(void)
{
	/* first we try to get rid of pending to-be-sent stuff */
	usb_out_process();

	/* next we deal with incoming reqyests from USB EP1 (OUT) */
	usb_in_process();

	ssc_rx_unthrottle();
}
