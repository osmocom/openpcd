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
#include <lib_AT91SAM7.h>
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
#include "tc_cdiv.h"
#include "tc_cdiv_sync.h"
#include "tc_fdt.h"
#include "usb_print.h"
#include "iso14443_layer3a.h"
#include "iso14443_sniffer.h"
#include "iso14443a_pretender.h"
#include "decoder.h"
#include "tc_sniffer.h"
#include "performance.h"

static inline int detect_board(void)
{
	/* OpenPICC board detection logic.
	 * Interesting board differences: PA31 is open on OPENPICC_v0_4 and connected
	 * to PA18 on OPENPICC_v0_4_p1. PA18 is connected to U7 on both and might read
	 * differently depending on the state of U7 (primarily depending on U5 and the
	 * receive circuitry).
	 * Strategy: Enable Pullups, read PA31 and PA18, if both read low then U7 is
	 * switched through and this is an v0.4p1. If PA18 reads low and PA31 reads high
	 * then U7 is switched through and this is an v0.4. If both read high, then U7 is
	 * not switched through and it might be either board. In this case drive PA31 down
	 * and see whether PA18 follows down, then it's a v0.4p1 otherwise a v0.4.
	 */
	int result = -1;
	
	AT91PS_PIO pio = AT91C_BASE_PIOA;
	u_int32_t old_OSR = pio->PIO_OSR, 
		old_ODSR = pio->PIO_ODSR,
		old_PUSR = pio->PIO_PPUSR,
		old_PSR = pio->PIO_PSR;
	
	pio->PIO_ODR   = AT91C_PIO_PA18 | AT91C_PIO_PA31;
	pio->PIO_PER   = AT91C_PIO_PA18 | AT91C_PIO_PA31;
	pio->PIO_PPUER = AT91C_PIO_PA18 | AT91C_PIO_PA31;
	
	unsigned int pa18 = AT91F_PIO_IsInputSet(pio, AT91C_PIO_PA18),
		pa31 = AT91F_PIO_IsInputSet(pio, AT91C_PIO_PA31);
	if(!pa18 && !pa31) {
	    //vLedInit();
		//vLedHaltBlinking(1);
		result = OPENPICC_v0_4_p2;
	} else if(!pa18 && pa31) {
	    vLedInit();
		vLedHaltBlinking(2);
		// Needs to be tested, should be v0.4
	} else if(pa18 && pa31) {
		// Can be either board
		pio->PIO_OER = AT91C_PIO_PA31;
		pio->PIO_CODR = AT91C_PIO_PA31;
		pa18 = AT91F_PIO_IsInputSet(pio, AT91C_PIO_PA18);
		if(!pa18) {
			result = OPENPICC_v0_4_p2;
		} else {
		    vLedInit();
			vLedHaltBlinking(3);
			// Needs to be tested, should be v0.4
		}
		
		// Restore state
		if( old_OSR & AT91C_PIO_PA31 ) {
			pio->PIO_OER = AT91C_PIO_PA31;
			if(old_ODSR & AT91C_PIO_PA31) {
				pio->PIO_SODR = AT91C_PIO_PA31;
			} else {
				pio->PIO_CODR = AT91C_PIO_PA31;
			}
		} else {
			pio->PIO_ODR = AT91C_PIO_PA31;
		}
	}
	
	// Restore state
	if(old_PSR & AT91C_PIO_PA18) pio->PIO_PER = AT91C_PIO_PA18; else pio->PIO_PDR = AT91C_PIO_PA18;
	if(old_PSR & AT91C_PIO_PA31) pio->PIO_PER = AT91C_PIO_PA31; else pio->PIO_PDR = AT91C_PIO_PA31;
	
	if(old_PUSR & AT91C_PIO_PA18) pio->PIO_PPUDR = AT91C_PIO_PA18; else pio->PIO_PPUER = AT91C_PIO_PA18;
	if(old_PUSR & AT91C_PIO_PA31) pio->PIO_PPUDR = AT91C_PIO_PA31; else pio->PIO_PPUER = AT91C_PIO_PA31;
	
	return result;
}

/**********************************************************************/
static inline void prvSetupHardware (void)
{
	/* The very, very first thing we do is setup the global OPENPICC variable to point to
	 * the correct hardware information.
	 */
	int release = detect_board();
	if(release < 0) {
		vLedInit();
		vLedHaltBlinking(0);
	}
	OPENPICC = &OPENPICC_HARDWARE[release];
	
	
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

void usb_print_flusher (void *pvParameters)
{
	(void)pvParameters;
	while(1) {
		usb_print_flush();
		vTaskDelay(100*portTICK_RATE_MS);
	}
}

/**********************************************************************/
int main (void)
{
    prvSetupHardware ();
    usb_print_init();
    decoder_init();
    performance_init();
    
    pio_irq_init();
    
    vLedInit();
    
    da_init();
    adc_init();
    
    xTaskCreate (usb_print_flusher, (signed portCHAR *) "PRINT-FLUSH", TASK_USB_STACK,
	NULL, TASK_USB_PRIORITY, NULL);
    /*xTaskCreate (iso14443_layer3a_state_machine, (signed portCHAR *) "ISO14443A-3", TASK_ISO_STACK,
	NULL, TASK_ISO_PRIORITY, NULL);*/
    xTaskCreate (iso14443_sniffer, (signed portCHAR *) "ISO14443-SNIFF", TASK_ISO_STACK,
	NULL, TASK_ISO_PRIORITY, NULL);
    /*xTaskCreate (iso14443a_pretender, (signed portCHAR *) "ISO14443A-PRETEND", TASK_ISO_STACK,
	NULL, TASK_ISO_PRIORITY, NULL);*/
    /*xTaskCreate (tc_sniffer, (signed portCHAR *) "RFID-SNIFFER", TASK_ISO_STACK,
		 	NULL, TASK_ISO_PRIORITY, NULL);*/

	    
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
