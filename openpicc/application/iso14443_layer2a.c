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
#include "ssc_picc.h"
#include "pll.h"
#include "tc_fdt.h"
#include "tc_cdiv.h"
#include "tc_cdiv_sync.h"
#include "load_modulation.h"

static u_int8_t fast_receive;
static volatile u_int8_t receiving = 0;

#ifdef FOUR_TIMES_OVERSAMPLING
#define RX_DIVIDER 32
#else
#define RX_DIVIDER 64
#endif

int iso14443_receive(iso14443_receive_callback_t callback, ssc_dma_rx_buffer_t **buffer, unsigned int timeout)
{
	int was_receiving = receiving;
	(void)callback;
	ssc_dma_rx_buffer_t* _buffer = NULL;
	int len;
	
	
	if(!was_receiving) {
		iso14443_l2a_rx_start();
	} else {
		/*
		 * handled by _iso14443_ssc_irq_ext below
		tc_fdt_set(0xff00);
		tc_cdiv_set_divider(RX_DIVIDER);
		tc_cdiv_sync_reset();
		*/
	}
	
	
	if(xQueueReceive(ssc_rx_queue, &_buffer, timeout)) {
		if(!was_receiving) {
			iso14443_l2a_rx_stop();
		}
		
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
	
	if(!was_receiving)
		iso14443_l2a_rx_stop();
	
	return -ETIMEDOUT;
}

int iso14443_wait_for_carrier(unsigned int timeout)
{
	(void)timeout;
	return 0;
}

int iso14443_l2a_rx_start(void)
{
	receiving = 1;
	tc_fdt_set(0xff00);
	tc_cdiv_set_divider(RX_DIVIDER);
	ssc_rx_start();
	return 0;
}

int iso14443_l2a_rx_stop(void)
{
	ssc_rx_stop();
	receiving = 0;
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

void _iso14443_ssc_irq_ext(u_int32_t ssc_sr, enum ssc_mode ssc_mode, u_int8_t* samples)
{
	(void) ssc_mode; (void) samples;
	if( (ssc_sr & AT91C_SSC_CP1) && receiving) {
		tc_fdt_set(0xff00);
		tc_cdiv_set_divider(RX_DIVIDER);
		tc_cdiv_sync_reset();
	}
}

int iso14443_layer2a_init(u_int8_t enable_fast_receive)
{
	pll_init();
    
	tc_cdiv_init();
	tc_fdt_init();
	//ssc_set_irq_extension((ssc_irq_ext_t)iso14443_layer3a_irq_ext);
	ssc_rx_init();
	ssc_tx_init();
	
	load_mod_init();
	load_mod_level(3);
	
	ssc_rx_mode_set(SSC_MODE_14443A);
	ssc_set_irq_extension(_iso14443_ssc_irq_ext);
	
	iso14443_set_fast_receive(enable_fast_receive);
	return 0;
}
