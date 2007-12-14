/***************************************************************
 *
 * OpenBeacon.org - main entry for 2.4GHz RFID USB reader
 *
 * Copyright 2007 Milosch Meriac <meriac@openbeacon.de>
 *
 * basically starts the USB task, initializes all IO ports
 * and introduces idle application hook to handle the HF traffic
 * from the nRF24L01 chip
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
/* Library includes. */
#include <string.h>
#include <stdio.h>

#include <FreeRTOS.h>
#include <AT91SAM7.h>
#include <USB-CDC.h>
#include <task.h>

#include "openpicc.h"
#include "board.h"
#include "led.h"
#include "env.h"
#include "cmd.h"
#include "da.h"
#include "adc.h"
#include "pll.h"
#include "pio_irq.h"
#include "ssc_picc.h"
#include "tc_cdiv.h"
#include "tc_cdiv_sync.h"
#include "tc_fdt.h"
#include "usb_print.h"
#include "iso14443_layer3a.h"
#include "decoder.h"

/**********************************************************************/
static inline void prvSetupHardware (void)
{
    /*	When using the JTAG debugger the hardware is not always initialised to
	the correct default state.  This line just ensures that this does not
	cause all interrupts to be masked at the start. */
    AT91C_BASE_AIC->AIC_EOICR = 0;

    /*	Enable the peripheral clock. */
    AT91C_BASE_PMC->PMC_PCER = 1 << AT91C_ID_PIOA;
    AT91C_BASE_PMC->PMC_PCER = 1 << AT91C_ID_PIOB;    

    /* initialize environment variables */
    env_init();
    if(!env_load())
    {
	env.e.mode=0;
	env.e.reader_id=255;
	env_store();
    }
}

/**********************************************************************/
void vApplicationIdleHook(void)
{
    static char disabled_green = 0;
    //static int i=0;
    //vLedSetGreen(i^=1);
    if(!disabled_green) {
    	//vLedSetGreen(0);
    	disabled_green = 1;
    }
    usb_print_flush();
}

void main_help_print_buffer(ssc_dma_rx_buffer_t *buffer, int *pktcount)
{
	ISO14443A_SHORT_TYPE *tmp = (ISO14443A_SHORT_TYPE*)buffer->data;
	int i, dumped = 0;
	unsigned int j;
	for(i = buffer->len / (sizeof(*tmp)*8); i >= 0 ; i--) {
		if( *tmp != 0x00000000 ) {
			if(dumped == 0) {
				DumpUIntToUSB(buffer->len);
				DumpStringToUSB(", ");
				DumpUIntToUSB((*pktcount)++);
				DumpStringToUSB(": ");
			} else {
				DumpStringToUSB(" ");
			}
			dumped = 1;
			DumpUIntToUSB(buffer->len / (sizeof(*tmp)*8) - i);
			DumpStringToUSB(": ");
			for(j=0; j<sizeof(*tmp)*8; j++) {
				usb_print_char_f( (((*tmp) >> j) & 0x1) ? '1' : '_' , 0);
			}
			usb_print_flush();
			//DumpBufferToUSB((char*)(tmp), sizeof(*tmp));
		}
		tmp++;
	}
	if(dumped) DumpStringToUSB("\n\r");
}

void vMainTestSSCRXConsumer (void *pvParameters)
{
	static int pktcount=0;
	(void)pvParameters;
	while(1) {
		ssc_dma_rx_buffer_t* buffer;
		if(xQueueReceive(ssc_rx_queue, &buffer, portMAX_DELAY)) {
			portENTER_CRITICAL();
			buffer->state = PROCESSING;
			portEXIT_CRITICAL();
			/*vLedBlinkGreen();
			for(i=0; i<buffer->len*8; i++) {
				vLedSetGreen( buffer->data[i/8] & (1<<(i%8)) );
			}
			vLedBlinkGreen();*/
			//i = usb_print_set_default_flush(0);
			
			main_help_print_buffer(buffer, &pktcount);
			
			//usb_print_set_default_flush(i);
			portENTER_CRITICAL();
			buffer->state = FREE;
			portEXIT_CRITICAL();
		}
	}
}

/* This task pings the watchdog even when the idle task is not running
 * It should be started with a very high priority and will delay most of the time */
void vMainWatchdogPinger (void *pvParameters)
{
	(void)pvParameters;
    
	while(1) {
		/* Restart watchdog, has been enabled in Cstartup_SAM7.c */
    		AT91F_WDTRestart(AT91C_BASE_WDTC);
    		vTaskDelay(500*portTICK_RATE_MS);
	}
}

/**********************************************************************/
int main (void)
{
    prvSetupHardware ();
    usb_print_init();
    decoder_init();
    
    pio_irq_init();
    
    vLedInit();
    
    da_init();
    adc_init();
    
    /*xTaskCreate (vMainTestSSCRXConsumer, (signed portCHAR *) "SSC_CONSUMER", TASK_USB_STACK,
	NULL, TASK_USB_PRIORITY, NULL);*/
    xTaskCreate (iso14443_layer3a_state_machine, (signed portCHAR *) "ISO14443A-3", TASK_ISO_STACK,
	NULL, TASK_ISO_PRIORITY, NULL);

    xTaskCreate (vUSBCDCTask, (signed portCHAR *) "USB", TASK_USB_STACK,
	NULL, TASK_USB_PRIORITY, NULL);
	
    vCmdInit();
    
    xTaskCreate (vMainWatchdogPinger, (signed portCHAR *) "WDT PINGER", 64,
	NULL, TASK_ISO_PRIORITY -1, NULL);
	
    //vLedSetGreen(1);
    
    /* Remap RAM to addr 0 */
    AT91C_BASE_MC->MC_RCR = AT91C_MC_RCB;    

    vTaskStartScheduler ();
    
    return 0;
}
