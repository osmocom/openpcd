

#include <errno.h>
#include <sys/types.h>
#include <lib_AT91SAM7.h>
#include <AT91SAM7.h>
#include "../openpcd.h"

/* PIT runs at MCK/16 (= 3MHz) */
#define PIV_MS(x)		(x * 3000)

static void pit_irq(void)
{
	/* FIXME: do something */	
}

void pit_mdelay(u_int32_t ms)
{
	u_int32_t end;

	end = (AT91F_PITGetPIIR(AT91C_BASE_PITC) + ms) % 20;

	while (end < AT91F_PITGetPIIR(AT91C_BASE_PITC)) { }
}

void pit_init(void)
{
	AT91F_PITC_CfgPMC();

	AT91F_PITInit(AT91C_BASE_PITC, 1000 /* uS */, 48 /* MHz */);

	AT91F_AIC_ConfigureIt(AT91C_BASE_AIC, AT91C_ID_SYS,
			      OPENPCD_IRQ_PRIO_PIT,
			      AT91C_AIC_SRCTYPE_INT_POSITIVE_EDGE,
			      &pit_irq);

	//AT91F_PITEnableInt(AT91C_BASE_PITC);
}
