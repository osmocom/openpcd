/***************************************************************
 *
 * OpenPICC - T/C based dumb sniffer
 * 
 * TC2 will reset its value on each falling edge of SSC_DATA, we
 * will have the FIQ store the TC2 value on each rising edge of
 * SSC_DATA. This will give us a count of carrier cycles between
 * modulation pauses which should be enough information to decode
 * the modified miller encoding.
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
#include <openpicc.h>
#include <task.h>
#include <semphr.h>
#include <string.h>

#include <USB-CDC.h>

#include "tc_sniffer.h"
#include "load_modulation.h"
#include "pll.h"
#include "tc_fdt.h"
#include "usb_print.h"
#include "cmd.h"
#include "pio_irq.h"
#include "led.h"
#include "clock_switch.h"
#include "performance.h"

#include "iso14443a_diffmiller.h"

/* Problem: We want to receive data from the FIQ without locking (the FIQ must not be blocked ever)
 * Strategy: Double buffering.
 */

static xSemaphoreHandle data_semaphore;
struct diffmiller_state *decoder;
iso14443_frame rx_frame;

#define WAIT_TICKS (20*portTICK_RATE_MS)

portBASE_TYPE currently_sniffing = 0;
enum { NONE, REQUEST_START, REQUEST_STOP } request_change = REQUEST_START;

//#define PRINT_TIMES
//#define USE_BINARY_PROTOCOL

#define MIN(a, b) ((a)>(b)?(b):(a))
static int overruns = 0; 
static void handle_buffer(u_int32_t data[], unsigned int count)
{
#ifdef USE_BINARY_PROTOCOL
		vUSBSendBuffer_blocking((unsigned char*)(&data[0]), 0, MIN(count,BUFSIZE)*4, WAIT_TICKS);
		if(count >= BUFSIZE)
			vUSBSendBuffer_blocking((unsigned char*)"////", 0, 4, WAIT_TICKS);
		else
			vUSBSendBuffer_blocking((unsigned char*)"____", 0, 4, WAIT_TICKS);
#elif defined(PRINT_TIMES)
		unsigned int i=0;
		for(i=0; i<count; i++) {
			DumpUIntToUSB(data[i]);
			DumpStringToUSB(" ");
		}
		DumpStringToUSB("\n\r");
#else
		unsigned int offset = 0;
		while(offset < count) {
			int ret = iso14443a_decode_diffmiller(decoder, &rx_frame, data, &offset, count);
			/*
			DumpStringToUSB("\n\r");
			if(ret < 0) {
				DumpStringToUSB("-");
				DumpUIntToUSB(-ret);
			} else {
				DumpUIntToUSB(ret);
			}
			DumpStringToUSB(" ");
			DumpUIntToUSB(offset); DumpStringToUSB(" "); DumpUIntToUSB(count); DumpStringToUSB(" "); DumpUIntToUSB(overruns);
			DumpStringToUSB("\n\r");
			if(ret >= 0) {
				DumpStringToUSB("Frame finished, ");
				DumpUIntToUSB(rx_frame.numbytes);
				DumpStringToUSB(" bytes, ");
				DumpUIntToUSB(rx_frame.numbits);
				DumpStringToUSB(" bits\n\r");
				switch(rx_frame.parameters.a.crc) {
				case CRC_OK: DumpStringToUSB("CRC OK\n\r"); break;
				case CRC_ERROR: DumpStringToUSB("CRC ERROR\n\r"); break;
				case CRC_UNCALCULATED: DumpStringToUSB("CRC UNCALCULATED\n\r"); break;
				}
			}*/
			(void)ret;
		}
#endif
}

void flush_buffer(fiq_buffer_t *buffer)
{
	/* Write all data from the given buffer out, then zero the count */
	if(buffer->count > 0) {
		if(buffer->count >= BUFSIZE) {
			DumpStringToUSB("Warning: Possible buffer overrun detected\n\r");
			overruns++;
		}
		buffer->count = MIN(buffer->count, BUFSIZE);
		handle_buffer(buffer->data, buffer->count);
		buffer->count = 0;
	}
}

void start_stop_sniffing(void)
{
	if(currently_sniffing)
		request_change = REQUEST_STOP;
	else
		request_change = REQUEST_START;
}

static portBASE_TYPE tc_sniffer_irq(u_int32_t pio, portBASE_TYPE xTaskWoken)
{
	(void)pio;
	usb_print_string_f("?", 0);
	xTaskWoken = xSemaphoreGiveFromISR(data_semaphore, xTaskWoken);
	return xTaskWoken;
}

static void main_loop(void)
{
	int current = 0;
	
	while(1) {
		/* Main loop of the sniffer */
		//vTaskDelay(1000 * portTICK_RATE_MS);
		
			u_int32_t start = *AT91C_TC2_CV;
			int next = (current+1)%(sizeof(fiq_buffers)/sizeof(fiq_buffers[0]));
			flush_buffer( &fiq_buffers[next] );
			/* The buffer designated by next is now empty, give it to the fiq,
			 * we'll just guess that this write is atomic */ 
			tc_sniffer_next_buffer_for_fiq = &fiq_buffers[current=next];
			u_int32_t stop = *AT91C_TC2_CV;
			
			DumpStringToUSB("{"); DumpUIntToUSB(start); DumpStringToUSB(":"); DumpUIntToUSB(stop); DumpStringToUSB("}");
			if(*AT91C_TC2_CV > 2*128) {
				u_int32_t dummybuf[1] = {*AT91C_TC2_CV};
				handle_buffer(dummybuf, 1);
				
				usb_print_string_f("[", 0);
				while(xSemaphoreTake(data_semaphore, portMAX_DELAY) == pdFALSE) ;
				usb_print_string_f("]", 0);
			}
	}	
}

static u_int32_t testdata[] = {65535, 75, 138, 75, 138, 139, 139, 300};
static u_int32_t testdata2[] = {65535, 80, 144, 208, 208, 208, 80, 80, 208, 208, 208, 208, 80, 144, 80, 144, 144, 80, 144, 208, 81, 80, 80, 81, 80, 81, 209, 209, 209, 80, 81, 145, 81, 300};

static void timing_loop(void)
{
	while(1) {
		vTaskDelay(5000*portTICK_RATE_MS);
		performance_start();
		handle_buffer(testdata, sizeof(testdata)/sizeof(testdata[0]));
		performance_set_checkpoint("end of first buffer");
		handle_buffer(testdata2, sizeof(testdata2)/sizeof(testdata2[0]));
		performance_stop_report();
		DumpStringToUSB("Produced frame of "); DumpUIntToUSB(rx_frame.numbytes);
		DumpStringToUSB(" bytes and "); DumpUIntToUSB(rx_frame.numbits);
		DumpStringToUSB(" bits: "); DumpBufferToUSB((char*)rx_frame.data, rx_frame.numbytes + (rx_frame.numbits+7)/8 );
		DumpStringToUSB(" CRC "); if(rx_frame.parameters.a.crc) DumpStringToUSB("OK"); else DumpStringToUSB("ERROR");
		DumpStringToUSB("\n\r");
	}
}

void tc_sniffer (void *pvParameters)
{
	(void)pvParameters;
	/* Disable load modulation circuitry
	 * (Must be done explicitly, because the default state is pull-up high, leading
	 * to a constant modulation output which prevents reception. I've been bitten by
	 * this more than once.)
	 */
	load_mod_init();
	load_mod_level(0);
	
	clock_switch_init();
	
	pll_init();
	pll_inhibit(0);
	
	tc_fdt_init();
	
	memset(fiq_buffers, 0, sizeof(fiq_buffers));
	
	/* Wait for the USB and CMD threads to start up */
	vTaskDelay(1000 * portTICK_RATE_MS);
	
	if(OPENPICC->features.clock_switching) {
		clock_switch(CLOCK_SELECT_CARRIER);
		decoder = iso14443a_init_diffmiller(0);
	} else {
		switch(OPENPICC->default_clock) {
		case CLOCK_SELECT_CARRIER:
			decoder = iso14443a_init_diffmiller(0);
			break;
		case CLOCK_SELECT_PLL:
			decoder = iso14443a_init_diffmiller(1);
			break;
		}
	}
	
	if(!decoder) vLedHaltBlinking(1);
	vSemaphoreCreateBinary(data_semaphore);
	if(data_semaphore == NULL) vLedHaltBlinking(3);
	
	// The change interrupt is going to be handled by the FIQ 
	AT91F_PIO_CfgInput(AT91C_BASE_PIOA, OPENPICC_SSC_DATA);
	if( pio_irq_register(OPENPICC_SSC_DATA, &tc_sniffer_irq) < 0) 
		vLedHaltBlinking(2);
	pio_irq_enable(OPENPICC_SSC_DATA);

	//main_loop();
	timing_loop();
	
	(void)main_loop; (void)timing_loop;
}
