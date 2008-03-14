/***************************************************************
 *
 * OpenPICC - ISO 14443 Layer 2 Type A PICC transceiver code
 * Manages receiving, sending and parts of framing
 * 
 * This does not fully implement layer 2 in that it won't 
 * automatically call the Miller decoder or Manchester encoder
 * for you. Instead you'll be given ssc rx buffer pointers and
 * are expected to hand in ssc tx buffer pointers. You've got
 * to call iso14443a_manchester and iso14443a_miller yourself.
 * The reason is that this makes it possible for the layer 3
 * implementation to work on raw samples without en/de-coding
 * time to enable fast responses during anticollision.
 *
 * Copyright 2008 Henryk Plötz <henryk@ploetzli.ch>
 *
 ***************************************************************

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; version 2.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*/

#include <FreeRTOS.h>
#include <board.h>
#include <task.h>
#include <errno.h>

#include "openpicc.h"
#include "iso14443_layer2a.h"
#include "ssc.h"
#include "ssc_buffer.h"
#include "pll.h"
#include "tc_fdt.h"
#include "tc_cdiv.h"
#include "tc_cdiv_sync.h"
#include "tc_recv.h"
#include "load_modulation.h"
#include "clock_switch.h"
#include "pio_irq.h"

#include "usb_print.h"
#include "cmd.h"

#define PRINT_DEBUG 0

static u_int8_t fast_receive;
static u_int8_t tx_pending=0;
static u_int8_t rx_pending=0;
static iso14443_receive_callback_t callback=NULL;
static ssc_handle_t *ssc;
static tc_recv_handle_t th;

#ifdef FOUR_TIMES_OVERSAMPLING
#define RX_DIVIDER 32
#else
#define RX_DIVIDER 64
#endif

int iso14443_receive(iso14443_receive_callback_t _callback, iso14443_frame **frame, unsigned int timeout)
{
	iso14443_frame* _frame = NULL;
	
	if(rx_pending) {
		return -EALREADY;
	}
	rx_pending=1;
	callback=_callback;
	
	if(tc_recv_receive(th, &_frame, timeout) == 0) {
		
		if(_frame == NULL) {
			/* Can this happen? */
			rx_pending=0;
			callback=NULL;
			return -ETIMEDOUT;
		}
		
		portENTER_CRITICAL();
		_frame->state = FRAME_PROCESSING;
		portEXIT_CRITICAL();
		
		if(callback != NULL && !fast_receive) {
			callback(NULL, _frame, 0);
		}
		
		if(frame != NULL) *frame = _frame;
		else {
			portENTER_CRITICAL();
			_frame->state = FRAME_FREE;
			portEXIT_CRITICAL();
		}
		
		rx_pending=0;
		callback=NULL;
		return 0;
	}
	
	/* Note: There is the remote chance of a race condition probability here if
	 * a frame start was received right before the timeout for this function 
	 * expired. In the future one might want to replace this with some safer code
	 * (hmm, maybe check TC2_CV?) but for now it's an essential safeguard to prevent
	 * a hung receiver when no proper frame end is signalled to iso14443_ssc_callback
	 * and therefore the callback never resets the flipflop */
	if(!tx_pending) {
		if(AT91F_PIO_IsInputSet(AT91C_BASE_PIOA, OPENPICC_PIO_FRAME)) tc_cdiv_sync_reset();
	}
	
	rx_pending=0;
	callback=NULL;
	return -ETIMEDOUT;
}

int iso14443_transmit(ssc_dma_tx_buffer_t *buffer, unsigned int fdt, u_int8_t async, unsigned int timeout)
{
	if(tx_pending) 
		return -EBUSY;
	
	tx_pending = 1;
	
	/* Immediately set up FDT and clock */
	clock_switch(CLOCK_SELECT_CARRIER);
	ssc_set_gate(0);
	tc_fdt_set(fdt);
	// We'll keep the divider at 8
	//tc_cdiv_set_divider(8); // FIXME Magic hardcoded number
	
	if(!async) {
		/* FIXME Implement */
		(void)timeout;
		tx_pending = 0;
		return -EINVAL;
	}
	
	int ret = ssc_send(ssc, buffer);
	if(ret < 0) {
		tx_pending = 0;
		return ret;
	}
	
	if(!async) {
		/* FIXME Wait for completion, timeout or abort */
	}
	
	return 0;
}

int iso14443_tx_abort(void)
{
	int ret = 0;
	taskENTER_CRITICAL();
	ret = ssc_send_abort(ssc);
	tx_pending = 0;
	taskEXIT_CRITICAL();
	return ret;
}

int iso14443_wait_for_carrier(unsigned int timeout)
{
	(void)timeout;
	return 0;
}

u_int8_t iso14443_set_fast_receive(u_int8_t enable_fast_receive)
{
	u_int8_t old_value = fast_receive;
	fast_receive = enable_fast_receive;
	return old_value;
}

u_int8_t iso14443_get_fast_receive(void)
{
	return fast_receive;
}

static void iso14443_ssc_callback(ssc_callback_reason reason, void *data)
{
	if( reason == SSC_CALLBACK_SETUP ) {
		// We'll keep the divider at 8
		tc_cdiv_set_divider(8); // FIXME Magic hardcoded number
	}

	if(reason == SSC_CALLBACK_RX_FRAME_BEGIN) {
		/* Busy loop for the frame end */
		int *end_asserted = data, i=0;
		for(i=0; i<96000; i++) 
			if(*AT91C_TC2_CV > 2*128) { // FIXME magic number
				*end_asserted = 1;
				if(PRINT_DEBUG) usb_print_string_f("^", 0); // DEBUG OUTPUT
				break;
			}
		return;
	}
	
	if(reason == SSC_CALLBACK_TX_FRAME_ENDED) {
		tx_pending = 0;
	}
	
	if( reason == SSC_CALLBACK_RX_FRAME_ENDED && fast_receive ) {
		clock_switch(CLOCK_SELECT_CARRIER); /* A Tx might be coming up */
		
		ssc_dma_rx_buffer_t *buffer = data;
		if(callback != NULL)
			callback(buffer, NULL, 1);
	}
	
	if( (reason == SSC_CALLBACK_RX_FRAME_ENDED && !tx_pending) || reason == SSC_CALLBACK_RX_STARTING 
			|| reason == SSC_CALLBACK_TX_FRAME_ENDED ) {
		/* For regular SSC Rx we'd set the clock to
		// clock_switch(CLOCK_SELECT_PLL);
		 * however, the SSC Rx code is going to go away (at least for 14443-A)
		 * and switching clocks messes up the Tx timing, so we do a */
		clock_switch(CLOCK_SELECT_CARRIER);
		ssc_set_gate(1);
		tc_fdt_set(0xff00);
		// We'll keep the divider at 8
		//tc_cdiv_set_divider(RX_DIVIDER);
		tc_cdiv_sync_reset();
#if 1
		int old=usb_print_set_default_flush(0);
		DumpStringToUSB("["); DumpUIntToUSB(reason); DumpStringToUSB("]");
		usb_print_set_default_flush(old);
#endif
	}
}

static void iso14443_tc_recv_callback(tc_recv_callback_reason reason, void *data)
{
	if( reason == TC_RECV_CALLBACK_RX_FRAME_ENDED && fast_receive ) {
		clock_switch(CLOCK_SELECT_CARRIER); /* A Tx might be coming up */
		
		iso14443_frame *frame = data;
		if(callback != NULL)
			callback(NULL, frame, 1);
	}
	
	if( (reason == TC_RECV_CALLBACK_RX_FRAME_ENDED && !tx_pending) ||
			reason == TC_RECV_CALLBACK_SETUP ) {
		/* For T/C Rx we set the clock to */
		clock_switch(CLOCK_SELECT_CARRIER);
		ssc_set_gate(1);
		tc_fdt_set(0xff00);
		// We'll keep the divider at 8
		//tc_cdiv_set_divider(RX_DIVIDER);
		tc_cdiv_sync_reset();
	}
}

int iso14443_layer2a_init(u_int8_t enable_fast_receive)
{
	pll_init();
    
	tc_cdiv_init();
	tc_fdt_init();
	
	clock_switch_init();
	load_mod_init();
	
	iso14443_set_fast_receive(enable_fast_receive);
	pio_irq_init_once();
	
	ssc = ssc_open(0, 1, SSC_MODE_14443A, iso14443_ssc_callback);
	if(ssc == NULL)
		return -EIO;
	
	if(tc_recv_init(&th, 0, iso14443_tc_recv_callback) < 0) {
		ssc_close(ssc);
		return -EIO;
	}
	
	load_mod_level(3);
	
	return 0;
}
