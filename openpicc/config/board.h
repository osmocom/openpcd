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

#define OPENPICC_MOD_PWM	   AT91C_PA23_PWM0
#define OPENPICC_MOD_SSC	   AT91C_PA17_TD
#define OPENPICC_SSC_DATA	   AT91C_PA18_RD
#define OPENPICC_SSC_CLOCK	   AT91C_PA19_RK

#define OPENPICC_PIO_FRAME         AT91C_PIO_PA20
#define OPENPICC_PIO_SSC_DATA_CONTROL   AT91C_PIO_PA21
#define OPENPICC_PIO_AB_DETECT          AT91C_PIO_PA22
#define OPENPICC_PIO_PLL_INHIBIT        AT91C_PIO_PA24

#define OPENPICC_ADC_FIELD_STRENGTH    AT91C_ADC_CH4

#define OPENPICC_PIO_CARRIER_IN          AT91C_PA28_TCLK1
#define OPENPICC_PIO_CARRIER_DIV_OUT     AT91C_PA1_TIOB0
#define OPENPICC_PIO_CDIV_HELP_OUT       AT91C_PA0_TIOA0
#define OPENPICC_PIO_CDIV_HELP_IN        AT91C_PA29_TCLK2

#define OPENPICC_IRQ_PRIO_PIO   (AT91C_AIC_PRIOR_LOWEST+4)
#define OPENPICC_IRQ_PRIO_SSC    (AT91C_AIC_PRIOR_HIGHEST-1)
#define OPENPCD_IRQ_PRIO_TC_FDT (AT91C_AIC_PRIOR_LOWEST+3)

/*-----------------*/
/* task priorities */
/*-----------------*/

#define TASK_BEACON_PRIORITY	( tskIDLE_PRIORITY )
#define TASK_BEACON_STACK	( 512 )

#define TASK_CMD_PRIORITY	( tskIDLE_PRIORITY + 1 )
#define TASK_CMD_STACK		( 512 )

#define TASK_USB_PRIORITY	( tskIDLE_PRIORITY + 2 )
#define TASK_USB_STACK		( 512 )

#define TASK_ISO_PRIORITY	( tskIDLE_PRIORITY + 3 )
#define TASK_ISO_STACK		( 512 )

#define TASK_NRF_PRIORITY	( tskIDLE_PRIORITY + 3 )
#define TASK_NRF_STACK		( 512 )

#endif /* Board_h */
