/* Synchronize TC_CDIV divided sample clock with the SOF of the packet */

#define PIO_DATA	AT91C_PIO_PA27

#include <lib_AT91SAM7.h>
#include <AT91SAM7.h>
#include <os/dbgu.h>
#include "../openpcd.h"

//#define USE_IRQ
#define DISABLE

static u_int8_t enabled;

static void pio_data_change(u_int32_t pio)
{
	/* FIXME: start ssc if we're in one-shot mode */
	//ssc_rx_start();
}

static void __ramfunc cdsync_cb(void)
{
	if (*AT91C_PIOA_ISR & PIO_DATA) {
		/* we get one interrupt for each change. If now, after the
		 * change the level is low, then it must have been a falling
		 * edge */
		if (*AT91C_PIOA_PDSR & PIO_DATA) {
			*AT91C_TC0_CCR = AT91C_TC_SWTRG;
			DEBUGP("SWTRG CV=0x%08x ", *AT91C_TC0_CV);
#ifdef DISABLE
			DEBUGP("CDIV_SYNC_FLIP ");
			//now in fiq *AT91C_PIOA_IDR = PIO_DATA;
#endif
			//ssc_rx_start();
		}
	}
#if 0
	AT91F_AIC_ClearIt(AT91C_BASE_AIC, AT91C_ID_PIOA);
	AT91F_AIC_ClearIt(AT91C_BASE_AIC, AT91C_ID_FIQ);
#endif
	DEBUGPCR("");
}

/* re-enable cdiv_sync FIQ after it has been disabled by our one-shot logic */
void tc_cdiv_sync_reset(void)
{
#ifdef DISABLE
	if (enabled) {
		u_int32_t tmp = *AT91C_PIOA_ISR;
		DEBUGP("CDIV_SYNC_FLOP ");
		*AT91C_PIOA_IER = PIO_DATA;
	}
#endif
}

void tc_cdiv_sync_disable(void)
{
	enabled = 0;	
	*AT91C_PIOA_IDR = PIO_DATA;
}

void tc_cdiv_sync_enable(void)
{
	enabled = 1;

	DEBUGP("CDIV_SYNC_ENABLE ");
	tc_cdiv_sync_reset();
#ifndef DISABLE
	*AT91C_PIOA_IER = PIO_DATA;
#endif
}

extern void (*fiq_handler)(void);
void tc_cdiv_sync_init(void)
{
	DEBUGPCRF("initializing");

	enabled = 0;

	AT91F_PIOA_CfgPMC();

#ifdef USE_IRQ
	/* Configure IRQ */
	AT91F_AIC_ConfigureIt(AT91C_BASE_AIC, AT91C_ID_PIOA,
			      AT91C_AIC_PRIOR_HIGHEST, 
			      AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL, &cdsync_cb);
#else
	/* Configure FIQ */
	AT91F_AIC_ConfigureIt(AT91C_BASE_AIC, AT91C_ID_FIQ,
			      //0, AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL, &cdsync_cb);
			      0, AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL, &fiq_handler);
	/* enable fast forcing for PIOA interrupt */
	*AT91C_AIC_FFER = (1 << AT91C_ID_PIOA);

	/* register pio irq handler */
	pio_irq_register(PIO_DATA, &pio_data_change);
#endif
	AT91F_AIC_EnableIt(AT91C_BASE_AIC, AT91C_ID_PIOA);

	tc_cdiv_sync_disable();
}
