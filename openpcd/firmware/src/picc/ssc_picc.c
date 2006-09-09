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
#include <string.h>
#include <sys/types.h>
#include <AT91SAM7.h>
#include <lib_AT91SAM7.h>
#include <openpcd.h>

#include <os/usb_handler.h>
#include <os/dbgu.h>
#include "../openpcd.h"

/* definitions for four-times oversampling */
#define REQA	0x10410441
#define WUPA	0x04041041

static const AT91PS_SSC ssc = AT91C_BASE_SSC;
static AT91PS_PDC rx_pdc;

enum ssc_mode {
	SSC_MODE_NONE,
	SSC_MODE_14443A_SHORT,
	SSC_MODE_14443A_STANDARD,
	SSC_MODE_14443B,
	SSC_MODE_EDGE_ONE_SHOT,
};

struct ssc_state {
	struct req_ctx *rx_ctx[2];
	enum ssc_mode mode;
};
static struct ssc_state ssc_state;

/* This is for four-times oversampling */
#define ISO14443A_SOF_SAMPLE	0x08
#define ISO14443A_SOF_LEN	4

static void ssc_rx_mode_set(enum ssc_mode ssc_mode)
{
	u_int8_t data_len, num_data, sync_len;
	u_int32_t start_cond;

	/* disable Rx */
	ssc->SSC_CR = AT91C_SSC_RXDIS;

	/* disable all Rx related interrupt sources */
	AT91F_SSC_DisableIt(ssc, ssc->SSC_IDR = AT91C_SSC_RXRDY | 
			    AT91C_SSC_OVRUN | AT91C_SSC_ENDRX |
			    AT91C_SSC_RXBUFF | AT91C_SSC_RXSYN |
			    AT91C_SSC_CP0 | AT91C_SSC_CP1);

	switch (ssc_mode) {
	case SSC_MODE_14443A_SHORT:
		start_cond = AT91C_SSC_START_0;
		sync_len = ISO14443A_SOF_LEN;
		ssc->SSC_RC0R = ISO14443A_SOF_SAMPLE;
		data_len = 32;
		num_data = 1;
		break;
	case SSC_MODE_14443A_STANDARD:
		start_cond = AT91C_SSC_START_0;
		sync_len = ISO14443A_SOF_LEN;
		ssc->SSC_RC0R = ISO14443A_SOF_SAMPLE;
		data_len = 1;
		num_data = 1;	/* FIXME */
		break;
	case SSC_MODE_14443B:
		/* start sampling at first falling data edge */
		//start_cond = 
		break;
	case SSC_MODE_EDGE_ONE_SHOT:
		start_cond = AT91C_SSC_START_EDGE_RF;
		sync_len = 0;
		data_len = 8;
		num_data = 50;
		break;
	default:
		return;
	}
	ssc->SSC_RFMR = (data_len-1) & 0x1f |
			(((num_data-1) & 0x0f) << 8) | 
			(((sync_len-1) & 0x0f) << 16);
	ssc->SSC_RCMR = AT91C_SSC_CKS_RK | AT91C_SSC_CKO_NONE | start_cond;

	/* Enable RX interrupts */
	AT91F_SSC_EnableIt(ssc, AT91C_SSC_OVRUN);
			   //AT91C_SSC_ENDRX | AT91C_SSC_RXBUFF);

	ssc_state.mode = ssc_mode;

	AT91F_PDC_EnableRx(rx_pdc);
}

static void ssc_tx_mode_set(enum ssc_mode ssc_mode)
{
	u_int8_t data_len, num_data, sync_len;
	u_int32_t start_cond;

	/* disable Tx */
	ssc->SSC_CR = AT91C_SSC_TXDIS;

	/* disable all Tx related interrupt sources */
	ssc->SSC_IDR = AT91C_SSC_TXRDY | AT91C_SSC_TXEMPTY | AT91C_SSC_ENDTX |
		       AT91C_SSC_TXBUFE | AT91C_SSC_TXSYN;

	switch (ssc_mode) {
	case SSC_MODE_14443A_SHORT:
		start_cond = AT91C_SSC_START_RISE_RF;
		sync_len = ISO14443A_SOF_LEN;
		data_len = 32;
		num_data = 1;
		break;
	case SSC_MODE_14443A_STANDARD:
		start_cond = AT91C_SSC_START_0;
		sync_len = ISO14443A_SOF_LEN;
		ssc->SSC_RC0R = ISO14443A_SOF_SAMPLE;
		data_len = 1;
		num_data = 1;	/* FIXME */
		break;
	}
	ssc->SSC_TFMR = (data_len-1) & 0x1f |
			(((num_data-1) & 0x0f) << 8) | 
			(((sync_len-1) & 0x0f) << 16);
	ssc->SSC_TCMR = AT91C_SSC_CKS_RK | AT91C_SSC_CKO_NONE | start_cond;

#if 0
	/* Enable RX interrupts */
	AT91F_SSC_EnableIt(ssc, AT91C_SSC_OVRUN |
			   AT91C_SSC_ENDRX | AT91C_SSC_RXBUFF);
	AT91F_PDC_EnableRx(rx_pdc);

	ssc_state.mode = ssc_mode;
#endif
}




static struct openpcd_hdr opcd_ssc_hdr = {
	.cmd	= OPENPCD_CMD_SSC_READ,
};

static inline void init_opcdhdr(struct req_ctx *rctx)
{
	memcpy(&rctx->tx.data[0], &opcd_ssc_hdr, sizeof(opcd_ssc_hdr));
	rctx->tx.tot_len = MAX_HDRSIZE + MAX_REQSIZE -1;
}

#ifdef DEBUG_SSC_REFILL
#define DEBUGR(x, args ...) DEBUGPCRF(x, ## args)
#else
#define DEBUGR(x, args ...)
#endif

static char dmabuf1[512];
static char dmabuf2[512];

/* Try to refill RX dma descriptors. Return values:
 *  0) no dma descriptors empty
 *  1) filled next/secondary descriptor
 *  2) filled both primary and secondary descriptor
 * -1) no free request contexts to use
 * -2) only one free request context, but two free descriptors
 */
static int8_t ssc_rx_refill(void)
{
#if 1
	struct req_ctx *rctx;
	
	rctx = req_ctx_find_get(RCTX_STATE_FREE, RCTX_STATE_SSC_RX_BUSY);
	if (!rctx) {
		DEBUGPCRF("no rctx for refill!");
		return -1;
	}
	init_opcdhdr(rctx);

	if (AT91F_PDC_IsRxEmpty(rx_pdc)) {
		DEBUGR("filling primary SSC RX dma ctx");
		AT91F_PDC_SetRx(rx_pdc, &rctx->rx.data[MAX_HDRSIZE],
				(sizeof(rctx->rx.data)-MAX_HDRSIZE)>>2);
		ssc_state.rx_ctx[0] = rctx;

		/* If primary is empty, secondary must be empty, too */
		rctx = req_ctx_find_get(RCTX_STATE_FREE, 
					RCTX_STATE_SSC_RX_BUSY);
		if (!rctx) {
			DEBUGPCRF("no rctx for secondary refill!");
			return -2;
		}
		init_opcdhdr(rctx);
	}

	if (AT91F_PDC_IsNextRxEmpty(rx_pdc)) {
		DEBUGR("filling secondary SSC RX dma ctx");
		AT91F_PDC_SetNextRx(rx_pdc, &rctx->rx.data[MAX_HDRSIZE],
				    (sizeof(rctx->rx.data)-MAX_HDRSIZE)>2);
		ssc_state.rx_ctx[1] = rctx;
		return 2;
	} else {
		/* we were unable to fill*/
		DEBUGPCRF("prim/secnd DMA busy, can't refill");
		req_ctx_put(rctx);	
		return 0;
	}
#else
	if (AT91F_PDC_IsRxEmpty(rx_pdc))
		AT91F_PDC_SetRx(rx_pdc, dmabuf1, sizeof(dmabuf1)>>2);
	
	if (AT91F_PDC_IsNextRxEmpty(rx_pdc))
		AT91F_PDC_SetNextRx(rx_pdc, dmabuf2, sizeof(dmabuf2)>>2);
	else
		DEBUGPCRF("prim/secnd DMA busy, can't refill");
#endif
}

#define ISO14443A_FDT_SHORT_1	1236
#define ISO14443A_FDT_SHORT_0	1172

static void ssc_irq(void)
{
	u_int32_t ssc_sr = ssc->SSC_SR;
	DEBUGP("ssc_sr=0x%08x: ", ssc_sr);

	if (ssc_sr & AT91C_SSC_OVRUN)
		DEBUGP("RX OVERRUN ");

	switch (ssc_state.mode) {
	case SSC_MODE_14443A_SHORT:
		if (ssc_sr & AT91C_SSC_RXSYN)
			DEBUGP("RXSYN ");
		if (ssc_sr & AT91C_SSC_RXRDY) {
			u_int32_t sample = ssc->SSC_RHR;	
			DEBUGP("RXRDY=0x%08x ", sample);
			/* Try to set FDT compare register ASAP */
			if (sample == REQA) {
				tc_fdt_set(ISO14443A_FDT_SHORT_0);
				/* FIXME: prepare and configure ATQA response */
			} else if (sample == WUPA) {
				tc_fdt_set(ISO14443A_FDT_SHORT_1);
				/* FIXME: prepare and configure ATQA response */
			} else 
				DEBUGP("<== unknown ");
		}	
		break;

	case SSC_MODE_14443A_STANDARD:

		if (ssc_sr & (AT91C_SSC_ENDRX | AT91C_SSC_RXBUFF)) {
#if 0
			/* Mark primary RCTX as ready to send for usb */
			req_ctx_set_state(ssc_state.rx_ctx[0], 
					  RCTX_STATE_UDP_EP2_PENDING);
			/* second buffer gets propagated to primary */
			ssc_state.rx_ctx[0] = ssc_state.rx_ctx[1];
			ssc_state.rx_ctx[1] = NULL;
#endif
	
			if (ssc_sr & AT91C_SSC_RXBUFF) {
				DEBUGPCRF("RXBUFF, shouldn't happen!");
#if 0
				req_ctx_set_state(ssc_state.rx_ctx[0],
						  RCTX_STATE_UDP_EP2_PENDING);
#endif
			}
			if (ssc_rx_refill() == -1)
				AT91F_AIC_DisableIt(ssc, AT91C_SSC_ENDRX |
						    AT91C_SSC_RXBUFF |
						    AT91C_SSC_OVRUN);
		}
		break;
	}
}


void ssc_rx_unthrottle(void)
{
	AT91F_SSC_EnableIt(ssc, AT91C_SSC_ENDRX |
			   AT91C_SSC_RXBUFF | AT91C_SSC_OVRUN);
}

void ssc_rx_start(void)
{
	DEBUGPCRF("starting SSC RX\n");	

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

static int ssc_usb_in(struct req_ctx *rctx)
{
	struct openpcd_hdr *poh = (struct openpcd_hdr *) &rctx->rx.data[0];
	struct openpcd_hdr *pih = (struct openpcd_hdr *) &rctx->tx.data[0];

	/* FIXME: implement this */
	switch (poh->cmd) {
	case OPENPCD_CMD_SSC_READ:
		break;
	case OPENPCD_CMD_SSC_WRITE:
		break;
	}

	req_ctx_put(rctx);
	return -EINVAL;
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
	/* Data bits per Data N = 32-1,  Data words per Frame = 15-1 (=60 byte)*/
	ssc->SSC_RFMR = 31 | AT91C_SSC_MSBF | (14 << 8);

	/* Enable RX interrupts */
	AT91F_SSC_EnableIt(ssc, AT91C_SSC_OVRUN |
			   AT91C_SSC_ENDRX | AT91C_SSC_RXBUFF);
	AT91F_PDC_EnableRx(rx_pdc);

	usb_hdlr_register(&ssc_usb_in, OPENPCD_CMD_CLS_SSC);
}

void ssc_fini(void)
{
	usb_hdlr_unregister(OPENPCD_CMD_CLS_SSC);
	AT91F_PDC_DisableRx(rx_pdc);
	AT91F_SSC_DisableTx(ssc);
	AT91F_SSC_DisableRx(ssc);
	AT91F_SSC_DisableIt(ssc, 0xfff);
	AT91F_PMC_DisablePeriphClock(AT91C_BASE_PMC, 
				 ((unsigned int) 1 << AT91C_ID_SSC));
}
