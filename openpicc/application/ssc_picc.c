/* AT91SAM7 SSC controller routines for OpenPICC
 * (C) 2006 by Harald Welte <hwelte@hmw-consulting.de>
 * (C) 2007 Henryk Plötz <henryk@ploetzli.ch>
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

#define DEBUG_SSC_REFILL 1
#ifdef DEBUG_SSC_REFILL
#define DEBUGR(x, args ...) DEBUGPCRF(x, ## args)
#else
#define DEBUGR(x, args ...)
#endif


static const AT91PS_SSC ssc = AT91C_BASE_SSC;
static AT91PS_PDC rx_pdc;
static AT91PS_PDC tx_pdc;

xQueueHandle ssc_rx_queue = NULL;

struct ssc_state {
	ssc_dma_rx_buffer_t* buffer[2];
	enum ssc_mode mode;
};
static struct ssc_state ssc_state;

/* Note: Only use 8, 16 or 32 for the transfersize. (These are the sizes used by the PDC and even though
 * the SSC supports different sizes, all PDC tranfers will be either 8, 16 or 32, rounding up.) */
static const ssc_mode_def ssc_sizes[] = {
	/* Undefined, no size set */
	[SSC_MODE_NONE]		   = {SSC_MODE_NONE, 0, 0, 0},
	/* 14443A Short Frame: 1 transfer of ISO14443A_SHORT_LEN bits */
	[SSC_MODE_14443A_SHORT]	   = {SSC_MODE_14443A_SHORT, ISO14443A_SHORT_LEN, ISO14443A_SHORT_TRANSFER_SIZE, 1},
	/* 14443A Standard Frame: FIXME 16 transfers of 32 bits (maximum number), resulting in 512 samples */ 
	[SSC_MODE_14443A_STANDARD] = {SSC_MODE_14443A_STANDARD, 32, 32, 16},
	/* 14443A Frame, don't care if standard or short */
	[SSC_MODE_14443A]          = {SSC_MODE_14443A, ISO14443A_SAMPLE_LEN, 8, ISO14443A_MAX_RX_FRAME_SIZE_IN_BITS},
	[SSC_MODE_14443B]	   = {SSC_MODE_14443B, 32, 32, 16},	/* 64 bytes */
	[SSC_MODE_EDGE_ONE_SHOT]   = {SSC_MODE_EDGE_ONE_SHOT, 32, 32,  16},	/* 64 bytes */
	[SSC_MODE_CONTINUOUS]	   = {SSC_MODE_CONTINUOUS, 32, 32,  511},	/* 2044 bytes */
};

/* ************** SSC BUFFER HANDLING *********************** */
static ssc_dma_rx_buffer_t dma_buffers[SSC_DMA_BUFFER_COUNT];
ssc_dma_tx_buffer_t ssc_tx_buffer;

static volatile int overflows;
static volatile int ssc_buffer_errors;
static volatile int late_frames = 0;

static int ssc_count_free(void) {
	int i,free = 0;
	for(i=0; i<SSC_DMA_BUFFER_COUNT; i++) {
		if(dma_buffers[i].state == FREE) free++;
	}
	return free;
}

int ssc_get_metric(ssc_metric metric) {
	switch(metric) {
		case OVERFLOWS:
			return overflows;
			break;
		case BUFFER_ERRORS:
			return ssc_buffer_errors;
			break;
		case FREE_BUFFERS:
			return ssc_count_free();
			break;
		case LATE_FRAMES:
			return late_frames;
			break;
		case SSC_ERRORS:
			return ssc_get_metric(OVERFLOWS) + ssc_get_metric(BUFFER_ERRORS);
			break;
	}
	return 0;
}

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


static int __ramfunc __ssc_rx_load(int secondary);
static ssc_dma_rx_buffer_t* __ramfunc __ssc_rx_unload(int secondary);
/* 
 * Find and load an RX buffer into the DMA controller, using the current SSC mode
 */
static int __ramfunc __ssc_rx_load(int secondary)
{
	ssc_dma_rx_buffer_t *buffer;
	
	buffer = ssc_find_dma_buffer(FREE, PENDING);
	if (!buffer) {
		DEBUGP("no_rctx_for_refill! ");
		overflows++;
		return -1;
	}
	DEBUGR("filling SSC RX%u dma ctx: %u (len=%u) ", secondary,
		req_ctx_num(buffer), buffer->size);
	buffer->len_transfers = ssc_sizes[ssc_state.mode].transfers;
	buffer->reception_mode = &ssc_sizes[ssc_state.mode];
	
	if(ssc_state.buffer[secondary] != NULL) { 
		/* This condition is not expected to happen and would probably indicate a bug 
		 * of some sort. However, instead of leaking buffers we'll just pretend it to
		 * be free again. */
		 ssc_buffer_errors++;
		 if(ssc_state.buffer[secondary]->state == PENDING) {
		 	ssc_state.buffer[secondary]->state = FREE;
		 }
	}
	
	if (secondary) {
		AT91F_PDC_SetNextRx(rx_pdc, buffer->data,
				    ssc_sizes[ssc_state.mode].transfers);
		ssc_state.buffer[1] = buffer;
	} else {
		AT91F_PDC_SetRx(rx_pdc, buffer->data,
				ssc_sizes[ssc_state.mode].transfers);
		ssc_state.buffer[0] = buffer;
	}
	
	if(secondary) {int i=usb_print_set_default_flush(0);
		DumpStringToUSB("{1:");
		DumpUIntToUSB(rx_pdc->PDC_RNCR);
		DumpStringToUSB(" ");
		DumpUIntToUSB(rx_pdc->PDC_RNPR);
		DumpStringToUSB("} ");
		usb_print_set_default_flush(i);}
	else {int i=usb_print_set_default_flush(0);
		DumpStringToUSB("{0:");
		DumpUIntToUSB(rx_pdc->PDC_RCR);
		DumpStringToUSB(" ");
		DumpUIntToUSB(rx_pdc->PDC_RPR);
		DumpStringToUSB("} ");
		usb_print_set_default_flush(i);}

	return 0;
}

/*
 * Take the RX buffer(s) from the DMA controller, e.g. to abort a currently executing receive process and
 * either reclaim the buffer(s) (if no transfer have been done so far) or mark them as used, updating 
 * the length fields to match the number of transfers that have actually executed.
 * Warning: When this function executes, the mapping in ssc_state is expected to match the mapping in
 * the PDC (e.g. ssc_state[0] is the RX Buffer and ssc_state[1] is the Next RX Buffer). Do not use this
 * function while the PDC transfer is enabled. Especially do not run it from the SSC RX IRQ.
 */
static int __ramfunc __ssc_tx_unload_all(ssc_dma_rx_buffer_t** primary, ssc_dma_rx_buffer_t** secondary)
{
	if(primary != NULL)   *primary   = __ssc_rx_unload(0); else __ssc_rx_unload(0);
	if(secondary != NULL) *secondary = __ssc_rx_unload(1); else __ssc_rx_unload(1);
	return 1;
}
static ssc_dma_rx_buffer_t* __ramfunc __ssc_rx_unload(int secondary)
{
	ssc_dma_rx_buffer_t *buffer = ssc_state.buffer[secondary];
	if(buffer == NULL) return NULL;
	
	if(secondary) {int i=usb_print_set_default_flush(0);
		DumpStringToUSB("(1:");
		DumpUIntToUSB(rx_pdc->PDC_RNCR);
		DumpStringToUSB(" ");
		DumpUIntToUSB(rx_pdc->PDC_RNPR);
		DumpStringToUSB(") ");
		usb_print_set_default_flush(i);}
	else {int i=usb_print_set_default_flush(0);
		DumpStringToUSB("(0:");
		DumpUIntToUSB(rx_pdc->PDC_RCR);
		DumpStringToUSB(" ");
		DumpUIntToUSB(rx_pdc->PDC_RPR);
		DumpStringToUSB(") ");
		usb_print_set_default_flush(i);}
	
	u_int16_t remaining_transfers = (secondary ? rx_pdc->PDC_RNCR : rx_pdc->PDC_RCR);
	u_int8_t* next_transfer_location = (u_int8_t*)(secondary ? rx_pdc->PDC_RNPR : rx_pdc->PDC_RPR);
	u_int16_t elapsed_transfers = buffer->reception_mode->transfers - remaining_transfers;
	
	/* BUG BUG BUG For some reason the RNCR is zero, even though there have been no transfers in the secondary
	 * buffer. For now just assume that secondary==1 && remaining_transfers==0 is a bug condition and actually
	 * means elapsed_transfers == 0. Of course this will fail should they second buffer really be completely full. */
	if(secondary && remaining_transfers==0) {
		remaining_transfers = buffer->reception_mode->transfers;
		elapsed_transfers = 0;
	}
	
	u_int32_t elapsed_size = buffer->reception_mode->transfersize_pdc/8  * elapsed_transfers;
	
	/* Consistency check */
	if( next_transfer_location - elapsed_size != buffer->data ) {
		int i=usb_print_set_default_flush(0);
		DumpStringToUSB("!!!"); DumpUIntToUSB(secondary); DumpStringToUSB(" "); 
		DumpUIntToUSB((int)next_transfer_location); DumpStringToUSB(" ");
		DumpUIntToUSB(elapsed_size); DumpStringToUSB(" "); DumpUIntToUSB((int)buffer->data); DumpStringToUSB(" ");
		usb_print_set_default_flush(i);
		ssc_buffer_errors++;
		if(buffer->state == PENDING) buffer->state = FREE;
		ssc_state.buffer[secondary] = NULL;
		return NULL;
	}
	
	if(secondary) {
		AT91F_PDC_SetNextRx(rx_pdc, 0, 0);
	} else {
		AT91F_PDC_SetRx(rx_pdc, 0, 0);
	}
	if(buffer->state == PENDING || buffer->state==FULL) {
		buffer->len_transfers = elapsed_transfers;
		{int i=usb_print_set_default_flush(0);
			DumpStringToUSB("<");
			DumpUIntToUSB((unsigned int)buffer);
			DumpStringToUSB(": ");
			DumpUIntToUSB(elapsed_transfers);
			DumpStringToUSB("> ");
		usb_print_set_default_flush(i);}
		if(elapsed_transfers > 0) {
			buffer->state = FULL;
		} else {
			buffer->state = FREE;
		}
	}
	ssc_state.buffer[secondary] = NULL;
	 
	return buffer;
}

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
	u_int32_t clock_gating=0;
	u_int8_t stop = 0;

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
		clock_gating = (0x2 << 6);
		break;
	case SSC_MODE_14443A_STANDARD:
		start_cond = AT91C_SSC_START_0;
		sync_len = ISO14443A_SOF_LEN;
		ssc->SSC_RC0R = ISO14443A_SOF_SAMPLE;
		data_len = 32;
		num_data = 16;	/* FIXME */
		clock_gating = (0x2 << 6);
		break;
	case SSC_MODE_14443A:
		start_cond = AT91C_SSC_START_0;
		sync_len = ISO14443A_EOF_LEN;
		ssc->SSC_RC0R = ISO14443A_SOF_SAMPLE << (ISO14443A_EOF_LEN-ISO14443A_SOF_LEN);
		ssc->SSC_RC1R = ISO14443A_EOF_SAMPLE;
		data_len = ISO14443A_SAMPLE_LEN;
		num_data = 16; /* Start with 16, then switch to continuous in the IRQ handler */
		stop = 1;      /* It's impossible to use "stop on compare 1" for the stop condition here */
		clock_gating = (0x0 << 6);
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
		clock_gating = (0x2 << 6);
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
			clock_gating | AT91C_SSC_CKI | start_cond | (stop << 12);

	/* Enable Rx DMA */
	AT91F_PDC_EnableRx(rx_pdc);

	/* Enable RX interrupts */
/*
	AT91F_SSC_EnableIt(ssc, AT91C_SSC_OVRUN | AT91C_SSC_CP0 |
			   AT91C_SSC_ENDRX | AT91C_SSC_RXBUFF);*/
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

/* This is the time that the TF FIQ should spin until before SWTRG'ing the tc_cdiv.
 * See fdt_timing.dia. Note: This means transmission is broken without USE_SSC_TX_TF_WORKAROUND */
volatile u_int32_t ssc_tx_fiq_fdt_cdiv = 0;
/* This is the time that the TF FIQ should spin until before starting the SSC 
 * There must be enough time between these two! */
volatile u_int32_t ssc_tx_fiq_fdt_ssc = 0;
#ifndef USE_SSC_TX_TF_WORKAROUND
#error Transmission is broken without USE_SSC_TX_TF_WORKAROUND, see comments in code
#endif

void ssc_tf_irq(u_int32_t pio);
void ssc_tx_start(ssc_dma_tx_buffer_t *buf)
{
	u_int8_t data_len, num_data, sync_len;
	u_int32_t start_cond;

	/* disable Tx */
	AT91F_PDC_DisableTx(tx_pdc);
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
	ssc->SSC_TCMR = 0x01 | AT91C_SSC_CKO_NONE | AT91C_SSC_CKI | start_cond;
	
	AT91F_PDC_SetTx(tx_pdc, buf->data, num_data);
	AT91F_PDC_SetNextTx(tx_pdc, 0, 0);
	buf->state = PENDING;

	AT91F_SSC_EnableIt(ssc, AT91C_SSC_ENDTX);
	/* Enable DMA */
	AT91F_PDC_EnableTx(tx_pdc);
	AT91F_PDC_SetTx(tx_pdc, buf->data, num_data);
	/* Start Transmission */
#ifndef USE_SSC_TX_TF_WORKAROUND
	AT91F_SSC_EnableTx(AT91C_BASE_SSC);
#else
	ssc_tx_pending = 1;
	pio_irq_enable(OPENPICC_SSC_TF);
	if(AT91F_PIO_IsInputSet(AT91C_BASE_PIOA, OPENPICC_SSC_TF)) {
		/* TF was probably already high when we enabled the PIO change interrupt for it. */
		ssc_tf_irq(OPENPICC_SSC_TF);
		vLedBlinkRed();
		late_frames++;
		usb_print_string_f("Late response\n\r", 0);
	}
#endif
}

#ifdef USE_SSC_TX_TF_WORKAROUND
void ssc_tf_irq(u_int32_t pio) {
	(void)pio;
	if(!AT91F_PIO_IsInputSet(AT91C_BASE_PIOA, OPENPICC_SSC_TF)) return;
	pio_irq_disable(OPENPICC_SSC_TF);
	if(ssc_tx_pending) { /* Transmit has not yet been started by the FIQ */
		AT91F_SSC_EnableTx(AT91C_BASE_SSC);
		ssc_tx_pending = 0;
	}
}
#endif


static ssc_irq_ext_t irq_extension = NULL;
ssc_irq_ext_t ssc_set_irq_extension(ssc_irq_ext_t ext_handler) {
	ssc_irq_ext_t old = irq_extension;
	irq_extension = ext_handler;
	return old;
}

void __ramfunc ssc_rx_stop_frame_ended(void)
{
}

static void __ramfunc ssc_irq(void) __attribute__ ((naked));
static void __ramfunc ssc_irq(void)
{
	portENTER_SWITCHING_ISR();
	vLedSetRed(1);
	portBASE_TYPE task_woken = pdFALSE;

	u_int32_t ssc_sr = ssc->SSC_SR;
	u_int32_t orig_ssc_sr = ssc_sr;
	int i, emptyframe = 0;
	u_int32_t *tmp;
	ssc_dma_rx_buffer_t *inbuf=NULL;
	DEBUGP("ssc_sr=0x%08x, mode=%u: ", ssc_sr, ssc_state.mode);
	
	if ((ssc_sr & AT91C_SSC_CP0) && (ssc_state.mode == SSC_MODE_14443A_SHORT || ssc_state.mode == SSC_MODE_14443A)) {
		if(ssc_state.mode == SSC_MODE_14443A && ISO14443A_SOF_LEN != ISO14443A_EOF_LEN) {
			/* Need to reprogram FSLEN */
			//ssc->SSC_RFMR = (ssc->SSC_RFMR & ~(0xf << 16)) | ( ((ISO14443A_EOF_LEN-1)&0xf) << 16 );
			//ssc->SSC_RCMR = (ssc->SSC_RCMR & ~(0xf << 8)) | AT91C_SSC_START_CONTINOUS;
		}
		/* Short frame, busy loop till the frame is received completely to
		 * prevent a second irq entrance delay when the actual frame end 
		 * irq is raised. (The scheduler masks interrupts for about 56us,
		 * which is too much for anticollision.) */
		 int i = 0;
		 vLedBlinkRed();
		 while( ! ((ssc_sr=ssc->SSC_SR) & (AT91C_SSC_ENDRX | AT91C_SSC_CP1)) ) {
		 	i++;
		 	if(i > 9600) break; /* Break out, clearly this is not a short frame or a reception error happened */
		 }
		 ssc_sr |= orig_ssc_sr;
		 vLedSetRed(1);
	}
	
	if(ssc_sr & AT91C_SSC_CP1) usb_print_string_f("CP1 ", 0);
	
	if (ssc_sr & (AT91C_SSC_ENDRX | AT91C_SSC_CP1)) {
#if 0
// Bitrotten
		/* Ignore empty frames */
		if (ssc_state.mode == SSC_MODE_CONTINUOUS) {
			/* This code section is probably bitrotten by now. */
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
#else
		(void)i;
		(void)tmp;
#endif
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
		vLedSetGreen(1);

		inbuf = ssc_state.buffer[0];
		if(ssc_sr & AT91C_SSC_ENDRX) {
			/* second buffer gets propagated to primary */
			ssc_state.buffer[0] = ssc_state.buffer[1];
			ssc_state.buffer[1] = NULL;
		}
		if(ssc_state.mode == SSC_MODE_EDGE_ONE_SHOT || ssc_state.mode == SSC_MODE_14443A_SHORT
			|| ssc_state.mode == SSC_MODE_14443A_STANDARD || (ssc_state.mode == SSC_MODE_14443A && ssc_sr & AT91C_SSC_CP1)) {
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
				if(ssc_sr & AT91C_SSC_RXENA) if (__ssc_rx_load(0) == -1)
					AT91F_SSC_DisableIt(ssc, AT91C_SSC_ENDRX |
							    AT91C_SSC_RXBUFF |
							    AT91C_SSC_OVRUN);
			}
	
			if(ssc_sr & AT91C_SSC_RXENA) if (__ssc_rx_load(1) == -1)
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
		DEBUGP("TXSYN ");
	
	if(ssc_sr & AT91C_SSC_ENDTX) {
		//usb_print_string_f("ENDTX ", 0);
		if(ssc_tx_buffer.state == PENDING) {
			ssc_tx_buffer.state = FREE;
			AT91F_SSC_DisableIt(ssc, SSC_TX_IRQ_MASK);
		}
	}

	if(ssc_sr & AT91C_SSC_TXBUFE)
		DEBUGP("TXBUFE ");
	
	if(irq_extension != NULL)
		irq_extension(ssc_sr, ssc_state.mode, inbuf?inbuf->data:NULL);


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
	AT91F_SSC_EnableIt(ssc, AT91C_SSC_ENDRX | AT91C_SSC_CP0 |
			   AT91C_SSC_RXBUFF | AT91C_SSC_OVRUN);
}

void ssc_rx_start(void)
{
	//DEBUGPCRF("starting SSC RX\n");
	
	__ssc_rx_load(0);
	if(ssc_state.mode != SSC_MODE_14443A_SHORT) __ssc_rx_load(1);
	
	/* Enable Reception */
	AT91F_SSC_EnableIt(ssc, AT91C_SSC_ENDRX | AT91C_SSC_CP0 | AT91C_SSC_CP1 |
			   AT91C_SSC_RXBUFF | AT91C_SSC_OVRUN);
	AT91F_PDC_EnableRx(rx_pdc);
	AT91F_SSC_EnableRx(AT91C_BASE_SSC);
	
	/* Clear the flipflop */
	tc_cdiv_sync_reset();
}

void ssc_rx_stop(void)
{
	/* Disable reception */
	AT91F_SSC_DisableRx(AT91C_BASE_SSC);
	AT91F_PDC_DisableRx(rx_pdc);
	__ssc_tx_unload_all(NULL, NULL);
	AT91F_SSC_DisableIt(ssc, SSC_RX_IRQ_MASK);
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
			      AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL, (THandler)&ssc_irq);

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
	/* Will be set to a real value some time later */
	tc_fdt_set(0xff00);

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
