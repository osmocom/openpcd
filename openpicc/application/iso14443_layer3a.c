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

static enum ISO14443_STATES state = STARTING_UP;
#define PLL_LOCK_HYSTERESIS portTICK_RATE_MS*5

#define LAYER3_DEBUG usb_print_string

extern void main_help_print_buffer(ssc_dma_buffer_t *buffer, int *pktcount);
void iso14443_layer3a_state_machine (void *pvParameters)
{
	unsigned long int last_pll_lock = ~0;
	int pktcount=0;
	(void)pvParameters;
	while(1) {
		ssc_dma_buffer_t* buffer = NULL;
		portBASE_TYPE need_receive = 0, switch_on = 0;
		
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
				tc_cdiv_set_divider(32);
				tc_fdt_init();
#if 0
				ssc_tx_init();
#else
				AT91F_PIO_CfgInput(AT91C_BASE_PIOA, OPENPICC_MOD_PWM);
				AT91F_PIO_CfgPeriph(AT91C_BASE_PIOA, OPENPICC_MOD_SSC | 
						    OPENPICC_SSC_DATA | OPENPICC_SSC_DATA |
						    AT91C_PIO_PA15, 0);
#endif
				ssc_rx_init();
				
				state = POWERED_OFF;
				break;
			case POWERED_OFF:
				if(switch_on == 1) {
					state=IDLE;
					ssc_rx_mode_set(SSC_MODE_14443A_SHORT);
					ssc_rx_start();
					continue;
				}
				break;
			case IDLE:
			case HALT:
				/* Wait for REQA or WUPA (HALT: only WUPA) */
				need_receive = 1;
			default:
				break;
		}
		
		if(need_receive) {
			if(xQueueReceive(ssc_rx_queue, &buffer, portTICK_RATE_MS)) {
				portENTER_CRITICAL();
				buffer->state = PROCESSING;
				portEXIT_CRITICAL();
				
				DumpStringToUSB("Frame: ");
				DumpUIntToUSB(*(u_int32_t*)buffer->data);
				DumpStringToUSB(" ");
				main_help_print_buffer(buffer, &pktcount);
				
				switch(state) {
					case IDLE:
					case HALT:
						ssc_rx_mode_set(SSC_MODE_14443A_SHORT);
						ssc_rx_start();
						break;
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
