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
#include "queue.h"
#include "task.h"

#include "dbgu.h"
#include "led.h"
#include "cmd.h"
#include "board.h"
#include "openpicc.h"

#include "ssc_picc.h"
#include "tc_cdiv_sync.h"
#include "tc_fdt.h"

#include "pio_irq.h"
#include "usb_print.h"
#include "iso14443_layer3a.h"

//#define DEBUG_SSC_REFILL

static const AT91PS_SSC ssc = AT91C_BASE_SSC;
static AT91PS_PDC rx_pdc;
static AT91PS_PDC tx_pdc;

static ssc_dma_rx_buffer_t dma_buffers[SSC_DMA_BUFFER_COUNT];
xQueueHandle ssc_rx_queue = NULL;

ssc_dma_tx_buffer_t ssc_tx_buffer;

#define TEST_WHETHER_NOT_ENABLING_IT_HELPS
#define TEST_WHETHER_THIS_INTERRUPT_WORKS_AT_ALL

static ssc_dma_rx_buffer_t* __ramfunc ssc_find_dma_buffer(ssc_dma_buffer_state_t oldstate, 
	ssc_dma_buffer_state_t newstate)
{
	ssc_dma_rx_buffer_t* result = NULL; 
	int i=0;
	for(i=0; i<SSC_DMA_BUFFER_COUNT; i++) {
		if(dma_buffers[i].state == oldstate) {
			result = &dma_buffers[i];
			result->state = newstate;
			break;
		}
	}
	return result;
}

struct ssc_state {
	ssc_dma_rx_buffer_t* buffer[2];
	enum ssc_mode mode;
};
static struct ssc_state ssc_state;

static const struct {u_int16_t bytes; u_int16_t transfers;} ssc_dmasize[] = {
	[SSC_MODE_NONE]			= {0, 0},
	[SSC_MODE_14443A_SHORT]		= {ISO14443A_SHORT_TRANSFER_SIZE/8, 1},	/* 1 transfer of ISO14443A_SHORT_LEN bits */
	[SSC_MODE_14443A_STANDARD]	= {16*4, 16},	/* 16 transfers of 32 bits (maximum number), resulting in 512 samples */
	[SSC_MODE_14443B]		= {16*4, 16},	/* 64 bytes */
	[SSC_MODE_EDGE_ONE_SHOT] 	= {16*4, 16},	/* 64 bytes */
	[SSC_MODE_CONTINUOUS]		= {511*4, 511},	/* 2044 bytes */
};

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
		data_len = ISO14443A_SHORT_LEN;
		num_data = 2;
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
			(((sync_len-1) & 0x0f) << 16)
			//| AT91C_SSC_MSBF
			;
	ssc->SSC_RCMR = AT91C_SSC_CKS_RK | AT91C_SSC_CKO_NONE | 
			(0x2 << 6) | AT91C_SSC_CKI | start_cond;

	/* Enable Rx DMA */
	AT91F_PDC_EnableRx(rx_pdc);

	/* Enable RX interrupts */
#ifdef TEST_WHETHER_NOT_ENABLING_IT_HELPS
/*
	AT91F_SSC_EnableIt(ssc, AT91C_SSC_OVRUN | AT91C_SSC_CP0 |
			   AT91C_SSC_ENDRX | AT91C_SSC_RXBUFF);*/
#endif
out_set_mode:
	ssc_state.mode = ssc_mode;
}

/* For some reason AT91C_SSC_START_RISE_RF (or AT91C_SSC_START_HIGH_RF or ...) doesn't
 * work as a start condition. Instead we'll configure TF as a PIO input pin, enable
 * a PIO change interrupt, have Fast Forcing enabled for the PIO change interrupt and
 * then activate the SSC TX in the FIQ handler on rising TF. ssc_tx_pending is queried
 * by the fiq handler to see whether to start the transmitter. */
#define USE_SSC_TX_TF_WORKAROUND
volatile u_int32_t ssc_tx_pending = 0;
void ssc_tf_irq(u_int32_t pio);
void ssc_tx_start(ssc_dma_tx_buffer_t *buf)
{
	u_int8_t data_len, num_data, sync_len;
	u_int32_t start_cond;

	/* disable Tx */
	AT91F_SSC_DisableTx(AT91C_BASE_SSC);

	/* disable all Tx related interrupt sources */
	AT91F_SSC_DisableIt(ssc, SSC_TX_IRQ_MASK);

#ifdef USE_SSC_TX_TF_WORKAROUND
	start_cond = AT91C_SSC_START_CONTINOUS;
#else
	start_cond = AT91C_SSC_START_RISE_RF;
#endif
	sync_len = 1;
	data_len = 32;
	num_data = buf->len/(data_len/8); /* FIXME This is broken for more than 64 bytes */
	
	ssc->SSC_TFMR = ((data_len-1) & 0x1f) |
			(((num_data-1) & 0x0f) << 8) | 
			(((sync_len-1) & 0x0f) << 16);
	ssc->SSC_TCMR = 0x01 | AT91C_SSC_CKO_NONE | start_cond;
	
	AT91F_PDC_SetTx(tx_pdc, buf->data, num_data);

#ifdef TEST_WHETHER_NOT_ENABLING_IT_HELPS
	AT91F_SSC_EnableIt(ssc, AT91C_SSC_TXSYN | AT91C_SSC_ENDTX | AT91C_SSC_TXBUFE);
#endif
	/* Enable DMA */
	AT91F_PDC_EnableTx(tx_pdc);
	/* Start Transmission */
#ifndef USE_SSC_TX_TF_WORKAROUND
	AT91F_SSC_EnableTx(AT91C_BASE_SSC);
#else
	ssc_tx_pending = 1;
	pio_irq_enable(OPENPICC_SSC_TF);
	if(AT91F_PIO_IsInputSet(AT91C_BASE_PIOA, OPENPICC_SSC_TF)) {
		/* TF was probably already high when we enabled the PIO change interrupt for it. */
		ssc_tf_irq(OPENPICC_SSC_TF);
	}
#endif
}

#ifdef USE_SSC_TX_TF_WORKAROUND
void ssc_tf_irq(u_int32_t pio) {
	(void)pio;
	pio_irq_disable(OPENPICC_SSC_TF);
	if(ssc_tx_pending) { /* Transmit has not yet been started by the FIQ */
		AT91F_SSC_EnableTx(AT91C_BASE_SSC);
		ssc_tx_pending = 0;
	}
}
#endif


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

static volatile portBASE_TYPE overflows;
portBASE_TYPE ssc_get_overflows(void) {
	return overflows;
}

int ssc_count_free(void) {
	int i,free = 0;
	for(i=0; i<SSC_DMA_BUFFER_COUNT; i++) {
		if(dma_buffers[i].state == FREE) free++;
	}
	return free;
}

static ssc_irq_ext_t irq_extension = NULL;
ssc_irq_ext_t ssc_set_irq_extension(ssc_irq_ext_t ext_handler) {
	ssc_irq_ext_t old = irq_extension;
	irq_extension = ext_handler;
	return old;
}

static int __ramfunc __ssc_rx_refill(int secondary)
{
	ssc_dma_rx_buffer_t *buffer;
	
	buffer = ssc_find_dma_buffer(FREE, PENDING);
	if (!buffer) {
		DEBUGP("no_rctx_for_refill! ");
		overflows++;
		return -1;
	}
	//init_opcdhdr(buffer);
	DEBUGR("filling SSC RX%u dma ctx: %u (len=%u) ", secondary,
		req_ctx_num(buffer), buffer->size);
	buffer->len = ssc_dmasize[ssc_state.mode].bytes;
	if (secondary) {
		AT91F_PDC_SetNextRx(rx_pdc, buffer->data,
				    ssc_dmasize[ssc_state.mode].transfers);
		ssc_state.buffer[1] = buffer;
	} else {
		AT91F_PDC_SetRx(rx_pdc, buffer->data,
				ssc_dmasize[ssc_state.mode].transfers);
		ssc_state.buffer[0] = buffer;
	}

	//tc_cdiv_sync_reset();
	
	return 0;
}

static void __ramfunc ssc_irq(void) __attribute__ ((naked));
static void __ramfunc ssc_irq(void)
{
	portENTER_SWITCHING_ISR();
	vLedSetRed(1);
	portBASE_TYPE task_woken = pdFALSE;

#ifdef TEST_WHETHER_THIS_INTERRUPT_WORKS_AT_ALL
	u_int32_t ssc_sr = ssc->SSC_SR;
	int i, emptyframe = 0;
	u_int32_t *tmp;
	ssc_dma_rx_buffer_t *inbuf=NULL;
	DEBUGP("ssc_sr=0x%08x, mode=%u: ", ssc_sr, ssc_state.mode);
	
	if (ssc_sr & AT91C_SSC_ENDRX) {
#if 1
		/* in a one-shot sample, we don't want to keep
		 * sampling further after having received the first
		 * packet.  */
		if (ssc_state.mode == SSC_MODE_EDGE_ONE_SHOT || ssc_state.mode == SSC_MODE_14443A_SHORT
			|| ssc_state.mode == SSC_MODE_14443A_STANDARD) {
			DEBUGP("DISABLE_RX ");
			ssc_rx_stop();
			vLedSetGreen(1);
		}
		//AT91F_SSC_DisableIt(AT91C_BASE_SSC, SSC_RX_IRQ_MASK);
#endif
		/* Ignore empty frames */
		if (ssc_state.mode == SSC_MODE_CONTINUOUS) {
			tmp = (u_int32_t*)ssc_state.buffer[0]->data;
			emptyframe = 1;
			for(i = (ssc_state.buffer[0]->len) / 4 - 8/*WTF?*/; i > 0; i--) {
				if( *tmp++ != 0x0 ) {
					DEBUGPCR("NONEMPTY(%08x, %i): %08x", tmp, i, *(tmp-1));
					emptyframe = 0;
					break;
				} else {
					//DEBUGPCR("DUNNO(%08x, %i): %08x", tmp, i, tmp[i]);
				}
			}
		}
		//DEBUGP("Sending primary RCTX(%u, len=%u) ", req_ctx_num(ssc_state.rx_ctx[0]), ssc_state.rx_ctx[0]->tot_len);
		/* Mark primary RCTX as ready to send for usb */
		if(!emptyframe) {
			//unsigned int i;
			DEBUGP("NONEMPTY");
			//gaportENTER_CRITICAL();
			ssc_state.buffer[0]->state = FULL;
			//gaportEXIT_CRITICAL();
			task_woken = xQueueSendFromISR(ssc_rx_queue, &ssc_state.buffer[0], task_woken);
		} else {
			DEBUGP("EMPTY");
			//gaportENTER_CRITICAL();
			ssc_state.buffer[0]->state = FREE;
			//gaportEXIT_CRITICAL();			
		}

		/* second buffer gets propagated to primary */
		inbuf = ssc_state.buffer[0];
		ssc_state.buffer[0] = ssc_state.buffer[1];
		ssc_state.buffer[1] = NULL;
		if(ssc_state.mode == SSC_MODE_14443A_SHORT) {
			// Stop sampling here
			ssc_rx_stop();
		} else {
			if (ssc_sr & AT91C_SSC_RXBUFF) {
	// FIXME
				DEBUGP("RXBUFF! ");
				if (ssc_state.buffer[0]) {
					//DEBUGP("Sending secondary RCTX(%u, len=%u) ", req_ctx_num(ssc_state.rx_ctx[0]), ssc_state.rx_ctx[0]->tot_len);
					//gaportENTER_CRITICAL();
					ssc_state.buffer[0]->state = FULL;
					//gaportEXIT_CRITICAL();
					task_woken = xQueueSendFromISR(ssc_rx_queue, &ssc_state.buffer[0], task_woken);
				}
				if (__ssc_rx_refill(0) == -1)
					AT91F_SSC_DisableIt(ssc, AT91C_SSC_ENDRX |
							    AT91C_SSC_RXBUFF |
							    AT91C_SSC_OVRUN);
			}
	
			if (__ssc_rx_refill(1) == -1)
				AT91F_SSC_DisableIt(ssc, AT91C_SSC_ENDRX |
						    AT91C_SSC_RXBUFF |
						    AT91C_SSC_OVRUN);
		}
	}
	
	if (ssc_sr & AT91C_SSC_OVRUN)
		DEBUGP("RX_OVERRUN ");

	if (ssc_sr & AT91C_SSC_CP0)
		DEBUGP("CP0 ");
	
	if (ssc_sr & AT91C_SSC_TXSYN)
		usb_print_string_f("TXSYN ", 0);
	
	if(irq_extension != NULL)
		irq_extension(ssc_sr, ssc_state.mode, inbuf?inbuf->data:NULL);


#endif
	DEBUGPCR("I");
	AT91F_AIC_ClearIt(AT91C_ID_SSC);
	AT91F_AIC_AcknowledgeIt();
	
	vLedSetRed(0);
	portEXIT_SWITCHING_ISR(task_woken);
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
#ifdef TEST_WHETHER_NOT_ENABLING_IT_HELPS
	AT91F_SSC_EnableIt(ssc, AT91C_SSC_ENDRX | AT91C_SSC_CP0 |
			   AT91C_SSC_RXBUFF | AT91C_SSC_OVRUN);
#endif
}

void ssc_rx_start(void)
{
	//DEBUGPCRF("starting SSC RX\n");
	
	__ssc_rx_refill(0);
	if(ssc_state.mode != SSC_MODE_14443A_SHORT) __ssc_rx_refill(1);
	
	/* Enable Reception */
#ifdef TEST_WHETHER_NOT_ENABLING_IT_HELPS
	AT91F_SSC_EnableIt(ssc, AT91C_SSC_ENDRX | AT91C_SSC_CP0 |
			   AT91C_SSC_RXBUFF | AT91C_SSC_OVRUN);
#endif
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
			    OPENPICC_SSC_CLOCK | OPENPICC_SSC_TF, 0);
#ifdef USE_SSC_TX_TF_WORKAROUND
	AT91F_PIO_CfgInput(AT91C_BASE_PIOA, OPENPICC_SSC_TF);
	pio_irq_register(OPENPICC_SSC_TF, ssc_tf_irq);
#endif
	
	tx_pdc = (AT91PS_PDC) &(ssc->SSC_RPR);
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
	
	if(ssc_rx_queue == NULL)
		ssc_rx_queue = xQueueCreate(10, sizeof(ssc_state.buffer[0]));

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
			      AT91C_AIC_SRCTYPE_INT_POSITIVE_EDGE, (THandler)&ssc_irq);

	/* don't divide clock inside SSC, we do that in tc_cdiv */
	ssc->SSC_CMR = 0;

#if 0
	ssc->SSC_RCMR = AT91C_SSC_CKS_RK | AT91C_SSC_CKO_NONE |
			AT91C_SSC_CKI | AT91C_SSC_START_CONTINOUS;
	/* Data bits per Data N = 32-1
	 * Data words per Frame = 15-1 (=60 byte)*/
	ssc->SSC_RFMR = 31 | AT91C_SSC_MSBF | (14 << 8);
#endif
	
	ssc_rx_mode_set(SSC_MODE_NONE);
	ssc_state.buffer[0] = ssc_state.buffer[1] = NULL;
	
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
	AT91F_PDC_DisableTx(tx_pdc);
	AT91F_SSC_DisableTx(ssc);
	AT91F_SSC_DisableRx(ssc);
	AT91F_SSC_DisableIt(ssc, 0xfff);
	AT91F_PMC_DisablePeriphClock(AT91C_BASE_PMC, 
				 ((unsigned int) 1 << AT91C_ID_SSC));
}
