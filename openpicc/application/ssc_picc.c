/* AT91SAM7 SSC controller routines for OpenPICC
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
 * We use SSC for both TX and RX side.
 *
 * RX side is interconnected with demodulated carrier 
 *
 * TX side is interconnected with load modulation circuitry
 */

//#undef DEBUG

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <AT91SAM7.h>
#include <lib_AT91SAM7.h>

#include <FreeRTOS.h>
//#include <os/usb_handler.h>
#include "dbgu.h"
#include "led.h"
#include "board.h"
#include "openpicc.h"

#include "ssc_picc.h"
#include "tc_cdiv_sync.h"
#include "tc_fdt.h"

//#define DEBUG_SSC_REFILL

/* definitions for four-times oversampling */
#define REQA	0x10410441
#define WUPA	0x04041041

static const AT91PS_SSC ssc = AT91C_BASE_SSC;
static AT91PS_PDC rx_pdc;

struct ssc_state {
	struct req_ctx *rx_ctx[2];
	enum ssc_mode mode;
};
static struct ssc_state ssc_state;

static const u_int16_t ssc_dmasize[] = {
	[SSC_MODE_NONE]			= 16,
	[SSC_MODE_14443A_SHORT]		= 16,	/* 64 bytes */
	[SSC_MODE_14443A_STANDARD]	= 16,	/* 64 bytes */
	[SSC_MODE_14443B]		= 16,	/* 64 bytes */
	[SSC_MODE_EDGE_ONE_SHOT] 	= 16,	/* 64 bytes */
	[SSC_MODE_CONTINUOUS]		= 511,	/* 2044 bytes */
};

/* This is for four-times oversampling */
#define ISO14443A_SOF_SAMPLE	0x01
#define ISO14443A_SOF_LEN	4

#define SSC_RX_IRQ_MASK	(AT91C_SSC_RXRDY | 	\
			 AT91C_SSC_OVRUN | 	\
			 AT91C_SSC_ENDRX |	\
			 AT91C_SSC_RXBUFF |	\
			 AT91C_SSC_RXSYN |	\
			 AT91C_SSC_CP0 |	\
			 AT91C_SSC_CP1)

#define SSC_TX_IRQ_MASK (AT91C_SSC_TXRDY |	\
			 AT91C_SSC_TXEMPTY | 	\
			 AT91C_SSC_ENDTX |	\
			 AT91C_SSC_TXBUFE |	\
			 AT91C_SSC_TXSYN)

void ssc_rx_mode_set(enum ssc_mode ssc_mode)
{
	u_int8_t data_len=0, num_data=0, sync_len=0;
	u_int32_t start_cond=0;

	/* disable Rx and all Rx interrupt sources */
	AT91F_SSC_DisableRx(AT91C_BASE_SSC);
	AT91F_SSC_DisableIt(ssc, SSC_RX_IRQ_MASK);

	switch (ssc_mode) {
	case SSC_MODE_14443A_SHORT:
		start_cond = AT91C_SSC_START_0;
		sync_len = ISO14443A_SOF_LEN;
		ssc->SSC_RC0R = ISO14443A_SOF_SAMPLE;
		data_len = 32;
		num_data = 16;
		break;
	case SSC_MODE_14443A_STANDARD:
		start_cond = AT91C_SSC_START_0;
		sync_len = ISO14443A_SOF_LEN;
		ssc->SSC_RC0R = ISO14443A_SOF_SAMPLE;
		data_len = 32;
		num_data = 16;	/* FIXME */
		break;
	case SSC_MODE_14443B:
		/* start sampling at first falling data edge */
		//start_cond = 
		break;
	case SSC_MODE_EDGE_ONE_SHOT:
	case SSC_MODE_CONTINUOUS:
		/* unfortunately we don't have RD and RF interconnected
		 * (at least not yet in the current hardware) */
		//start_cond = AT91C_SSC_START_EDGE_RF;
		start_cond = AT91C_SSC_START_CONTINOUS;
				//AT91C_SSC_START_RISE_RF;
		sync_len = 0;
		data_len = 32;
		num_data = 16;
		break;
	case SSC_MODE_NONE:
		goto out_set_mode;
		break;
	}
	//ssc->SSC_RFMR = AT91C_SSC_MSBF | (data_len-1) & 0x1f |
	ssc->SSC_RFMR = ((data_len-1) & 0x1f) |
			(((num_data-1) & 0x0f) << 8) | 
			(((sync_len-1) & 0x0f) << 16);
	ssc->SSC_RCMR = AT91C_SSC_CKS_RK | AT91C_SSC_CKO_NONE | 
			(0x2 << 6) | AT91C_SSC_CKI | start_cond;

	/* Enable Rx DMA */
	AT91F_PDC_EnableRx(rx_pdc);

	/* Enable RX interrupts */
	AT91F_SSC_EnableIt(ssc, AT91C_SSC_OVRUN | AT91C_SSC_CP0 |
			   AT91C_SSC_ENDRX | AT91C_SSC_RXBUFF);
out_set_mode:
	ssc_state.mode = ssc_mode;
}

static void ssc_tx_mode_set(enum ssc_mode ssc_mode)
{
	u_int8_t data_len, num_data, sync_len;
	u_int32_t start_cond;

	/* disable Tx */
	AT91F_SSC_DisableTx(AT91C_BASE_SSC);

	/* disable all Tx related interrupt sources */
	AT91F_SSC_DisableIt(ssc, SSC_TX_IRQ_MASK);

	switch (ssc_mode) {
	case SSC_MODE_14443A_SHORT:
		start_cond = AT91C_SSC_START_RISE_RF;
		sync_len = 0;
		data_len = 32;
		num_data = 1;
		break;
	case SSC_MODE_14443A_STANDARD:
		start_cond = AT91C_SSC_START_0;
		sync_len = ISO14443A_SOF_LEN;
		ssc->SSC_RC0R = ISO14443A_SOF_SAMPLE;
		data_len = 32;
		num_data = 1;	/* FIXME */
		break;
	default:
		break;
	}
	ssc->SSC_TFMR = ((data_len-1) & 0x1f) |
			(((num_data-1) & 0x0f) << 8) | 
			(((sync_len-1) & 0x0f) << 16);
	ssc->SSC_TCMR = AT91C_SSC_CKS_RK | AT91C_SSC_CKO_NONE | start_cond;

	AT91F_SSC_EnableIt(ssc, AT91C_SSC_TXSYN);
	AT91F_SSC_EnableTx(AT91C_BASE_SSC);
#if 0
	/* Enable RX interrupts */
	AT91F_SSC_EnableIt(ssc, AT91C_SSC_OVRUN |
			   AT91C_SSC_ENDTX | AT91C_SSC_TXBUFE);
	AT91F_PDC_EnableTx(tx_pdc);

	ssc_state.mode = ssc_mode;
#endif
}




//static struct openpcd_hdr opcd_ssc_hdr = {
//	.cmd	= OPENPCD_CMD_SSC_READ,
//};

//static inline void init_opcdhdr(struct req_ctx *rctx)
//{
//	memcpy(rctx->data, &opcd_ssc_hdr, sizeof(opcd_ssc_hdr));
//	rctx->tot_len = sizeof(opcd_ssc_hdr);
//}

#define DEBUG_SSC_REFILL 1
#ifdef DEBUG_SSC_REFILL
#define DEBUGR(x, args ...) DEBUGPCRF(x, ## args)
#else
#define DEBUGR(x, args ...)
#endif

static int __ramfunc __ssc_rx_refill(int secondary)
{
	(void)secondary;
// FIXME
/*	struct req_ctx *rctx;

	rctx = req_ctx_find_get(1, RCTX_STATE_FREE, RCTX_STATE_SSC_RX_BUSY);
	if (!rctx) {
		DEBUGP("no_rctx_for_refill! ");
		return -1;
	}
	init_opcdhdr(rctx);
	DEBUGR("filling SSC RX%u dma ctx: %u (len=%u) ", secondary,
		req_ctx_num(rctx), rctx->size);
	rctx->tot_len = ssc_dmasize[ssc_state.mode]*4 + 
			sizeof(struct openpcd_hdr);
	if (secondary) {
		AT91F_PDC_SetNextRx(rx_pdc, rctx->data+MAX_HDRSIZE,
				    ssc_dmasize[ssc_state.mode]);
		ssc_state.rx_ctx[1] = rctx;
	} else {
		AT91F_PDC_SetRx(rx_pdc, rctx->data+MAX_HDRSIZE,
				ssc_dmasize[ssc_state.mode]);
		ssc_state.rx_ctx[0] = rctx;
	}
*/
	tc_cdiv_sync_reset();
	
	return 0;
}

#if 0
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
	struct req_ctx *rctx;
	DEBUGR("refill ");
#if 1
	rctx = req_ctx_find_get(1, RCTX_STATE_FREE, RCTX_STATE_SSC_RX_BUSY);
	DEBUGP("SSC_SR=0x%08x ", ssc->SSC_SR);
	if (AT91F_PDC_IsRxEmpty(rx_pdc)) {
		DEBUGR("filling primary SSC RX dma ctx: %u (len=%u) ",
			req_ctx_num(rctx), rctx->size);
		rctx->tot_len = rctx->size;
		AT91F_PDC_SetRx(rx_pdc, rctx->data+MAX_HDRSIZE,
				(rctx->size-MAX_HDRSIZE)>>2);
		ssc_state.rx_ctx[0] = rctx;

		/* If primary is empty, secondary must be empty, too */
		rctx = req_ctx_find_get(1, RCTX_STATE_FREE, 
					RCTX_STATE_SSC_RX_BUSY);
		if (!rctx) {
			DEBUGPCRF("no rctx for secondary refill!");
			return -2;
		}
		init_opcdhdr(rctx);
	}

	if (AT91F_PDC_IsNextRxEmpty(rx_pdc)) {
		DEBUGR("filling secondary SSC RX dma ctx: %u (len=%u) ",
			req_ctx_num(rctx), rctx->size);
		rctx->tot_len = rctx->size;
		AT91F_PDC_SetNextRx(rx_pdc, rctx->data+MAX_HDRSIZE,
				    (rctx->size-MAX_HDRSIZE)>2);
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
#endif

#define ISO14443A_FDT_SHORT_1	1236
#define ISO14443A_FDT_SHORT_0	1172

static void __ramfunc ssc_irq_inner(void)
{
	u_int32_t ssc_sr = ssc->SSC_SR;
//	int i, *tmp, emptyframe = 0;
	DEBUGP("ssc_sr=0x%08x, mode=%u: ", ssc_sr, ssc_state.mode);

	if (ssc_sr & AT91C_SSC_ENDRX) {
#if 1
		/* in a one-shot sample, we don't want to keep
		 * sampling further after having received the first
		 * packet.  */
		if (ssc_state.mode == SSC_MODE_EDGE_ONE_SHOT) {
			DEBUGP("DISABLE_RX ");
			ssc_rx_stop();
		}
		//AT91F_SSC_DisableIt(AT91C_BASE_SSC, SSC_RX_IRQ_MASK);
#endif
#if 0
/* Experimental start SSC on frame, stop on FFFFFFFF */
		if (ssc_state.mode == SSC_MODE_CONTINUOUS) {
			//ssc->SSC_RCMR = (ssc->SSC_RCMR & ~AT91C_SSC_START) | AT91C_SSC_START_CONTINOUS;
			tmp = (u_int32_t*)ssc_state.rx_ctx[0]->data;
			for(i = ssc_state.rx_ctx[0]->size / 4; i >= 0 ; i--) {
				if( *tmp++ == 0xFFFFFFFF ) {
					*(tmp-1) = 0xAAAAAAAA; // debug marker
					/* No modulation for a long time, stop sampling 
					 * and prepare for next frame */
					DEBUGP("RESTART RX ");
					ssc_rx_stop();
					ssc_rx_mode_set(ssc_state.mode);
					ssc_rx_start();
					led_toggle(1);
					break;
				}
			}
		}
#endif
		/* Ignore empty frames */
		if (ssc_state.mode == SSC_MODE_CONTINUOUS) {
// FIXME
//			tmp = (u_int32_t*)ssc_state.rx_ctx[0]->data + MAX_HDRSIZE;
//			emptyframe = 1;
//			for(i = (ssc_state.rx_ctx[0]->size-MAX_HDRSIZE) / 4 - 8/*WTF?*/; i > 0; i--) {
//				if( *tmp++ != 0xFFFFFFFF ) {
//					DEBUGPCR("NONEMPTY(%08x, %i): %08x", tmp, i, *(tmp-1));
//					emptyframe = 0;
//					break;
//				} else {
//					//DEBUGPCR("DUNNO(%08x, %i): %08x", tmp, i, tmp[i]);
//				}
//			}
		}
		//DEBUGP("Sending primary RCTX(%u, len=%u) ", req_ctx_num(ssc_state.rx_ctx[0]), ssc_state.rx_ctx[0]->tot_len);
		/* Mark primary RCTX as ready to send for usb */
//		if(!emptyframe) {
// FIXME
//			DEBUGP("NONEMPTY");
//			req_ctx_set_state(ssc_state.rx_ctx[0], 
//					  RCTX_STATE_UDP_EP2_PENDING);
//					  //RCTX_STATE_FREE);
//		} else {
//			DEBUGP("EMPTY");
//			req_ctx_put(ssc_state.rx_ctx[0]);
//		}

		/* second buffer gets propagated to primary */
		ssc_state.rx_ctx[0] = ssc_state.rx_ctx[1];
		ssc_state.rx_ctx[1] = NULL;
		if (ssc_sr & AT91C_SSC_RXBUFF) {
//			DEBUGP("RXBUFF! ");
//			if (ssc_state.rx_ctx[0]) {
//				//DEBUGP("Sending secondary RCTX(%u, len=%u) ", req_ctx_num(ssc_state.rx_ctx[0]), ssc_state.rx_ctx[0]->tot_len);
//				req_ctx_set_state(ssc_state.rx_ctx[0],
//						  RCTX_STATE_UDP_EP2_PENDING);
//						  //RCTX_STATE_FREE);
//			}
			if (__ssc_rx_refill(0) == -1)
				AT91F_SSC_DisableIt(ssc, AT91C_SSC_ENDRX |
						    AT91C_SSC_RXBUFF |
						    AT91C_SSC_OVRUN);
		}

		if (__ssc_rx_refill(1) == -1)
			AT91F_SSC_DisableIt(ssc, AT91C_SSC_ENDRX |
					    AT91C_SSC_RXBUFF |
					    AT91C_SSC_OVRUN);

//		udp_refill_ep(2);

#if 0
			if (__ssc_rx_refill(1) == -1)
				AT91F_SSC_DisableIt(ssc, AT91C_SSC_ENDRX |
						    AT91C_SSC_RXBUFF |
						    AT91C_SSC_OVRUN);
#endif
	}
	
	if (ssc_sr & AT91C_SSC_OVRUN)
		DEBUGP("RX_OVERRUN ");

	if (ssc_sr & AT91C_SSC_CP0)
		DEBUGP("CP0 ");
	
	if (ssc_sr & AT91C_SSC_TXSYN)
		DEBUGP("TXSYN ");
#if 0
	led_toggle(1);

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
	case SSC_MODE_EDGE_ONE_SHOT:
		DEBUGP("ONE_SHOT ");
		break;
	default:
		DEBUGP("UNKNOWN_MODE ");
		break;
	}

#endif
	DEBUGPCR("I");
	AT91F_AIC_ClearIt(AT91C_ID_SSC);
}

static void __ramfunc ssc_irq_outer(void) __attribute__ ((naked));
static void __ramfunc ssc_irq_outer(void) {
	portSAVE_CONTEXT();
	ssc_irq_inner();
	portRESTORE_CONTEXT();
}

void ssc_print(void)
{
	DEBUGP("PDC_RPR=0x%08x ", rx_pdc->PDC_RPR);
	DEBUGP("PDC_RCR=0x%08x ", rx_pdc->PDC_RCR);
	DEBUGP("PDC_RNPR=0x%08x ", rx_pdc->PDC_RNPR);
	DEBUGP("PDC_RNCR=0x%08x ", rx_pdc->PDC_RNCR);
}


void ssc_rx_unthrottle(void)
{
	AT91F_SSC_EnableIt(ssc, AT91C_SSC_ENDRX | AT91C_SSC_CP0 |
			   AT91C_SSC_RXBUFF | AT91C_SSC_OVRUN);
}

void ssc_rx_start(void)
{
	//DEBUGPCRF("starting SSC RX\n");	

	/* Enable Reception */
	AT91F_SSC_EnableIt(ssc, AT91C_SSC_ENDRX | AT91C_SSC_CP0 |
			   AT91C_SSC_RXBUFF | AT91C_SSC_OVRUN);
	AT91F_SSC_EnableRx(AT91C_BASE_SSC);
	
	/* Clear the flipflop */
	tc_cdiv_sync_reset();
}

void ssc_rx_stop(void)
{
	/* Disable reception */
	AT91F_SSC_DisableRx(AT91C_BASE_SSC);
}

void ssc_tx_init(void)
{
	/* IMPORTANT: Disable PA23 (PWM0) output, since it is connected to 
	 * PA17 !! */
	AT91F_PIO_CfgInput(AT91C_BASE_PIOA, OPENPICC_MOD_PWM);
	AT91F_PIO_CfgPeriph(AT91C_BASE_PIOA, OPENPICC_MOD_SSC | 
			    OPENPICC_SSC_DATA | OPENPICC_SSC_DATA |
			    AT91C_PIO_PA15, 0);
	
	ssc_tx_mode_set(SSC_MODE_14443A_SHORT);
}

//static int ssc_usb_in(struct req_ctx *rctx)
//{
// FIXME
//	struct openpcd_hdr *poh = (struct openpcd_hdr *) rctx->data;
//
//	switch (poh->cmd) {
//	case OPENPCD_CMD_SSC_READ:
//		/* FIXME: allow host to specify mode */
//		ssc_rx_mode_set(SSC_MODE_EDGE_ONE_SHOT);
//		ssc_rx_start();
//		req_ctx_put(rctx);
//		return 0;
//		break;
//	case OPENPCD_CMD_SSC_WRITE:
//		/* FIXME: implement this */
//		//ssc_tx_start()
//		break;
//	default:
//		return USB_ERR(USB_ERR_CMD_UNKNOWN);
//		break;
//	}
//
//	return (poh->flags & OPENPCD_FLAG_RESPOND) ? USB_RET_RESPOND : 0;
//}

void ssc_rx_init(void)
{ 
	tc_cdiv_sync_init();
	tc_cdiv_sync_enable();

	rx_pdc = (AT91PS_PDC) &(ssc->SSC_RPR);

	AT91F_SSC_CfgPMC();

	AT91F_PIO_CfgPeriph(AT91C_BASE_PIOA, 
			    OPENPICC_SSC_DATA | OPENPICC_SSC_CLOCK |
			    OPENPICC_PIO_FRAME,
			    0);
	
	AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, OPENPICC_PIO_SSC_DATA_CONTROL);
	AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, OPENPICC_PIO_SSC_DATA_CONTROL);

	AT91F_AIC_ConfigureIt(AT91C_ID_SSC,
			      OPENPICC_IRQ_PRIO_SSC,
			      AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL, (THandler)&ssc_irq_outer);

	/* don't divide clock inside SSC, we do that in tc_cdiv */
	ssc->SSC_CMR = 0;

#if 0
	ssc->SSC_RCMR = AT91C_SSC_CKS_RK | AT91C_SSC_CKO_NONE |
			AT91C_SSC_CKI | AT91C_SSC_START_CONTINOUS;
	/* Data bits per Data N = 32-1
	 * Data words per Frame = 15-1 (=60 byte)*/
	ssc->SSC_RFMR = 31 | AT91C_SSC_MSBF | (14 << 8);
#endif
	
	__ssc_rx_refill(0);
	__ssc_rx_refill(1);

	ssc_rx_mode_set(SSC_MODE_NONE);
#if 0
	AT91F_PDC_EnableRx(rx_pdc);

	/* Enable RX interrupts */
	AT91F_SSC_EnableIt(ssc, AT91C_SSC_OVRUN |
			   AT91C_SSC_ENDRX | AT91C_SSC_RXBUFF);
#endif
	/* FIXME: This is hardcoded for REQA 0x26 */
	tc_fdt_set(ISO14443A_FDT_SHORT_0);

	AT91F_AIC_EnableIt(AT91C_ID_SSC);

	//usb_hdlr_register(&ssc_usb_in, OPENPCD_CMD_CLS_SSC);

	DEBUGP("\r\n");
}

void ssc_fini(void)
{
//	usb_hdlr_unregister(OPENPCD_CMD_CLS_SSC);
	AT91F_PDC_DisableRx(rx_pdc);
	AT91F_SSC_DisableTx(ssc);
	AT91F_SSC_DisableRx(ssc);
	AT91F_SSC_DisableIt(ssc, 0xfff);
	AT91F_PMC_DisablePeriphClock(AT91C_BASE_PMC, 
				 ((unsigned int) 1 << AT91C_ID_SSC));
}
