/* AT91SAM7 SSC controller routines for OpenPCD
 * (C) 2006 by Harald Welte <hwelte@hmw-consulting.de>
 *
 * We use SSC for both TX and RX side.
 *
 * RX side is interconnected with MFOUT of RC632
 *
 * TX side is interconnected with MFIN of RC632
 */

#include <errno.h>
#include <sys/types.h>
#include <lib_AT91SAM7.h>

#include "openpcd.h"
#include "dbgu.h"

static AT91PS_SSC ssc = AT91C_BASE_SSC;
static AT91PS_PDC rx_pdc;

struct ssc_state {
	struct req_ctx *rx_ctx[2];
};

static struct ssc_state ssc_state;

/* Try to refill RX dma descriptors. Return values:
 *  0) no dma descriptors empty
 *  1) filled next/secondary descriptor
 *  2) filled both primary and secondary descriptor
 * -1) no free request contexts to use
 * -2) only one free request context, but two free descriptors
 */
static int8_t ssc_rx_refill(void)
{
	struct req_ctx *rctx;
	
	rctx = req_ctx_find_get(RCTX_STATE_FREE, RCTX_STATE_SSC_RX_BUSY);
	if (!rctx) {
		DEBUGPCRF("no rctx for refill!");
		return -1;
	}

	if (AT91F_PDC_IsRxEmpty(rx_pdc)) {
		DEBUGPCRF("filling primary SSC RX dma ctx");
		AT91F_PDC_SetRx(rx_pdc, &rctx->rx.data[MAX_HDRSIZE],
				MAX_REQSIZE);
		ssc_state.rx_ctx[0] = rctx;

		/* If primary is empty, secondary must be empty, too */
		rctx = req_ctx_find_get(RCTX_STATE_FREE, 
					RCTX_STATE_SSC_RX_BUSY);
		if (!rctx) {
			DEBUGPCRF("no rctx for secondary refill!");
			return -2;
		}
	}

	if (AT91F_PDC_IsNextRxEmpty(rx_pdc)) {
		DEBUGPCRF("filling secondary SSC RX dma ctx");
		AT91F_PDC_SetNextRx(rx_pdc, &rctx->rx.data[MAX_HDRSIZE],
				    MAX_REQSIZE);
		ssc_state.rx_ctx[1] = rctx;
		return 2;
	} else {
		/* we were unable to fill*/
		DEBUGPCRF("prim/secnd DMA busy, can't refill");
		req_ctx_put(rctx);	
		return 0;
	}
}

static void ssc_irq(void)
{
	u_int32_t ssc_sr = ssc->SSC_SR;
	DEBUGPCRF("ssc_sr=0x%08x", ssc_sr);

	if (ssc_sr & (AT91C_SSC_ENDRX | AT91C_SSC_TXBUFE)) {
		/* Mark primary RCTX as ready to send for usb */
		req_ctx_set_state(ssc_state.rx_ctx[0], 
				  RCTX_STATE_UDP_EP2_PENDING);
		/* second buffer gets propagated to primary */
		ssc_state.rx_ctx[0] = ssc_state.rx_ctx[1];
		ssc_state.rx_ctx[1] = NULL;

		if (ssc_sr & AT91C_SSC_TXBUFE) {
			DEBUGPCRF("TXBUFE, shouldn't happen!");
			req_ctx_set_state(ssc_state.rx_ctx[0],
					  RCTX_STATE_UDP_EP2_PENDING);
		}
		if (ssc_rx_refill() == -1)
			AT91F_AIC_DisableIt(ssc, AT91C_SSC_ENDRX |
					    AT91C_SSC_TXBUFE |
					    AT91C_SSC_OVRUN);
	}

	if (ssc_sr & AT91C_SSC_OVRUN)
		DEBUGPCRF("Rx Overrun, shouldn't happen!");
}


void ssc_rx_unthrottle(void)
{
	AT91F_AIC_EnableIt(ssc, AT91C_SSC_ENDRX |
			   AT91C_SSC_TXBUFE | AT91C_SSC_OVRUN);
}

void ssc_rx_start(void)
{
	DEBUGPCRF("starting SSC RX\n");	
	/* 're'fill DMA descriptors */
	//ssc_rx_refill();

	/* Enable Reception */
	AT91F_SSC_EnableRx(ssc);
}

void ssc_rx_stop(void)
{
	/* Disable reception */
	AT91F_SSC_DisableRx(ssc);
}

void ssc_tx_init(void)
{
	/* IMPORTANT: Disable PA23 (PWM0) output, since it is connected to 
	 * PA17 !! */
	AT91F_PIO_CfgInput(AT91C_BASE_PIOA, OPENPCD_PIO_MFIN_PWM);
	AT91F_PIO_CfgPeriph(AT91C_BASE_PIOA, OPENPCD_PIO_MFIN_SSC_TX | 
			    OPENPCD_PIO_MFOUT_SSC_RX | OPENPCD_PIO_SSP_CKIN,
			    0);
}

void ssc_rx_init(void)
{
	rx_pdc = (AT91PS_PDC) &(ssc->SSC_RPR);

	AT91F_SSC_CfgPMC();
	AT91F_AIC_ConfigureIt(AT91C_BASE_AIC, AT91C_ID_SSC,
			      OPENPCD_IRQ_PRIO_SSC,
			      AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL, &ssc_irq);
	AT91F_AIC_EnableIt(AT91C_BASE_AIC, AT91C_ID_SSC);

	/* Reset */
	ssc->SSC_CR = AT91C_SSC_SWRST;

	/* don't divide clock */
	ssc->SSC_CMR = 0;

	ssc->SSC_RCMR = AT91C_SSC_CKS_RK | AT91C_SSC_CKO_NONE |
			AT91C_SSC_START_CONTINOUS;
	/* Data bits per Data N = 32-1,  Data words per Frame = 16-1*/
	ssc->SSC_RFMR = 31 | AT91C_SSC_MSBF | (15 << 8);

	/* Enable RX interrupts */
	AT91F_SSC_EnableIt(ssc, AT91C_SSC_OVRUN |
			   AT91C_SSC_ENDRX | AT91C_SSC_RXBUFF);
	AT91F_PDC_EnableRx(rx_pdc);
}

void ssc_fini(void)
{
	AT91F_PDC_DisableRx(rx_pdc);
	AT91F_SSC_DisableTx(ssc);
	AT91F_SSC_DisableRx(ssc);
	AT91F_SSC_DisableIt(ssc, 0xfff);
	AT91F_PMC_DisablePeriphClock(AT91C_BASE_PMC, 
				 ((unsigned int) 1 << AT91C_ID_SSC));
}
