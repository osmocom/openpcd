#include <errno.h>
#include <include/lib_AT91SAM7.h>
#include <include/openpcd.h>
#include <os/dbgu.h>
#include "rc632.h"
#include <os/led.h>
#include <os/pcd_enumerate.h>
#include <os/usb_handler.h>
#include "../openpcd.h"
#include <os/main.h>

void _init_func(void)
{
	rc632_init();
	udp_init();
	udp_open();
	rc632_test(RAH);
}

int _main_dbgu(char key)
{
	unsigned char value;

	switch (key) {
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
	}

	return -EINVAL;
}

void _main_func(void)
{
	/* first we try to get rid of pending to-be-sent stuff */
	usb_out_process();

	/* next we deal with incoming reqyests from USB EP1 (OUT) */
	usb_in_process();

	rc632_unthrottle();
}
