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
#include "iso14443a_miller.h"
#include "led.h"

static enum ISO14443_STATES state = STARTING_UP;
const iso14443_frame ATQA_FRAME = {
	TYPE_A,
	{{STANDARD_FRAME, PARITY, ISO14443A_LAST_BIT_NONE}},
	2,
	0, 0,
	{4, 0},
	{}
};

const iso14443_frame NULL_FRAME = {
	TYPE_A,
	{{STANDARD_FRAME, PARITY, ISO14443A_LAST_BIT_NONE}},
	4, 
	0, 0,
	//{0xF3, 0xFB, 0xAE, 0xED},
	{0xFF, 0xFF, 0xFF, 0xFF},
	//{0, 0, 0, 0},
	{}
};


#define PLL_LOCK_HYSTERESIS portTICK_RATE_MS*5

#define LAYER3_DEBUG usb_print_string

#define INITIAL_STATE IDLE
//#define INITIAL_STATE ACTIVE

#if INITIAL_STATE == IDLE
#define INITIAL_FRAME ATQA_FRAME
#else
#define INITIAL_FRAME NULL_FRAME
#endif

#if 1
#define SHORT_MODE SSC_MODE_14443A_SHORT
#define STANDARD_MODE SSC_MODE_14443A_STANDARD
#else
#define SHORT_MODE SSC_MODE_14443A
#define STANDARD_MODE SSC_MODE_14443A
#endif

#define ISO14443A_TRANSMIT_AT_NEXT_INTERVAL_0 -1
#define ISO14443A_TRANSMIT_AT_NEXT_INTERVAL_1 -2

/* Transmit a frame in ISO14443A mode from buffer buf at fdt carrier cycles
 * after the end of the last modulation pause from the PCD with a clock divisor
 * of div. Set fdt to ISO14443A_TRANSMIT_AT_NEXT_INTERVAL_0 or _1 to have the 
 * transmission start at the next possible interval. Use _0 when the last bit
 * from the PCD was a 0 and _1 when it was a 1. */ 
void iso14443_transmit(ssc_dma_tx_buffer_t *buf, int fdt, int div)
{
	tc_cdiv_set_divider(div);
	if(fdt == ISO14443A_TRANSMIT_AT_NEXT_INTERVAL_0) {
		fdt = tc_fdt_get_next_slot(ISO14443A_FDT_SHORT_0, ISO14443A_FDT_SLOTLEN);
	} else if (fdt == ISO14443A_TRANSMIT_AT_NEXT_INTERVAL_1) {
		fdt = tc_fdt_get_next_slot(ISO14443A_FDT_SHORT_1, ISO14443A_FDT_SLOTLEN);
	}
	ssc_tx_fiq_fdt_cdiv = fdt -3*div -1;
	tc_fdt_set(ssc_tx_fiq_fdt_cdiv -MAX_TF_FIQ_ENTRY_DELAY -MAX_TF_FIQ_OVERHEAD);
	ssc_tx_fiq_fdt_ssc  = fdt -div +1;
	*AT91C_TC0_CCR = AT91C_TC_CLKDIS;
	ssc_tx_start(buf);
}

static int atqa_sent = 0;
/* Running in ISR mode */
void __ramfunc iso14443_layer3a_irq_ext(u_int32_t ssc_sr, enum ssc_mode ssc_mode, u_int8_t* samples)
{
	(void)ssc_sr;
	int fdt;
	if((ssc_mode == SSC_MODE_14443A_SHORT || ssc_mode == SSC_MODE_14443A) && samples) {
		ISO14443A_SHORT_TYPE sample =  *(ISO14443A_SHORT_TYPE*)samples;
		portBASE_TYPE send_atqa = 0;
		if(sample == REQA) {
			fdt = ISO14443A_FDT_SHORT_0;
			if(state == IDLE)
				send_atqa = 1;
		} else if(sample == WUPA) {
			fdt = ISO14443A_FDT_SHORT_1;
			if(state == IDLE || state == HALT)
				send_atqa = 1;
		}
		
		if(send_atqa) {
		vLedSetGreen(0);
			if(ssc_tx_buffer.state == PREFILLED && ssc_tx_buffer.source == &ATQA_FRAME) {
				ssc_tx_buffer.state = PROCESSING;
				vLedSetGreen(1);
				iso14443_transmit(&ssc_tx_buffer, fdt, 8);
				atqa_sent = 1;
				vLedSetGreen(0);
			}
		vLedSetGreen(1);
		}
		vLedSetGreen(0);
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
				src);
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

static iso14443_frame received_frame;

static void enable_reception(enum ssc_mode mode) {
	tc_fdt_set(0xff00);
	ssc_rx_mode_set(mode);
#ifdef FOUR_TIMES_OVERSAMPLING
	tc_cdiv_set_divider(32);
#else
	tc_cdiv_set_divider(64);
#endif
	ssc_rx_start();
}

extern void main_help_print_buffer(ssc_dma_rx_buffer_t *buffer, int *pktcount);
void iso14443_layer3a_state_machine (void *pvParameters)
{
	unsigned long int last_pll_lock_change = 0;
	int pktcount=0, locked, last_was_locked=0;
	(void)pvParameters;
	while(1) {
		ssc_dma_rx_buffer_t* buffer = NULL;
		portBASE_TYPE need_receive = 0, switch_on = 0;
		
		if(ssc_get_metric(SSC_ERRORS) > 0 && state != ERROR) {
			LAYER3_DEBUG("SSC overflow error, please debug\n\r");
			state = ERROR;
		}
		
		/* First let's see whether there is a reader */
		locked = pll_is_locked();
		unsigned long int now = xTaskGetTickCount();
		switch(state) {
			case STARTING_UP: /* Fall through */
			case ERROR:
				// do nothing here
				break;
			case POWERED_OFF:
				if(locked && now - last_pll_lock_change > PLL_LOCK_HYSTERESIS) { 
					/* Go to idle when in POWERED_OFF and pll 
					 * was locked for at least 
					 * PLL_LOCK_HYSTERESIS ticks */
					switch_on = 1;
					LAYER3_DEBUG("PLL locked, switching on \n\r");
				}
				break;
			default:
				if(!locked && now - last_pll_lock_change > PLL_LOCK_HYSTERESIS) {
					/* Power off when not powered off and pll
					 * was unlocked for at least  PLL_LOCK_HYSTERESIS
					 * ticks */
					state = POWERED_OFF;
					ssc_rx_stop();
					LAYER3_DEBUG("PLL lost lock, switching off \n\r");
					continue;
				} 
				break;
		}
		if( (!locked && last_was_locked) || (locked && !last_was_locked) ) 
			last_pll_lock_change = now;
		last_was_locked = locked;
		
		switch(state) {
			case STARTING_UP:
				pll_init();
			    
				tc_cdiv_init();
				tc_fdt_init();
				ssc_set_irq_extension((ssc_irq_ext_t)iso14443_layer3a_irq_ext);
				ssc_rx_init();
				ssc_tx_init();
				
				load_mod_init();
				load_mod_level(3);
				
				
				state = POWERED_OFF;
				last_was_locked = 0;
				vTaskDelay(200*portTICK_RATE_MS);
				break;
			case POWERED_OFF:
				if(switch_on == 1) {
					if(prefill_buffer(&ssc_tx_buffer, &INITIAL_FRAME)) {
						LAYER3_DEBUG("Buffer prefilled\n\r");
						DumpUIntToUSB(ssc_tx_buffer.state);
						DumpStringToUSB(" ");
						DumpUIntToUSB((unsigned int)ssc_tx_buffer.source);
						DumpStringToUSB(" ");
						DumpUIntToUSB((unsigned int)&INITIAL_FRAME);
						DumpStringToUSB(" ");
                                                DumpUIntToUSB(ssc_tx_buffer.len);
                                                DumpStringToUSB(" ");
                                                DumpBufferToUSB((char*)ssc_tx_buffer.data, ssc_tx_buffer.len);
						DumpStringToUSB("\n\r");
						state=INITIAL_STATE;
						if(INITIAL_STATE == IDLE)
							enable_reception(SHORT_MODE);
						else if(INITIAL_STATE == ACTIVE)
							enable_reception(STANDARD_MODE);
						else enable_reception(SSC_MODE_NONE);
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
			if(xQueueReceive(ssc_rx_queue, &buffer, portTICK_RATE_MS) && buffer != NULL) {
				vLedSetGreen(0);
				vLedBlinkGreen();
				portENTER_CRITICAL();
				buffer->state = PROCESSING;
				portEXIT_CRITICAL();
				u_int32_t first_sample = *(u_int32_t*)buffer->data;
				
				if(0) {
					DumpStringToUSB("Frame: ");
					DumpUIntToUSB(first_sample);
					DumpStringToUSB(" ");
					main_help_print_buffer(buffer, &pktcount);
				}
				vLedBlinkGreen();
				if(1) {
					int i = usb_print_set_default_flush(0);
					DumpBufferToUSB((char*)buffer->data, (buffer->len+7)/8);
					DumpStringToUSB(" Decoded: ");
					DumpUIntToUSB(buffer->len);
					DumpStringToUSB(" ");
					iso14443a_decode_miller(&received_frame, buffer->data, (buffer->len+7)/8);
					DumpBufferToUSB((char*)received_frame.data, received_frame.numbytes + (received_frame.numbits+7)/8);
					DumpStringToUSB(" ");
					DumpUIntToUSB(received_frame.parameters.a.last_bit);
					DumpStringToUSB("\n\r");
					usb_print_set_default_flush(i);
				}
				
				switch(state) {
					case IDLE:
					case HALT:
						if(first_sample == WUPA || (state==IDLE && first_sample==REQA)) {
							/* Need to transmit ATQA */
							LAYER3_DEBUG("Received ");
							LAYER3_DEBUG(first_sample == WUPA ? "WUPA" : "REQA");
							if(atqa_sent) {
								LAYER3_DEBUG(", woke up to send ATQA\n\r");
								atqa_sent = 0;
							}
							/* For debugging, wait 1ms, then wait for another frame 
							 * Normally we'd go to anticol from here*/
							vTaskDelay(portTICK_RATE_MS);
							if(prefill_buffer(&ssc_tx_buffer, &ATQA_FRAME)) {
								enable_reception(SHORT_MODE);
							}
						} else {
							/* Wait for another frame */
							enable_reception(SHORT_MODE);
						}
						break;
					case ACTIVE:
					case ACTIVE_STAR:
							/* Wait for another frame */
							if(0) {
								ssc_rx_mode_set(STANDARD_MODE);
								ssc_rx_start();
							} else {
								//vTaskDelay(portTICK_RATE_MS);
								if(ssc_tx_buffer.source == &ATQA_FRAME) ssc_tx_buffer.state = FREE;
								if(prefill_buffer(&ssc_tx_buffer, &NULL_FRAME)) {
									usb_print_string_f("Sending response ...",0);
									ssc_tx_buffer.state = PROCESSING;
									iso14443_transmit(&ssc_tx_buffer, ISO14443A_TRANSMIT_AT_NEXT_INTERVAL_1, 8);
									while( ssc_tx_buffer.state != FREE ) {
										vTaskDelay(portTICK_RATE_MS);
									}
									usb_print_string("done\n\r");
									usb_print_flush();
								}
							/* Wait for another frame */
							enable_reception(STANDARD_MODE);
							}
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
