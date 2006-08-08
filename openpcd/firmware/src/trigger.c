#include <lib_AT91SAM7.h>
#include "openpcd.h"
#include "trigger.h"

void trigger_init(void)
{
	AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, OPENPCD_PIO_TRIGGER);
}

void trigger_pulse(void)
{
	volatile int i;
	AT91F_PIO_SetOutput(AT91C_BASE_PIOA, OPENPCD_PIO_TRIGGER);
	for (i=0; i < 0xff; i++)
		{ }
	AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, OPENPCD_PIO_TRIGGER);
}
