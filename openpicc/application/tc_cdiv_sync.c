/* Synchronize TC_CDIV divided sample clock with the SOF of the packet */

#include <lib_AT91SAM7.h>
#include <AT91SAM7.h>
#include "dbgu.h"
#include "pio_irq.h"
#include "openpicc.h"

#define USE_IRQ

static u_int8_t enabled;

#if 0
static void pio_data_change(u_int32_t pio)
{
	(void)pio;
	DEBUGP("PIO_FRAME_IRQ: ");
	/* we get one interrupt for each change. If now, after the
	 * change the level is high, then it must have been a rising
	 * edge */
	if (*AT91C_PIOA_PDSR & OPENPICC_PIO_FRAME) {
		*AT91C_TC0_CCR = AT91C_TC_SWTRG;
		DEBUGPCR("CDIV_SYNC_FLIP SWTRG CV=0x%08x",
			  *AT91C_TC0_CV);
	} else
		DEBUGPCR("");
}

#else

static void __ramfunc cdsync_cb(void)
{
	DEBUGP("PIO_IRQ: ");
	if (*AT91C_PIOA_ISR & OPENPICC_PIO_FRAME) {
		DEBUGP("PIO_FRAME_IRQ: ");
		/* we get one interrupt for each change. If now, after the
		 * change the level is high, then it must have been a rising
		 * edge */
		if (*AT91C_PIOA_PDSR & OPENPICC_PIO_FRAME) {
			*AT91C_TC0_CCR = AT91C_TC_SWTRG;
			DEBUGPCR("CDIV_SYNC_FLIP SWTRG CV=0x%08x",
				  *AT91C_TC0_CV);
		} else
			DEBUGPCR("");
	} else
		DEBUGPCR("");
}
#endif

void tc_cdiv_sync_reset(void)
{
	if (enabled) {
		u_int32_t tmp = *AT91C_PIOA_ISR;
		(void)tmp;
		volatile int i;
		DEBUGPCRF("CDIV_SYNC_FLOP");

		/* reset the hardware flipflop */
		AT91F_PIO_ClearOutput(AT91C_BASE_PIOA,
				      OPENPICC_PIO_SSC_DATA_CONTROL);
		for (i = 0; i < 0xff; i++) ;
		AT91F_PIO_SetOutput(AT91C_BASE_PIOA,
				    OPENPICC_PIO_SSC_DATA_CONTROL);
	}
}

void tc_cdiv_sync_disable(void)
{
	enabled = 0;	
	*AT91C_PIOA_IDR = OPENPICC_PIO_FRAME;
}

void tc_cdiv_sync_enable(void)
{
	enabled = 1;

	DEBUGPCRF("CDIV_SYNC_ENABLE ");
	tc_cdiv_sync_reset();
	*AT91C_PIOA_IER = OPENPICC_PIO_FRAME;
}

extern void (*fiq_handler)(void);
void tc_cdiv_sync_init(void)
{
	DEBUGPCRF("initializing");

	enabled = 0;

	AT91F_PIOA_CfgPMC();

#ifdef USE_IRQ
	/* Configure IRQ */
	AT91F_AIC_ConfigureIt(AT91C_ID_PIOA,
			      AT91C_AIC_PRIOR_HIGHEST, 
			      AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL, (THandler)&cdsync_cb);
#else
	/* Configure FIQ */
	AT91F_AIC_ConfigureIt(AT91C_ID_FIQ,
			      //0, AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL, &cdsync_cb);
			      0, AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL, (THandler)&fiq_handler);
	/* enable fast forcing for PIOA interrupt */
	*AT91C_AIC_FFER = (1 << AT91C_ID_PIOA);

	/* register pio irq handler */
	pio_irq_register(OPENPICC_PIO_FRAME, &pio_data_change);
#endif
	AT91F_AIC_EnableIt(AT91C_ID_PIOA);

	tc_cdiv_sync_disable();
}
