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
	rc632_test(RAH);
}

int _main_dbgu(char key)
{
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
