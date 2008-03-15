/* AT91SAM7 SSC controller routines for OpenPICC
 * (C) 2006 by Harald Welte <hwelte@hmw-consulting.de>
 * (C) 2007-2008 Henryk Plötz <henryk@ploetzli.ch>
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

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>
#include <openpicc.h>

#include <string.h>
#include <errno.h>

#include "ssc.h"
#include "iso14443.h"

#include "clock_switch.h"
#include "tc_cdiv_sync.h"
#include "tc_fdt.h"
#include "led.h"

#include "usb_print.h"
#include "cmd.h"

#define PRINT_DEBUG 0
#define DEBUG_DATA_GATING 0

struct _ssc_handle {
	enum ssc_mode mode;
	const struct openpicc_hardware *openpicc;
	ssc_dma_rx_buffer_t* rx_buffer[2];
	ssc_dma_tx_buffer_t* tx_buffer;
	ssc_callback_t callback;
	xQueueHandle rx_queue;
	AT91PS_PDC pdc;
	AT91PS_SSC ssc;
	u_int8_t rx_enabled, tx_enabled;
	u_int8_t rx_running, tx_running, tx_need_switching;
};

static ssc_handle_t _ssc;
static const ssc_mode_def ssc_modes[] = {
	/* Undefined, no size set */
	[SSC_MODE_NONE]		   = {SSC_MODE_NONE, 0, 0, 0},
	/* 14443A Frame */
	[SSC_MODE_14443A]      = {SSC_MODE_14443A, 
		ISO14443_BITS_PER_SSC_TRANSFER * ISO14443A_SAMPLE_LEN,               // transfersize_ssc 
		ISO14443_BITS_PER_SSC_TRANSFER * ISO14443A_SAMPLE_LEN <= 8 ? 8 : 16, // transfersize_pdc 
		DIV_ROUND_UP(ISO14443A_MAX_RX_FRAME_SIZE_IN_BITS, ISO14443_BITS_PER_SSC_TRANSFER) },
};
static struct {
	ssc_metric metric;
	char *name;
	int value;
} ssc_metrics[] = {
		[METRIC_RX_OVERFLOWS]      = {METRIC_RX_OVERFLOWS, "Rx overflows", 0},
		[METRIC_FREE_RX_BUFFERS]   = {METRIC_FREE_RX_BUFFERS, "Free Rx buffers", 0},
		[METRIC_MANAGEMENT_ERRORS] = {METRIC_MANAGEMENT_ERRORS, "Internal buffer management error", 0},
		[METRIC_MANAGEMENT_ERRORS_1] = {METRIC_MANAGEMENT_ERRORS_1, "Internal buffer management error type 1", 0},
		[METRIC_MANAGEMENT_ERRORS_2] = {METRIC_MANAGEMENT_ERRORS_2, "Internal buffer management error type 2", 0},
		[METRIC_MANAGEMENT_ERRORS_3] = {METRIC_MANAGEMENT_ERRORS_3, "Internal buffer management error type 3", 0},
		[METRIC_LATE_TX_FRAMES]    = {METRIC_LATE_TX_FRAMES, "Late Tx frames", 0},
		[METRIC_RX_FRAMES]         = {METRIC_RX_FRAMES, "Rx frames", 0},
		[METRIC_TX_FRAMES]         = {METRIC_TX_FRAMES, "Tx frames", 0},
		[METRIC_TX_ABORTED_FRAMES] = {METRIC_TX_ABORTED_FRAMES, "Aborted Tx frames", 0},
};

static ssc_dma_rx_buffer_t _rx_buffers[SSC_DMA_BUFFER_COUNT];
ssc_dma_tx_buffer_t        _tx_buffer;

/******* PRIVATE "meat" code *************************************************/

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

static __ramfunc ssc_dma_rx_buffer_t* _unload_rx(ssc_handle_t *sh);
static __ramfunc int _reload_rx(ssc_handle_t *sh);


static int __ramfunc _ssc_rx_irq(u_int32_t orig_sr, int start_asserted, portBASE_TYPE task_woken)
{
	int end_asserted = 0;
	ssc_handle_t *sh = &_ssc;
	u_int32_t orig_rcmr = sh->ssc->SSC_RCMR;
	
#if PRINT_DEBUG
	int old = usb_print_set_default_flush(0);  // DEBUG OUTPUT
	DumpStringToUSB("orig:");  // DEBUG OUTPUT
	DumpUIntToUSB(orig_sr);  // DEBUG OUTPUT
	DumpStringToUSB("\n\r");  // DEBUG OUTPUT
#endif
	u_int32_t sr = orig_sr | _ssc.ssc->SSC_SR;
	
	if( (sr & AT91C_SSC_RXSYN) || start_asserted) {
		sh->ssc->SSC_RCMR = (orig_rcmr & (~AT91C_SSC_START)) | (AT91C_SSC_START_CONTINOUS);
		/* Receiving has started */
		if(sh->callback != NULL) {
			sh->callback(SSC_CALLBACK_RX_FRAME_BEGIN, &end_asserted);
			if(end_asserted)
				sr = orig_sr | _ssc.ssc->SSC_SR;
		}
	}
	
#if PRINT_DEBUG	
	DumpStringToUSB("sr:");  // DEBUG OUTPUT
	DumpUIntToUSB(sr);  // DEBUG OUTPUT
	DumpStringToUSB("\n\r");  // DEBUG OUTPUT
	usb_print_set_default_flush(old);  // DEBUG OUTPUT
#endif
	
	if(((sr & (AT91C_SSC_CP1 | AT91C_SSC_ENDRX)) || end_asserted) && (sh->rx_buffer[0] != NULL)) {
		/* Receiving has ended */
		AT91F_PDC_DisableRx(sh->pdc);
		AT91F_SSC_DisableRx(sh->ssc);
		sh->ssc->SSC_RCMR = ((sh->ssc->SSC_RCMR) & (~AT91C_SSC_START)) | (AT91C_SSC_START_0);

		ssc_dma_rx_buffer_t *buffer = _unload_rx(sh);
		if(buffer != NULL) {
			if(sh->callback != NULL)
				sh->callback(SSC_CALLBACK_RX_FRAME_ENDED, buffer);

			if(buffer->state != SSC_FREE) {
				task_woken = xQueueSendFromISR(sh->rx_queue, &buffer, task_woken);
			}
		}
		
		if(_reload_rx(sh)) {
			/* Clear the receive holding register. Is this necessary? */
			int dummy = sh->ssc->SSC_RHR; (void)dummy;
			AT91F_PDC_EnableRx(sh->pdc);
			if(!sh->tx_running) {
				/* Only start the receiver if no Tx has been started. Warning: Need
				 * to make sure that the receiver is restarted again when the Tx is over.
				 * Note that this is only necessary when not using Compare 0 to start
				 * reception. When using Compare 0 the data gating mechanism will keep
				 * CP0 from happening and thus keep the receiver stopped.
				 *
				 * Note also that we're not resetting rx_running to 0, because conceptionally
				 * the Rx is still running,. So rx_running=1 and tx_running=1 means SSC_RX
				 * stopped and SSC_RX should be started as soon as SSC_TX stops.
				 */
				AT91F_SSC_EnableRx(sh->ssc);
			}
		} else {
			sh->ssc->SSC_IDR = SSC_RX_IRQ_MASK;
			sh->rx_running = 0;
			sh->callback(SSC_CALLBACK_RX_STOPPED, sh);
		}
		
	}

	sh->ssc->SSC_RCMR = orig_rcmr;
	return task_woken;
}

/* Exported callback for the case when the frame start has been detected externally */
void ssc_frame_started(void)
{
	_ssc_rx_irq(_ssc.ssc->SSC_SR, 1, pdFALSE);
}

static void __ramfunc _ssc_tx_end(ssc_handle_t *sh, int is_an_abort)
{
	AT91F_PDC_DisableTx(sh->pdc);
	AT91F_SSC_DisableTx(sh->ssc);
	sh->ssc->SSC_IDR = SSC_TX_IRQ_MASK;
	
	AT91F_PIO_CfgPeriph(AT91C_BASE_PIOA, AT91C_PA15_TF, 0);
	usb_print_string_f(">",0);
	
	AT91F_PDC_SetTx(sh->pdc, 0, 0);
	AT91F_PDC_SetNextTx(sh->pdc, 0, 0);

	if(sh->tx_buffer) {
		sh->tx_buffer->state = SSC_FREE;
		sh->tx_running = 0;
	}
	
	if(sh->rx_running) {
		/* Receiver has been suspended by the pending transmission. Restart it. */
		AT91F_SSC_EnableRx(sh->ssc);
	}
	
	if(sh->callback) {
		if(is_an_abort)
			sh->callback(SSC_CALLBACK_TX_FRAME_ABORTED, sh->tx_buffer);
		else
			sh->callback(SSC_CALLBACK_TX_FRAME_ENDED, sh->tx_buffer);
	}
		
	sh->tx_buffer = NULL;
}

static int __ramfunc _ssc_tx_irq(u_int32_t sr, portBASE_TYPE task_woken)
{
	ssc_handle_t *sh = &_ssc;
	
	if( sr & AT91C_SSC_TXSYN ) {
		/* Tx starting, hardwire TF pin to high */
		AT91F_PIO_SetOutput(AT91C_BASE_PIOA, AT91C_PA15_TF);
		AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, AT91C_PA15_TF);
		usb_print_string_f("<",0);
		
		/* Also set SSC mode to continous 
		 * FIXME BUG: This will somehow add two samples on the SSC 
		 * The abomination below is designed to find a pause in the outgoing modulation data
		 * (WARNING: Will only work with manchester, not with PSK) so that the SSC start type
		 * switch will happen during a time of no subcarrier modulation (in order to not
		 * upset the subcarrier). Still need to find a way to 'absorb' the extra two samples. */
		if(sh->tx_need_switching) {
			vLedSetGreen(0);
			int j = 0;
off_pre:
			if(j++ > 100) goto out;
			if(!AT91F_PIO_IsInputSet(AT91C_BASE_PIOA, OPENPICC_MOD_PWM)) {
				goto off_pre;
			}
on:
			if(j++ > 100) goto out;
			if(AT91F_PIO_IsInputSet(AT91C_BASE_PIOA, OPENPICC_MOD_PWM)) goto on;
			int i=0;
off:
			if(j++ > 100) goto out;
			if(!AT91F_PIO_IsInputSet(AT91C_BASE_PIOA, OPENPICC_MOD_PWM)) {
				if(i++ > 2) goto out; else goto off;
			} else goto on;
out:
			sh->ssc->SSC_TCMR = (sh->ssc->SSC_TCMR & ~AT91C_SSC_START) | AT91C_SSC_START_CONTINOUS;
			vLedSetGreen(1);
		}

		if(sh->callback) 
			sh->callback(SSC_CALLBACK_TX_FRAME_BEGIN, NULL);
	}
	
	if( sr & AT91C_SSC_TXEMPTY ) {
		/* Tx has ended */
		ssc_metrics[METRIC_TX_FRAMES].value++;
		_ssc_tx_end(sh, 0);
	}
	
	return task_woken;
}

static void __ramfunc ssc_irq(void) __attribute__ ((naked));
static void __ramfunc ssc_irq(void)
{
	portENTER_SWITCHING_ISR();
	vLedSetRed(1);
	portBASE_TYPE task_woken = pdFALSE;
	
	u_int32_t sr = _ssc.ssc->SSC_SR;
	if(sr & AT91C_SSC_RXSYN) {
		task_woken = _ssc_rx_irq(sr, 1, task_woken);
	} else if(sr & SSC_RX_IRQ_MASK) {
		task_woken = _ssc_rx_irq(sr, 0, task_woken);
	}
	
	if(sr & SSC_TX_IRQ_MASK) {
		task_woken = _ssc_tx_irq(sr, task_woken);
	}
	
	AT91F_AIC_ClearIt(AT91C_ID_SSC);
	AT91F_AIC_AcknowledgeIt();

	vLedSetRed(0);
	portEXIT_SWITCHING_ISR(task_woken);
}

static __ramfunc ssc_dma_rx_buffer_t *_get_buffer(ssc_dma_buffer_state_t old, ssc_dma_buffer_state_t new)
{
	ssc_dma_rx_buffer_t *buffer = NULL;
	int i;
	for(i=0; i < SSC_DMA_BUFFER_COUNT; i++) {
		if(_rx_buffers[i].state == old) {
			buffer = &_rx_buffers[i];
			buffer->state = new;
			break;
		}
	}
	return buffer;
}

/* Doesn't check sh, must be called with interrupts disabled */
static __ramfunc int _reload_rx(ssc_handle_t *sh)
{
	int result = 0;
	if(sh->rx_buffer[0] != NULL) {
		ssc_metrics[METRIC_MANAGEMENT_ERRORS_1].value++;
		result = 1;
		goto out;
	}
	
	ssc_dma_rx_buffer_t *buffer = _get_buffer(SSC_FREE, SSC_PENDING);
	
	if(buffer == NULL) {
		ssc_metrics[METRIC_RX_OVERFLOWS].value++;
		goto out;
	}

	buffer->reception_mode = &ssc_modes[sh->mode];
	buffer->len_transfers = ssc_modes[sh->mode].transfers;
	
	AT91F_PDC_SetRx(sh->pdc, buffer->data, buffer->len_transfers);
	sh->rx_buffer[0] = buffer;
	
	result = 1;
out:
	return result;
}

// Doesn't check sh, call with interrupts disabled, SSC/PDC stopped
static __ramfunc ssc_dma_rx_buffer_t* _unload_rx(ssc_handle_t *sh)
{
	if(sh->rx_buffer[0] == NULL) {
		ssc_metrics[METRIC_MANAGEMENT_ERRORS_2].value++;
		return NULL;
	}
	
	ssc_dma_rx_buffer_t *buffer = sh->rx_buffer[0];
	u_int32_t rpr = sh->pdc->PDC_RPR, 
		rcr = sh->pdc->PDC_RCR;
	AT91F_PDC_SetRx(sh->pdc, 0, 0);
	sh->rx_buffer[0] = NULL;
	buffer->state = SSC_FULL;
	
	if(rcr == 0) {
		buffer->flags.overflow = 1;
	} else {
		buffer->flags.overflow = 0;
	}
	
	if(rcr > 0) {
		/* Append a zero to make sure the buffer decoder finds the stop bit */
		switch(buffer->reception_mode->transfersize_pdc) {
		case 8:
			//*((u_int8_t*)rpr++) = sh->ssc->SSC_RSHR;
			*((u_int8_t*)rpr++) = 0;
			--rcr;
			break;
		case 16:
			*((u_int16_t*)rpr++) = 0;
			--rcr;
			break;
		case 32:
			*((u_int32_t*)rpr++) = 0;
			--rcr;
			break;
		}
	}
	
	if((buffer->len_transfers - rcr) != (rpr - (unsigned int)buffer->data)*(buffer->reception_mode->transfersize_pdc/8)) {
		ssc_metrics[METRIC_MANAGEMENT_ERRORS_3].value++;
		buffer->state = SSC_FREE;
		return NULL;
	}
	
	buffer->len_transfers = buffer->len_transfers - rcr;
	
	return buffer;
}

void ssc_set_gate(int data_enabled) {
	if(OPENPICC->features.data_gating) {
		if(data_enabled) {
			AT91F_PIO_SetOutput(AT91C_BASE_PIOA, OPENPICC->DATA_GATE);
			if(DEBUG_DATA_GATING) usb_print_string_f("(", 0);
		} else {
			AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, OPENPICC->DATA_GATE);
			if(DEBUG_DATA_GATING) usb_print_string_f(")", 0);
		}
	}
}

static void _ssc_start_rx(ssc_handle_t *sh)
{
	taskENTER_CRITICAL();
	if(sh != &_ssc) goto out;
	if(sh->rx_running) goto out;
	if(!sh->rx_enabled) goto out;

	// Load buffer
	if(!_reload_rx(sh))
		goto out;
	
	sh->ssc->SSC_IER = AT91C_SSC_RXSYN | \
		 AT91C_SSC_CP1 | AT91C_SSC_ENDRX;
	sh->rx_running = 1;
	if(sh->callback != NULL)
		sh->callback(SSC_CALLBACK_RX_STARTING, sh);

	// Actually enable reception
	int dummy = sh->ssc->SSC_RHR; (void)dummy;
	AT91F_PDC_EnableRx(sh->pdc);
	AT91F_SSC_EnableRx(sh->ssc);

out:
	taskEXIT_CRITICAL();
	usb_print_string_f(sh->rx_running ? "SSC now running\n\r":"SSC not running\n\r", 0);
}

static void _ssc_stop_rx(ssc_handle_t *sh)
{
	taskENTER_CRITICAL();
	sh->ssc->SSC_IDR = SSC_RX_IRQ_MASK;
	sh->rx_running = 0;
	if(sh->callback != NULL)
		sh->callback(SSC_CALLBACK_RX_STOPPED, sh);
	taskEXIT_CRITICAL();
}

/******* PRIVATE Initialization Code *****************************************/
static void _ssc_rx_mode_set(ssc_handle_t *sh, enum ssc_mode ssc_mode)
{
	taskENTER_CRITICAL();
	int was_running = sh->rx_running;
	if(was_running) _ssc_stop_rx(sh);

	u_int8_t data_len=0, num_data=0, sync_len=0;
	u_int32_t start_cond=0;
	u_int32_t clock_gating=0;
	u_int8_t stop = 0, invert=0;

	switch(ssc_mode) {
		case SSC_MODE_14443A:
			/* Start on Compare 0. The funky calculations down there are designed to allow a different
			 * (longer) compare length for Compare 1 than for Compare 0. Both lengths are set by the
			 * same register. */
			start_cond = AT91C_SSC_START_0;
			sync_len = ISO14443A_EOF_LEN;
			sh->ssc->SSC_RC0R = ISO14443A_SOF_SAMPLE << (ISO14443A_EOF_LEN-ISO14443A_SOF_LEN);
			sh->ssc->SSC_RC1R = ISO14443A_EOF_SAMPLE;
			
			data_len = ssc_modes[SSC_MODE_14443A].transfersize_ssc;
			
			/* We are using stop on Compare 1. The docs are ambiguous but my interpretation is that
			 * this means that num_data is basically ignored and reception is continuous until stop
			 * event. Set num_data to the maximum anyways. */
			num_data = 16;
			stop = 1;
			
			stop = 0;
			start_cond = AT91C_SSC_START_CONTINOUS;
			sync_len = 0;
			
			/* We can't use receive clock gating with RF because RF will only go up with the first
			 * edge, this doesn't cooperate with the longer sync len above. 
			 * FIXME: What's the interaction between clock BURST on v0.4p1, RF and Compare 0? 
			 * In theory this shouldn't work even without SSC clock_gating because BURST gates the
			 * clock before the SSC and so it shouldn't get sync_len samples before Compare 0. 
			 * I believe there's a bug waiting to happen somewhere here. */
			clock_gating = (0x0 << 6);
			//invert = AT91C_SSC_CKI;
			break;
		case SSC_MODE_NONE:
			goto out;
			break;
	}
	
	/* Receive frame mode register */
	sh->ssc->SSC_RFMR = ((data_len-1) & 0x1f) |
			(((num_data-1) & 0x0f) << 8) | 
			(((sync_len-1) & 0x0f) << 16);
	
	/* Receive clock mode register, Clock selection: RK, Clock output: None */
	sh->ssc->SSC_RCMR = AT91C_SSC_CKS_RK | AT91C_SSC_CKO_NONE | 
			clock_gating | invert | start_cond | (stop << 12);

out:
	sh->mode = ssc_mode;
	if(was_running) _ssc_start_rx(sh);
	taskEXIT_CRITICAL();
}

#if SSC_DMA_BUFFER_COUNT > 0
static inline int _init_ssc_rx(ssc_handle_t *sh)
{
	tc_cdiv_sync_init();
	tc_cdiv_sync_enable();
	
	if(sh->rx_queue == NULL) {
		sh->rx_queue = xQueueCreate(10, sizeof(sh->rx_buffer[0]));
		if(sh->rx_queue == NULL)
			goto out_fail_queue;
	}
	
	AT91F_PIO_CfgPeriph(AT91C_BASE_PIOA, 
			    OPENPICC_SSC_DATA | OPENPICC_SSC_CLOCK |
			    OPENPICC_PIO_FRAME,
			    0);
	
	/* FIXME: This is handled by tc_cdiv_sync and shouldn't be necessary */
	AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, OPENPICC_PIO_SSC_DATA_CONTROL);
	AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, OPENPICC_PIO_SSC_DATA_CONTROL);
	
	if(OPENPICC->features.clock_switching) {
		AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, OPENPICC->CLOCK_SWITCH);
		clock_switch(CLOCK_SELECT_PLL);
	}

	/* Disable all interrupts */
	sh->ssc->SSC_IDR = SSC_RX_IRQ_MASK;

	/* don't divide clock inside SSC, we do that in tc_cdiv */
	sh->ssc->SSC_CMR = 0;

	unsigned int i;
	for(i=0; i<sizeof(_rx_buffers)/sizeof(_rx_buffers[0]); i++)
		memset(&_rx_buffers[i], 0, sizeof(_rx_buffers[i]));
	
	sh->rx_buffer[0] = sh->rx_buffer[1] = NULL;
	
	/* Will be set to a real value some time later 
	 * FIXME Layering? */
	tc_fdt_set(0xff00);
	
	return 1;
	
out_fail_queue:
	return 0;
}
#endif

static inline int _init_ssc_tx(ssc_handle_t *sh)
{
	/* IMPORTANT: Disable PA23 (PWM0) output, since it is connected to 
	 * PA17 !! */
	AT91F_PIO_CfgInput(AT91C_BASE_PIOA, OPENPICC_MOD_PWM);
	AT91F_PIO_CfgPeriph(AT91C_BASE_PIOA, OPENPICC_MOD_SSC | 
			    OPENPICC_SSC_CLOCK | OPENPICC_SSC_TF, 0);
	
	if(OPENPICC->features.clock_switching) {
		clock_switch_init();
		/* Users: remember to clock_switch(CLOCK_SELECT_CARRIER) as
		 * early as possible, e.g. right after receive end */
	}

	/* Disable all interrupts */
	sh->ssc->SSC_IDR = SSC_TX_IRQ_MASK;

	/* don't divide clock inside SSC, we do that in tc_cdiv */
	sh->ssc->SSC_CMR = 0;

	return 1;
}

static int _ssc_register_callback(ssc_handle_t *sh, ssc_callback_t _callback)
{
	if(!sh) return -EINVAL;
	if(sh->callback != NULL) return -EBUSY;
	sh->callback = _callback;
	if(sh->callback != NULL) 
		sh->callback(SSC_CALLBACK_SETUP, sh);
	return 0;
}

static int _ssc_unregister_callback(ssc_handle_t *sh, ssc_callback_t _callback)
{
	if(!sh) return -EINVAL;
	if(_callback == NULL || sh->callback == _callback) {
		if(sh->callback != NULL) 
			sh->callback(SSC_CALLBACK_TEARDOWN, sh);
		sh->callback = NULL;
	}
	return 0;
}

/******* PUBLIC API **********************************************************/
ssc_handle_t* ssc_open(u_int8_t init_rx, u_int8_t init_tx, enum ssc_mode mode, ssc_callback_t callback)
{
	ssc_handle_t *sh = &_ssc;
	
	if(sh->rx_enabled || sh->tx_enabled || sh->rx_running) {
		if( ssc_close(sh) != 0) {
			return NULL;
		}
	}
	
	if(init_rx || init_tx) {
		sh->ssc = AT91C_BASE_SSC;
		sh->pdc = (AT91PS_PDC) &(sh->ssc->SSC_RPR);

		AT91F_SSC_CfgPMC();

		if(OPENPICC->features.data_gating) {
			AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, OPENPICC->DATA_GATE);
			ssc_set_gate(1);
		}
	}
	

	if(init_tx) {
		sh->tx_enabled = _init_ssc_tx(sh);
		if(!sh->tx_enabled) {
			ssc_close(sh);
			return NULL;
		}
	}
	
	if(init_rx) {
#if SSC_DMA_BUFFER_COUNT > 0
		sh->rx_enabled = _init_ssc_rx(sh);
		if(!sh->rx_enabled) {
			ssc_close(sh);
			return NULL;
		}
#else
		ssc_close(sh);
		return NULL;
#endif
	}
	
	if(sh->rx_enabled || sh->tx_enabled) {
		_ssc_rx_mode_set(sh, mode);
		AT91F_AIC_ConfigureIt(AT91C_ID_SSC,
				      OPENPICC_IRQ_PRIO_SSC,
				      AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL, (THandler)&ssc_irq);
				      //AT91C_AIC_SRCTYPE_INT_POSITIVE_EDGE, (THandler)&ssc_irq);
		AT91F_AIC_EnableIt(AT91C_ID_SSC);
	}
	
	if(callback != NULL)
		_ssc_register_callback(sh, callback);
	
	if(init_rx)
		_ssc_start_rx(sh);
	
	return sh;
}

int ssc_recv(ssc_handle_t* sh, ssc_dma_rx_buffer_t* *buffer,unsigned int timeout)
{
	if(sh == NULL) return -EINVAL;
	if(!sh->rx_enabled) return -EINVAL;
	
	taskENTER_CRITICAL();
	if(sh->rx_running) {
		if(PRINT_DEBUG) usb_print_string_f("Continuing SSC Rx\n\r",0); // DEBUG OUTPUT
	} else {
		if(PRINT_DEBUG) usb_print_string_f("ERR: SSC halted\n\r",0); // DEBUG OUTPUT
		/* Try starting the Reception */
		_ssc_start_rx(sh);
	}
	taskEXIT_CRITICAL();
	
	if(xQueueReceive(sh->rx_queue, buffer, timeout)){
		if(*buffer != NULL) return 0;
		else return -EINTR;
	}
	
	return -ETIMEDOUT;
}
extern u_int32_t fdt_offset;
/* Must be called with IRQs disabled. E.g. from IRQ context or within a critical section. */
int ssc_send(ssc_handle_t* sh, ssc_dma_tx_buffer_t *buffer)
{
	if(sh == NULL) return -EINVAL;
	if(!sh->tx_enabled) return -EINVAL;
	if(sh->tx_running) return -EBUSY;
	
	sh->tx_buffer = buffer;
	sh->tx_running = 1;
	
	/* disable Tx */
	sh->ssc->SSC_IDR = SSC_TX_IRQ_MASK;
	AT91F_PDC_DisableTx(sh->pdc);
	AT91F_SSC_DisableTx(sh->ssc);

	int start_cond = AT91C_SSC_START_HIGH_RF;
	
	int sync_len = 1;
	int data_len = 32;
	int num_data = buffer->len/(data_len/8); /* FIXME This is broken for more than 64 bytes, or is it? */
	int num_data_ssc = num_data > 16 ? 16 : num_data;
	sh->tx_need_switching = (num_data > 16);
	
	sh->ssc->SSC_TFMR = ((data_len-1) & 0x1f) |
			(((num_data_ssc-1) & 0x0f) << 8) | 
			(((sync_len-1) & 0x0f) << 16);
	sh->ssc->SSC_TCMR = 0x01 | AT91C_SSC_CKO_NONE | (AT91C_SSC_CKI&0) | start_cond;
	
	AT91F_PDC_SetTx(sh->pdc, buffer->data, num_data);
	AT91F_PDC_SetNextTx(sh->pdc, 0, 0);
	buffer->state = SSC_PENDING;

	sh->ssc->SSC_IER = AT91C_SSC_TXEMPTY | AT91C_SSC_TXSYN;
	/* Enable DMA */
	sh->ssc->SSC_THR = 0;
	AT91F_PDC_EnableTx(sh->pdc);
	
	/* Disable Receiver, see comments in _ssc_rx_irq */
	AT91F_SSC_DisableRx(sh->ssc);
	/* Start Transmission */
	AT91F_SSC_EnableTx(sh->ssc);
	vLedSetGreen(1);
	
	if(AT91F_PIO_IsInputSet(AT91C_BASE_PIOA, OPENPICC_SSC_TF)) {
		ssc_metrics[METRIC_LATE_TX_FRAMES].value++;
	}
	
	return 0;
}

int ssc_send_abort(ssc_handle_t* sh)
{
	if(!sh) return -EINVAL;
	if(!sh->tx_enabled) return -EINVAL;
	if(!sh->tx_running) return -EINVAL;
	
	ssc_metrics[METRIC_TX_ABORTED_FRAMES].value++;
	_ssc_tx_end(sh, 1);
	
	return 0;
}

int ssc_close(ssc_handle_t* sh)
{
	if(sh->rx_running)
		_ssc_stop_rx(sh);
	
	if(sh->rx_enabled) {
		// FIXME Implement
		sh->rx_enabled = 0;
	}
	if(sh->tx_enabled) {
		// FIXME Implement
		sh->tx_enabled = 0;
	}
	
	_ssc_unregister_callback(sh, NULL);
	return 0;
}

/* Hard reset the SSC to flush all buffers and whatnot. Call with IRQs disabled */ 
void ssc_hard_reset(ssc_handle_t *sh)
{
	if(sh == NULL) return;
	if(!sh->rx_enabled && !sh->tx_enabled) return;
	
	u_int32_t
		cmr = sh->ssc->SSC_CMR,
		rcmr = sh->ssc->SSC_RCMR,
		rfmr = sh->ssc->SSC_RFMR,
		tcmr = sh->ssc->SSC_TCMR,
		tfmr = sh->ssc->SSC_TFMR,
		rc0r = sh->ssc->SSC_RC0R,
		rc1r = sh->ssc->SSC_RC1R,
		sr = sh->ssc->SSC_SR,
		imr = sh->ssc->SSC_IMR;
	
	sh->ssc->SSC_CR = AT91C_SSC_SWRST;
	
	sh->ssc->SSC_CMR = cmr;
	sh->ssc->SSC_RCMR = rcmr;
	sh->ssc->SSC_RFMR = rfmr;
	sh->ssc->SSC_TCMR = tcmr;
	sh->ssc->SSC_TFMR = tfmr;
	sh->ssc->SSC_RC0R = rc0r;
	sh->ssc->SSC_RC1R = rc1r;
	
	sh->ssc->SSC_IDR = ~imr;
	sh->ssc->SSC_IER = imr;
	
	if(sr & AT91C_SSC_RXEN) 
		AT91F_SSC_EnableRx(sh->ssc);
	else
		AT91F_SSC_DisableRx(sh->ssc);
	
	if(sr & AT91C_SSC_TXEN) 
		AT91F_SSC_EnableTx(sh->ssc);
	else
		AT91F_SSC_DisableTx(sh->ssc);
}


int ssc_get_metric(ssc_metric metric, char **description, int *value)
{
	char *_name="Undefined";
	int _value=0;
	int valid=0;
	
	if(metric < sizeof(ssc_metrics)/sizeof(ssc_metrics[0])) {
		_name  = ssc_metrics[metric].name;
		_value = ssc_metrics[metric].value;
		valid = 1;
	}
		
	switch(metric) {
		case METRIC_FREE_RX_BUFFERS:
			_value = 0;
			int i;
			for(i=0; i < SSC_DMA_BUFFER_COUNT; i++)
				if(_rx_buffers[i].state == SSC_FREE) _value++;
			break;
		case METRIC_MANAGEMENT_ERRORS:
			_value = ssc_metrics[METRIC_MANAGEMENT_ERRORS_1].value +
				ssc_metrics[METRIC_MANAGEMENT_ERRORS_2].value +
				ssc_metrics[METRIC_MANAGEMENT_ERRORS_3].value;
			break;
		default:
			break;
	}
	
	if(!valid) return 0;
	
	if(description != NULL) *description = _name;
	if(value != NULL) *value = _value;
	return 1;
}

