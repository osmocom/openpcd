#include "openpcd.h"
#include "trigger.h"
#include <lib_AT91SAM7.h>

void trigger_init(void)
{
	AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, OPENPCD_PIO_TRIGGER);
}

void trigger_pulse(void)
{
	AT91F_PIO_SetOutput(AT91C_BASE_PIOA, OPENPCD_PIO_TRIGGER);
	AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, OPENPCD_PIO_TRIGGER);
}
