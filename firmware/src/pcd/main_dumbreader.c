/* AT91SAM7 "dumb reader" firmware for OpenPCD
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
#include <include/lib_AT91SAM7.h>
#include <include/openpcd.h>
#include <os/dbgu.h>
#include "rc632.h"
#include "rc632_highlevel.h"
#include <os/led.h>
#include <os/pcd_enumerate.h>
#include <os/usb_handler.h>
#include "../openpcd.h"
#include <os/main.h>

#define RAH NULL

void _init_func(void)
{
	rc632_init();
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
		opcd_rc632_reg_read(RAH, RC632_REG_RX_WAIT, &value);
		DEBUGPCR("Reading RC632 Reg RxWait: 0x%02xr", value);

		break;
	case '6':
		DEBUGPCR("Writing RC632 Reg RxWait: 0x55");
		opcd_rc632_reg_write(RAH, RC632_REG_RX_WAIT, 0x55);
		break;
	case '7':
		rc632_dump();
		break;
	case 'P':
		rc632_power(1);
		rc632_init();
		break;
	case 'p':
		rc632_power(0);
		break;
	}

	return -EINVAL;
}

void _main_func(void)
{
	static int i;
	
	if(i<4096)
	    i++;
	else
	{
	    led_toggle(1);
	    i=0;
	}
	
	/* first we try to get rid of pending to-be-sent stuff */
	usb_out_process();

	/* next we deal with incoming reqyests from USB EP1 (OUT) */
	usb_in_process();

	rc632_unthrottle();
}
