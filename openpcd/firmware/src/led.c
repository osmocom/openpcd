
#include <sys/types.h>
#include <include/lib_AT91SAM7.h>
#include "openpcd.h"
#include "dbgu.h"

static int led2port(int led)
{
	if (led == 1)
		return OPENPCD_PIO_LED1;
	else if (led == 2)
		return OPENPCD_PIO_LED2;
	else
		return 0;
}

void led_switch(int led, int on)
{
	int port = led2port(led);

	if (port == -1)
		return;

	if (on)
		AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, port);
	else
		AT91F_PIO_SetOutput(AT91C_BASE_PIOA, port);
}

int led_get(int led)
{
	int port = led2port(led);

	if (port == -1)
		return -1;

	return !(AT91F_PIO_GetOutputDataStatus(AT91C_BASE_PIOA) & port);
}

int led_toggle(int led)
{
	int on = led_get(led);
	if (on == -1)
		return -1;

	if (on)
		led_switch(led, 0);
	else
		led_switch(led, 1);
	
	return !on;
}

void led_init(void)
{
	AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, OPENPCD_PIO_LED1);
	AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, OPENPCD_PIO_LED2);
	led_switch(1, 0);
	led_switch(2, 0);
}
