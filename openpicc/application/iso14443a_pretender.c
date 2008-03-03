/***************************************************************
 *
 * OpenPICC - ISO 14443 Layer 3 response pretender, 
 * proof of concept
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
#include <string.h>
#include <stdlib.h>

#include "openpicc.h"
#include "ssc_buffer.h"
#include "iso14443.h"
#include "iso14443_layer2a.h"
#include "iso14443a_pretender.h"
#include "iso14443a_manchester.h"
#include "usb_print.h"
#include "cmd.h"
extern volatile int fdt_offset;
#include "led.h"

static const iso14443_frame ATQA_FRAME = {
	TYPE_A,
	{{STANDARD_FRAME, PARITY, ISO14443A_LAST_BIT_NONE}},
	2,
	0, 0,
	{4, 0},
	{}
};

static const iso14443_frame LONG_FRAME = {
	TYPE_A,
	{{STANDARD_FRAME, PARITY, ISO14443A_LAST_BIT_NONE}},
	40,
	0, 0,
		{	0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
			0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 
			0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 
			0, 1, 2, 3, 4, 5, 6, 7, 8, 9, },
	{}
};

#define FRAME_SIZE(bytes) (2* (1+(9*bytes)+1) )
#define SIZED_BUFFER(bytes) struct { int len; u_int8_t data[FRAME_SIZE(bytes)]; }

static SIZED_BUFFER(2) ATQA_BUFFER;
static SIZED_BUFFER(40) LONG_BUFFER;

static ssc_dma_tx_buffer_t tx_buffer;

static void fast_receive_callback(ssc_dma_rx_buffer_t *buffer, u_int8_t in_irq)
{
	(void)buffer; (void)in_irq;
	unsigned int buffy=0;
	u_int32_t cv = *AT91C_TC2_CV;
	
	const iso14443_frame *tx_frame = NULL;
	int fdt = 0;
	
	switch(buffer->len_transfers) {
	case 3: case 4: /* REQA (7 bits) */
	case 7: case 8: case 9:
		tx_frame = &ATQA_FRAME;
		fdt = 1172;
		break;
	//case 6: case 7: /* ANTICOL (2 bytes) */
	case 22: /* SELECT (9 bytes) */
		
		break;
	case 577:
		usb_print_string_f("f", 0); 
		buffy=(unsigned int)buffer;
		break;
	}
	
	/* Add some extra room to the fdt for testing */
	//fdt += 3*128;
	fdt += fdt_offset;
	
	
	int ret = 0;
	if(tx_frame != NULL) {
		
		tx_buffer.source = (void*)tx_frame;
		if(tx_frame == &ATQA_FRAME) {
			memcpy(tx_buffer.data, ATQA_BUFFER.data, ATQA_BUFFER.len);
			ret = tx_buffer.len = ATQA_BUFFER.len;
		} else if(tx_frame == &LONG_FRAME) {
			memcpy(tx_buffer.data, LONG_BUFFER.data, LONG_BUFFER.len);
			ret = tx_buffer.len = LONG_BUFFER.len;
		} else {
			tx_buffer.len = sizeof(tx_buffer.data);
			ret = manchester_encode(tx_buffer.data,
					tx_buffer.len,
					tx_frame);
		}
		if(ret >= 0) {
			tx_buffer.state = FULL;
			tx_buffer.len = ret;
			if(	iso14443_transmit(&tx_buffer, fdt, 1, 0) < 0) {
				tx_buffer.state = FREE;
				usb_print_string_f("Tx failed ", 0);
			}
		} else tx_buffer.state = FREE;

	}
	
	u_int32_t cv2 = *AT91C_TC2_CV;
	int old=usb_print_set_default_flush(0);
	DumpUIntToUSB(cv);
	DumpStringToUSB(":");
	DumpUIntToUSB(cv2);
	if(ret<0) {
		DumpStringToUSB("!");
		DumpUIntToUSB(-ret);
	}
	if(buffy!=0) {
		DumpStringToUSB("§");
		DumpUIntToUSB(buffy);
	}
	usb_print_set_default_flush(old);
	usb_print_string_f("%", 0);
}

void iso14443a_pretender (void *pvParameters)
{
	(void)pvParameters;
	int res;
	
	/* Delay until USB print etc. are ready */
	int i;
	for(i=0; i<=1000; i++) {
		vLedSetBrightness(LED_RED, i);
		vLedSetBrightness(LED_GREEN, i);
		vTaskDelay(1*portTICK_RATE_MS);
	}
	
	for(i=0; i<=1000; i+=4) {
		vLedSetBrightness(LED_GREEN, 1000-i);
		vTaskDelay(1*portTICK_RATE_MS);
	}
	
	ATQA_BUFFER.len = manchester_encode(ATQA_BUFFER.data, sizeof(ATQA_BUFFER.data), &ATQA_FRAME);
	LONG_BUFFER.len = manchester_encode(LONG_BUFFER.data, sizeof(LONG_BUFFER.data), &LONG_FRAME);
	if(ATQA_BUFFER.len < 0 || LONG_BUFFER.len < 0) {
		usb_print_string("Buffer prefilling failed\n\r");
		while(1) {
			for(i=1000; i<=3000; i++) {
				vLedSetBrightness(LED_GREEN, abs(1000-(i%2000)));
				vTaskDelay(1*portTICK_RATE_MS);
			}
		}
	}
	
	do {
		res = iso14443_layer2a_init(1);
		if(res < 0) {
			usb_print_string("Pretender: Initialization failed\n\r");
			for(i=0; i<=10000; i++) {
				vLedSetBrightness(LED_RED, abs(1000-(i%2000)));
				vTaskDelay(1*portTICK_RATE_MS);
			}
		}
	} while(res < 0);
	
	
	usb_print_string("Waiting for carrier. ");
	while(iso14443_wait_for_carrier(1000 * portTICK_RATE_MS) != 0) {
	}
	usb_print_string("Carrier detected.\n\r");
	
	for(i=0; i<=1000; i+=4) {
		vLedSetBrightness(LED_RED, 1000-i);
		vTaskDelay(1*portTICK_RATE_MS);
	}
	
	vTaskDelay(250*portTICK_RATE_MS);
	vLedSetGreen(0);
	vLedSetRed(0);
	
	int current_detected = 0, last_detected = 0;
	portTickType last_switched = 0;
#define DETECTED_14443A_3 1
#define DETECTED_14443A_4 2
#define DETECTED_MIFARE 4
	
	while(true) {
		ssc_dma_rx_buffer_t *buffer = 0;
		res = iso14443_receive(fast_receive_callback, &buffer, 1000 * portTICK_RATE_MS);
		if(res >= 0) {
			DumpUIntToUSB(buffer->len_transfers);
			DumpStringToUSB("\n\r");
			
			switch(buffer->len_transfers) {
			case 3: case 4: /* REQA (7 bits) */
				current_detected |= DETECTED_14443A_3;
				break;
			case 6: case 7: /* ANTICOL (2 bytes) */
			case 22: /* SELECT (9 bytes) */
				current_detected |= DETECTED_14443A_3;
				break;
			case 10: case 11: /* AUTH1A (or any other four byte frame) */
			case 19: case 20: /* AUTH2 (8 bytes) */
				current_detected |= DETECTED_MIFARE;
				break;
			}
			
			portENTER_CRITICAL();
			buffer->state = FREE;
			portEXIT_CRITICAL();
		} else {
			if(res != -ETIMEDOUT) {
				usb_print_string("Receive error: ");
				switch(res) {
					case -ENETDOWN:   usb_print_string("PLL is not locked or PLL lock lost\n\r"); break;
					case -EBUSY:      usb_print_string("A Tx is currently running or pending, can't receive\n\r"); break;
					case -EALREADY:   usb_print_string("There's already an iso14443_receive() invocation running\n\r"); break;
				}
				vTaskDelay(1000 * portTICK_RATE_MS); // FIXME Proper error handling, e.g. wait for Tx end in case of EBUSY
			} else if(0) {
				DumpStringToUSB("\n\r");
				DumpTimeToUSB(xTaskGetTickCount());
				usb_print_string(": -- Mark --");
			}
		}
		
		if( last_switched < xTaskGetTickCount()-(1000*portTICK_RATE_MS) ) {
			last_detected = current_detected;
			current_detected = 0;
			last_switched = xTaskGetTickCount();
		}

#if 0
		if(last_detected & (DETECTED_14443A_3 | DETECTED_14443A_4 | DETECTED_MIFARE)) {
			if(last_detected & DETECTED_MIFARE) {
				vLedSetGreen(0);
				vLedSetRed(1);
			} else if(last_detected & DETECTED_14443A_4) {
				vLedSetGreen(1);
				vLedSetRed(0);
			} else {
				vLedSetGreen(1);
				vLedSetRed(1);
			}
		} else {
			vLedSetGreen(0);
			vLedSetRed(0);
		}
#endif
	}
}
