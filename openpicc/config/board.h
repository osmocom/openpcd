/***************************************************************
 *
 * OpenBeacon.org - board specific configuration
 *
 * Copyright 2007 Milosch Meriac <meriac@openbeacon.de>
 *
 * change this file to reflect hardware design changes
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
#ifndef Board_h
#define Board_h

#include "lib_AT91SAM7.h"

#define RAMFUNC __attribute__ ((long_call, section (".ramfunc")))
#define IRQFUNC __attribute__ ((interrupt("IRQ")))
#define FIQFUNC __attribute__ ((interrupt("FIQ")))

#define true	-1
#define false	0

/*-------------------------------*/
/* SAM7Board Memories Definition */
/*-------------------------------*/

#define  ENVIRONMENT_SIZE	1024
#define  FLASH_PAGE_NB		AT91C_IFLASH_NB_OF_PAGES-(ENVIRONMENT_SIZE/AT91C_IFLASH_PAGE_SIZE)

/*-----------------*/
/* Master Clock    */
/*-----------------*/

#define EXT_OC		18432000	// Exetrnal ocilator MAINCK
#define MCK		47923200	// MCK (PLLRC div by 2)
#define MCKKHz		(MCK/1000)	//

/*-----------------*/
/* Pins            */
/*-----------------*/

#define LED_GREEN	AT91C_PIO_PA25
#define LED_RED		AT91C_PIO_PA12
#define LED_MASK	(LED_GREEN|LED_RED)

#define OPENPICC_PIO_SS2_DT_THRESH AT91C_PIO_PA8
#define OPENPICC_PIO_PLL_INHIBIT   AT91C_PIO_PA24
#define OPENPICC_PIO_PLL_LOCK      AT91C_PIO_PA4

#define OPENPCD_PIO_UDP_CNX     NO_UDP_CNX
#define OPENPCD_PIO_UDP_PUPv4   AT91C_PIO_PA16
#define OPENPCD_PIO_LED1        AT91C_PIO_PA25
#define OPENPCD_PIO_LED2        AT91C_PIO_PA12
#define PIO_BOOTLDR             AT91C_PIO_PA6

#define OPENPCD_VENDOR_ID       0x16c0
#define OPENPCD_PRODUCT_ID      0x076b
#define OPENPICC_PRODUCT_ID     0x076c

#define USB_PRODUCT_ID          OPENPICC_PRODUCT_ID
#define USB_VENDOR_ID   OPENPCD_VENDOR_ID

/*----------------------*/
/* interrupt priorities */
/*----------------------*/

#define OPENPCD_IRQ_PRIO_UDP    (AT91C_AIC_PRIOR_LOWEST+2)

/*-----------------*/
/* task priorities */
/*-----------------*/

#define TASK_BEACON_PRIORITY	( tskIDLE_PRIORITY )
#define TASK_BEACON_STACK	( 512 )

#define TASK_CMD_PRIORITY	( tskIDLE_PRIORITY + 1 )
#define TASK_CMD_STACK		( 512 )

#define TASK_USB_PRIORITY	( tskIDLE_PRIORITY + 2 )
#define TASK_USB_STACK		( 512 )

#define TASK_NRF_PRIORITY	( tskIDLE_PRIORITY + 3 )
#define TASK_NRF_STACK		( 512 )

#endif /* Board_h */
