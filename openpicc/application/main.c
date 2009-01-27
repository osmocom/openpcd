/***************************************************************
 *
 * OpenPICC.org - Main file for RFID sniffer-only firmware 
 *
 * Copyright 2007 Milosch Meriac <meriac@openbeacon.de>
 * Copyright 2008 Henryk Plötz <henryk@ploetzli.ch>
 *
 * basically starts the USB task, initializes all IO ports
 * and starts the sniffer task
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
#include "cmd.h"
#include "da.h"
#include "adc.h"
#include "pll.h"
#include "pio_irq.h"
#include "tc_cdiv.h"
#include "tc_cdiv_sync.h"
#include "tc_fdt.h"
#include "usb_print.h"
#include "tc_sniffer.h"
#include "ssc.h"

/**********************************************************************/
static inline void prvSetupHardware (void)
{
	/* The very, very first thing we do is setup the global OPENPICC variable to point to
	 * the correct hardware information.
	 * FIXME: Detect dynamically in the future
	 */
	OPENPICC = &OPENPICC_HARDWARE[OPENPICC_v0_4];
	
	
    /*	When using the JTAG debugger the hardware is not always initialised to
	the correct default state.  This line just ensures that this does not
	cause all interrupts to be masked at the start. */
    AT91C_BASE_AIC->AIC_EOICR = 0;

    /*	Enable the peripheral clock. */
    AT91C_BASE_PMC->PMC_PCER = 1 << AT91C_ID_PIOA;
    AT91C_BASE_PMC->PMC_PCER = 1 << AT91C_ID_PIOB;    

}

/**********************************************************************/
void vApplicationIdleHook(void)
{
    usb_print_flush();
}

/* This task pings the watchdog even when the idle task is not running
 * It should be started with a very high priority and will delay most of the time */
void vMainWatchdogPinger (void *pvParameters)
{
	(void)pvParameters;
    
	while(1) {
		/* Restart watchdog, has been enabled in Cstartup_SAM7.c */
    		AT91F_WDTRestart(AT91C_BASE_WDTC);
    		vTaskDelay(100*portTICK_RATE_MS);
	}
}

/**********************************************************************/
int main (void)
{
    prvSetupHardware ();
    usb_print_init();
    
    pio_irq_init();
    
    vLedInit();
    
    da_init();
    adc_init();
    
    ssc_init();
    
    xTaskCreate (tc_sniffer, (signed portCHAR *) "RFID-SNIFFER", TASK_ISO_STACK,
	NULL, TASK_ISO_PRIORITY, NULL);

    xTaskCreate (vUSBCDCTask, (signed portCHAR *) "USB", TASK_USB_STACK,
	NULL, TASK_USB_PRIORITY, NULL);
	
    vCmdInit();
    
    xTaskCreate (vMainWatchdogPinger, (signed portCHAR *) "WDT PINGER", 64,
	NULL, TASK_WDT_PRIORITY, NULL);
	
    //vLedSetGreen(1);
    
    /* Remap RAM to addr 0 */
    AT91C_BASE_MC->MC_RCR = AT91C_MC_RCB;    

    vTaskStartScheduler ();
    
    return 0;
}
