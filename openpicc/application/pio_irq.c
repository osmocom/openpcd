/* PIO IRQ Implementation for OpenPCD
 * (C) 2006 by Harald Welte <hwelte@hmw-consulting.de>
 * (C) 2007 Henryk Plötz <henryk@ploetzli.ch>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by 
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <errno.h>
#include <sys/types.h>
#include <lib_AT91SAM7.h>
#include <string.h>
#include "pio_irq.h"
#include "dbgu.h"
//#include <os/req_ctx.h>
#include "openpicc.h"
#include "board.h"
#include "led.h"

#include <FreeRTOS.h>
#include <task.h>

struct pioirq_state {
	irq_handler_t *handlers[NR_PIO];
	u_int32_t usbmask;
	u_int32_t usb_throttled; /* atomic? */
};

static struct pioirq_state pirqs;
static unsigned long count = 0;

/* This FIQ implementation of pio data change works in close cooperation with function fiq_handler  
 * in os/boot/boot.s
 * This code uses fast forcing for the PIOA irq so that each PIOA data change triggers
 * a FIQ. The FreeRTOS code has been modified to not mask FIQ ever. This means that the FIQ
 * code will run asynchronously with regards to the other code and especially might invade critical
 * sections. The actual FIQ code must therefore be as short as possible and may not call into the
 * FreeRTOS API (or parts of the application that call into the FreeRTOS API).
 * Instead a trick will be attempted: The PIOA IRQ will be set to fast forcing with my_fiq_handler
 * as handler and the FIQ handler then does the absolutely time critical tasks without calling any
 * other code. Additionally a second, normal IRQ handler is set up for a reserved IRQ on the AIC 
 * that is not connected to any peripherals (see #define of PIO_SECONDARY_IRQ). After handling
 * the time critical tasks the FIQ handler will then manually assert this IRQ which will then
 * be handled by the AIC and priority controller and also execute synchronized with regards to 
 * critical sections. 
 * Potential problem: look for race conditions between PIO data change FIQ and handling 
 * of PIO_SECONDARY_IRQ.
 * 
 * Note: Originally I wanted to use 15 for the PIO_SECONDARY_IRQ but it turns out that that
 * won't work (the identifier is marked as reserved). Use 31 (external IRQ1) instead. Another
 * candidate would be 7 (USART1).
 */
#define USE_FIQ
#define PIO_SECONDARY_IRQ 31
extern void fiq_handler(void);

/* Will be used in pio_irq_demux_secondary below and contains the PIO_ISR value 
 * from when the FIQ was raised */
volatile u_int32_t pio_irq_isr_value; 


/* low-level handler, used by Cstartup_app.S PIOA fast forcing and
 * by regular interrupt handler below */
void __ramfunc __pio_irq_demux(u_int32_t pio)
{
	u_int8_t send_usb = 0;
	int i;
	count++;

	DEBUGPCRF("PIO_ISR_STATUS = 0x%08x", pio);

	for (i = 0; i < NR_PIO; i++) {
		if (pio & (1 << i) && pirqs.handlers[i])
			pirqs.handlers[i](i);
		if (pirqs.usbmask & (1 << i))
			send_usb = 1;
	}

	AT91F_AIC_AcknowledgeIt();
	//AT91F_AIC_ClearIt(AT91C_ID_PIOA);
}

/* regular interrupt handler, in case fast forcing for PIOA disabled */
static void pio_irq_demux(void) __attribute__ ((naked));
static void pio_irq_demux(void)
{
	portSAVE_CONTEXT();
	u_int32_t pio = AT91F_PIO_GetInterruptStatus(AT91C_BASE_PIOA);
	__pio_irq_demux(pio);
	portRESTORE_CONTEXT();	
}

/* nearly regular interrupt handler, in case fast forcing for PIOA is enabled and the secondary irq hack used */
static void pio_irq_demux_secondary(void) __attribute__ ((naked));
static void pio_irq_demux_secondary(void)
{
	portSAVE_CONTEXT();
	__pio_irq_demux(pio_irq_isr_value);
	AT91F_AIC_ClearIt(PIO_SECONDARY_IRQ);
	portRESTORE_CONTEXT();	
}

void pio_irq_enable(u_int32_t pio)
{
	AT91F_PIO_InterruptEnable(AT91C_BASE_PIOA, pio);
}

void pio_irq_disable(u_int32_t pio)
{
	AT91F_PIO_InterruptDisable(AT91C_BASE_PIOA, pio);
}

/* Return the number of PIO IRQs received */ 
long pio_irq_get_count(void)
{
	return count;
}

int pio_irq_register(u_int32_t pio, irq_handler_t *handler)
{
	u_int8_t num = ffs(pio);

	if (num == 0)
		return -EINVAL;
	num--;

	if (pirqs.handlers[num])
		return -EBUSY;

	pio_irq_disable(pio);
	AT91F_PIO_CfgInput(AT91C_BASE_PIOA, pio);
	pirqs.handlers[num] = handler;
	DEBUGPCRF("registering handler %p for PIOA %u", handler, num);

	return 0;
}

void pio_irq_unregister(u_int32_t pio)
{
	u_int8_t num = ffs(pio);

	if (num == 0)
		return;
	num--;

	pio_irq_disable(pio);
	pirqs.handlers[num] = NULL;
}

static int initialized = 0;
void pio_irq_init_once(void)
{
	if(!initialized) pio_irq_init();
}


void pio_irq_init(void)
{
	initialized = 1;
	AT91F_PIOA_CfgPMC();
#ifdef USE_FIQ
/*	This code is not necessary anymore, because fiq_handler is directly part of
	the vector table, so no jump will happen. 
        AT91F_AIC_ConfigureIt(AT91C_ID_FIQ,
                              //0, AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL, &cdsync_cb);
                              0, AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL, &fiq_handler);
*/
        /* enable fast forcing for PIOA interrupt */
        *AT91C_AIC_FFER = (1 << AT91C_ID_PIOA);
        
        /* Set up a regular IRQ handler to be triggered from within the FIQ */
	AT91F_AIC_ConfigureIt(PIO_SECONDARY_IRQ,
			      OPENPICC_IRQ_PRIO_PIO,
			      AT91C_AIC_SRCTYPE_INT_POSITIVE_EDGE, &pio_irq_demux_secondary);
        AT91F_AIC_ClearIt(PIO_SECONDARY_IRQ);
	AT91F_AIC_EnableIt(PIO_SECONDARY_IRQ);
#else
	AT91F_AIC_ConfigureIt(AT91C_ID_PIOA,
			      OPENPICC_IRQ_PRIO_PIO,
			      AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL, &pio_irq_demux);
#endif
	AT91F_AIC_EnableIt(AT91C_ID_PIOA);
	(void)pio_irq_demux; // FIXME NO IRQ
}
