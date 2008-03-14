/***************************************************************
 *
 * OpenPICC - ISO 14443 Layer 2 Type A Sniffer
 * Also serves as PoC code for iso14443_layer2a usage
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
#include <stdlib.h>

#include "openpicc.h"
#include "ssc_buffer.h"
#include "iso14443.h"
#include "iso14443_sniffer.h"
#include "iso14443_layer2a.h"
#include "iso14443a_miller.h"
#include "usb_print.h"
#include "cmd.h"
#include "led.h"

static iso14443_frame *rx_frame;

void iso14443_sniffer (void *pvParameters)
{
	(void)pvParameters;
	int res;
	
	/* Delay until USB print etc. are ready */
	vTaskDelay(1000 * portTICK_RATE_MS);
	
	do {
		res = iso14443_layer2a_init(0);
		if(res < 0) {
			usb_print_string("Sniffer: Initialization failed\n\r");
			vTaskDelay(10000 * portTICK_RATE_MS);
		}
	} while(res < 0);
	
	
	//while(1) { static int i=0; vTaskDelay(10*portTICK_RATE_MS); vLedSetBrightness(LED_RED, abs(1000-i)); i=(i+8)%2000; }
		
	usb_print_string("Waiting for carrier. ");
	while(iso14443_wait_for_carrier(1000 * portTICK_RATE_MS) != 0) {
	}
	usb_print_string("Carrier detected.\n\r");
	
	while(true) {
		res = iso14443_receive(NULL, &rx_frame, 20000 * portTICK_RATE_MS);
		if(res >= 0) {
			//DumpStringToUSB("\n\r");
			DumpTimeToUSB(xTaskGetTickCount());
			usb_print_string(": Frame received, consists of ");
			
			DumpUIntToUSB(rx_frame->numbytes);
			usb_print_string(" bytes and ");
			DumpUIntToUSB(rx_frame->numbits);
			usb_print_string(" bits:  ");
			DumpBufferToUSB((char*)rx_frame->data, rx_frame->numbytes + (rx_frame->numbits+7)/8 );
			usb_print_string("\n\r");
			
			portENTER_CRITICAL();
			rx_frame->state = FRAME_FREE;
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
	}
}
