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
#include "pll.h"
#include "tc_fdt.h"
#include "tc_cdiv.h"
#include "tc_cdiv_sync.h"
#include "load_modulation.h"
#include "pio_irq.h"

#include "usb_print.h"

#define PRINT_DEBUG 0

static u_int8_t fast_receive;
static ssc_handle_t *ssc;

#ifdef FOUR_TIMES_OVERSAMPLING
#define RX_DIVIDER 32
#else
#define RX_DIVIDER 64
#endif

int iso14443_receive(iso14443_receive_callback_t callback, ssc_dma_rx_buffer_t **buffer, unsigned int timeout)
{
	(void)callback; (void)timeout;
	ssc_dma_rx_buffer_t* _buffer = NULL;
	int len;
	
	if(ssc_recv(ssc, &_buffer, timeout) == 0) {
		
		if(_buffer == NULL) {
			/* Can this happen? */
			return -ETIMEDOUT;
		}
		
		portENTER_CRITICAL();
		_buffer->state = PROCESSING;
		portEXIT_CRITICAL();
		
		len = _buffer->len_transfers;
		
		if(buffer != NULL) *buffer = _buffer;
		else {
			portENTER_CRITICAL();
			_buffer->state = FREE;
			portEXIT_CRITICAL();
		}
		
		return len;
	}
	
	if(AT91F_PIO_IsInputSet(AT91C_BASE_PIOA, OPENPICC_PIO_FRAME)) tc_cdiv_sync_reset();
	return -ETIMEDOUT;
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
	(void) data;
	if(reason == CALLBACK_RX_FRAME_BEGIN) {
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
	if( reason == CALLBACK_RX_FRAME_ENDED || reason == CALLBACK_RX_STARTED ) {
		tc_fdt_set(0xff00);
		tc_cdiv_set_divider(RX_DIVIDER);
		tc_cdiv_sync_reset();
	}
}

static void iso14443_rx_FRAME_cb(u_int32_t pio)
{
	(void)pio;
	if(PRINT_DEBUG) usb_print_string_f("°", 0);  // DEBUG OUTPUT
	if(AT91F_PIO_IsInputSet(AT91C_BASE_PIOA, OPENPICC_PIO_FRAME))
		ssc_frame_started();
}

int iso14443_layer2a_init(u_int8_t enable_fast_receive)
{
	pll_init();
    
	tc_cdiv_init();
	tc_fdt_init();
	
	load_mod_init();
	
	iso14443_set_fast_receive(enable_fast_receive);
	pio_irq_init_once();
	
	if(pio_irq_register(OPENPICC_PIO_FRAME, &iso14443_rx_FRAME_cb) >= 0) {
		if(PRINT_DEBUG) usb_print_string("FRAME irq registered\n\r"); // DEBUG OUTPUT
	}
	
	ssc = ssc_open(1, 0, SSC_MODE_14443A, iso14443_ssc_callback);
	if(ssc == NULL)
		return -EIO;
	
	// FIXME should be 3 for production, when tx code is implemented
	load_mod_level(0);
	
	return 0;
}
