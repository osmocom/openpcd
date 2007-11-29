/***************************************************************
 *
 * OpenPICC - ISO 14443 Layer 3 Type A state machine
 *
 * Copyright 2007 Henryk Plötz <henryk@ploetzli.ch>
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

#include "openpicc.h"
#include "iso14443_layer3a.h"
#include "ssc_picc.h"
#include "pll.h"
#include "tc_fdt.h"
#include "tc_cdiv.h"
#include "tc_cdiv_sync.h"
#include "usb_print.h"
#include "cmd.h"
#include "load_modulation.h"
#include "decoder.h"
#include "iso14443a_manchester.h"
#include "led.h"

static enum ISO14443_STATES state = STARTING_UP;
const iso14443_frame ATQA_FRAME = {
	TYPE_A,
	{{STANDARD_FRAME, NO_PARITY}},
	2, 
	0, 0,
	{4, 0},
	{}
};

const iso14443_frame NULL_FRAME = {
	TYPE_A,
	{{STANDARD_FRAME, PARITY}},
	4, 
	0, 0,
	{0, 0, 0, 0},
	{}
};


#define PLL_LOCK_HYSTERESIS portTICK_RATE_MS*5

#define LAYER3_DEBUG usb_print_string

#define INITIAL_STATE IDLE
//#define INITIAL_STATE ACTIVE

/* Running in ISR mode */
void __ramfunc iso14443_layer3a_irq_ext(u_int32_t ssc_sr, enum ssc_mode ssc_mode, u_int8_t* samples)
{
	(void)ssc_sr;
	if(ssc_mode == SSC_MODE_14443A_SHORT && samples) {
		ISO14443A_SHORT_TYPE sample =  *(ISO14443A_SHORT_TYPE*)samples;
		portBASE_TYPE send_atqa = 0;
		if(sample == REQA) {
			tc_fdt_set(ISO14443A_FDT_SHORT_0);
			if(state == IDLE)
				send_atqa = 1;
		} else if(sample == WUPA) {
			tc_fdt_set(ISO14443A_FDT_SHORT_1);
			if(state == IDLE || state == HALT)
				send_atqa = 1;
		}
		
		if(send_atqa) {
		vLedSetGreen(0);
			if(ssc_tx_buffer.state == PREFILLED && ssc_tx_buffer.source == &ATQA_FRAME) {
				ssc_tx_buffer.state = PROCESSING;
				vLedSetGreen(1);
				tc_cdiv_set_divider(8);
				ssc_tx_start(&ssc_tx_buffer);
				vLedSetGreen(0);
			}
		vLedSetGreen(1);
		}
	}
}

#define FALSE (0!=0)
static int prefill_buffer(ssc_dma_tx_buffer_t *dest, const iso14443_frame *src) {
	portENTER_CRITICAL();
	if(dest->state == FREE) {
		dest->state = PROCESSING;
		portEXIT_CRITICAL();
		dest->source = (void*)src;
		dest->len = sizeof(ssc_tx_buffer.data);
		int ret = manchester_encode(dest->data,
				dest->len,
				&ATQA_FRAME);
		if(ret>0) {
			dest->len = ret;
			portENTER_CRITICAL();
			dest->state = PREFILLED;
			portEXIT_CRITICAL();
		} else {
			portENTER_CRITICAL();
			dest->state = FREE;
			portEXIT_CRITICAL();
		}
		return ret > 0;
	} else if(dest->state == PREFILLED) {
		portEXIT_CRITICAL();
		return dest->source == src;
	} else {
		portEXIT_CRITICAL();
		return FALSE;
	}
	
}

extern void main_help_print_buffer(ssc_dma_rx_buffer_t *buffer, int *pktcount);
void iso14443_layer3a_state_machine (void *pvParameters)
{
	unsigned long int last_pll_lock = ~0;
	int pktcount=0;
	(void)pvParameters;
	while(1) {
		ssc_dma_rx_buffer_t* buffer = NULL;
		portBASE_TYPE need_receive = 0, switch_on = 0;
		
		if(ssc_get_overflows() > 0 && state != ERROR) {
			LAYER3_DEBUG("SSC overflow error, please debug\n\r");
			state = ERROR;
		}
		
		/* First let's see whether there is a reader */
		switch(state) {
			case STARTING_UP: /* Fall through */
			case ERROR:
				// do nothing here
				break;
			case POWERED_OFF:
				if(pll_is_locked()) {
					unsigned long int now = xTaskGetTickCount();
					if(now - last_pll_lock > PLL_LOCK_HYSTERESIS) { 
						/* Go to idle when in POWERED_OFF and pll 
						 * was locked for at least 
						 * PLL_LOCK_HYSTERESIS ticks */
						switch_on = 1;
						last_pll_lock = ~0;
						LAYER3_DEBUG("PLL locked, switching on \n\r");
					} else last_pll_lock = now; 
				} else last_pll_lock = ~0;
				break;
			default:
				if(!pll_is_locked()) {
					unsigned long int now = xTaskGetTickCount();
					if(now - last_pll_lock > PLL_LOCK_HYSTERESIS) {
						/* Power off when not powered off and pll
						 * was unlocked for at least  PLL_LOCK_HYSTERESIS
						 * ticks */
						state = POWERED_OFF;
						ssc_rx_stop();
						last_pll_lock = ~0;
						LAYER3_DEBUG("PLL lost lock, switching off \n\r");
						continue; 
					} else last_pll_lock = now; 
				} else last_pll_lock = ~0;
				break;
		}
		
		switch(state) {
			case STARTING_UP:
				pll_init();
			    
				tc_cdiv_init();
#ifdef FOUR_TIMES_OVERSAMPLING
				tc_cdiv_set_divider(32);
#else
				tc_cdiv_set_divider(64);
#endif
				tc_fdt_init();
				ssc_set_irq_extension((ssc_irq_ext_t)iso14443_layer3a_irq_ext);
				ssc_rx_init();
				ssc_tx_init();
				
				load_mod_init();
				load_mod_level(3);
				
				
				state = POWERED_OFF;
				break;
			case POWERED_OFF:
				if(switch_on == 1) {
					if(prefill_buffer(&ssc_tx_buffer, &ATQA_FRAME)) {
						state=INITIAL_STATE;
						if(INITIAL_STATE == IDLE)
							ssc_rx_mode_set(SSC_MODE_14443A_SHORT);
						else if(INITIAL_STATE == ACTIVE)
							ssc_rx_mode_set(SSC_MODE_14443A_STANDARD);
						else ssc_rx_mode_set(SSC_MODE_NONE);
						ssc_rx_start();
					} else {
						LAYER3_DEBUG("SSC TX overflow error, please debug");
						state=ERROR;
					}
					continue;
				}
				break;
			case IDLE:
			case HALT:
				/* Wait for REQA or WUPA (HALT: only WUPA) */
				need_receive = 1;
			case ACTIVE:
			case ACTIVE_STAR:
				need_receive = 1;
			default:
				break;
		}
		
		if(need_receive) {
			if(xQueueReceive(ssc_rx_queue, &buffer, portTICK_RATE_MS)) {
				vLedSetGreen(0);
				portENTER_CRITICAL();
				buffer->state = PROCESSING;
				portEXIT_CRITICAL();
				u_int32_t first_sample = *(u_int32_t*)buffer->data;
				
				DumpStringToUSB("Frame: ");
				DumpUIntToUSB(first_sample);
				DumpStringToUSB(" ");
				main_help_print_buffer(buffer, &pktcount);
				
				switch(state) {
					case IDLE:
					case HALT:
						if(first_sample == WUPA || (state==IDLE && first_sample==REQA)) {
							/* Need to transmit ATQA */
							LAYER3_DEBUG("Received ");
							LAYER3_DEBUG(first_sample == WUPA ? "WUPA" : "REQA");
							LAYER3_DEBUG(" waking up to send ATQA\n\r");
							if(ssc_tx_buffer.state == PROCESSING) {
								LAYER3_DEBUG("Buffer ");
								DumpUIntToUSB(ssc_tx_buffer.len);
								LAYER3_DEBUG(" ");
								DumpBufferToUSB((char*)ssc_tx_buffer.data, ssc_tx_buffer.len);
								LAYER3_DEBUG("\n\r");
							}
							/*portENTER_CRITICAL();
							if(ssc_tx_buffer.state != FREE) {
								portEXIT_CRITICAL();
								* Wait for another frame */ /*
								ssc_rx_mode_set(SSC_MODE_14443A_SHORT);
								ssc_rx_start();
							} else {
								ssc_tx_buffer.state = PROCESSING;
								portEXIT_CRITICAL();
								ssc_tx_buffer.len = sizeof(ssc_tx_buffer.data);
								unsigned int ret = 
									manchester_encode(ssc_tx_buffer.data,
										ssc_tx_buffer.len,
										&ATQA_FRAME);
								if(ret>0) {
									ssc_tx_buffer.len = ret;
									ssc_tx_start(&ssc_tx_buffer);
									LAYER3_DEBUG("Buffer ");
									DumpUIntToUSB(ret);
									LAYER3_DEBUG(" ");
									DumpBufferToUSB((char*)ssc_tx_buffer.data, ssc_tx_buffer.len);
									LAYER3_DEBUG("\n\r");
								} else {
									portENTER_CRITICAL();
									ssc_tx_buffer.state = FREE;
									portEXIT_CRITICAL();
									* Wait for another frame */ /*
									ssc_rx_mode_set(SSC_MODE_14443A_SHORT);
									ssc_rx_start();
								}
							}*/
						} else {
							/* Wait for another frame */
							ssc_rx_mode_set(SSC_MODE_14443A_SHORT);
							ssc_rx_start();
						}
						break;
					case ACTIVE:
					case ACTIVE_STAR:
							/* Wait for another frame */
							ssc_rx_mode_set(SSC_MODE_14443A_STANDARD);
							ssc_rx_start();
					default:
						break;
				}
				
				portENTER_CRITICAL();
				buffer->state = FREE;
				portEXIT_CRITICAL();
			}
		} else vTaskDelay(portTICK_RATE_MS);
	}
}
