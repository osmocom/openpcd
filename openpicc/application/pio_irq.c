/* PIO IRQ Implementation for OpenPCD
 * (C) 2006 by Harald Welte <hwelte@hmw-consulting.de>
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

//	if (send_usb && !pirqs.usb_throttled) {
//		struct req_ctx *irq_rctx;
//		irq_rctx = req_ctx_find_get(0, RCTX_STATE_FREE,
//					    RCTX_STATE_PIOIRQ_BUSY);
//		if (!irq_rctx) {
//			/* we cannot disable the interrupt, since we have
//			 * non-usb listeners */
//			pirqs.usb_throttled = 1;
//		} else {
//			struct openpcd_hdr *opcdh;
//			u_int32_t *regmask;
//			opcdh = (struct openpcd_hdr *) irq_rctx->data;
//			regmask = (u_int32_t *) (irq_rctx->data + sizeof(*opcdh));
//			opcdh->cmd = OPENPCD_CMD_PIO_IRQ;
//			opcdh->reg = 0x00;
//			opcdh->flags = 0x00;
//			opcdh->val = 0x00;
//
//			irq_rctx->tot_len = sizeof(*opcdh) + sizeof(u_int32_t);
//			req_ctx_set_state(irq_rctx, RCTX_STATE_UDP_EP3_PENDING);
//		}
//	}

	AT91F_AIC_AcknowledgeIt();
	//AT91F_AIC_ClearIt(AT91C_ID_PIOA);
}

/* regular interrupt handler, in case fast forcing for PIOA disabled */
static void pio_irq_demux(void)
{
	portSAVE_CONTEXT();
	u_int32_t pio = AT91F_PIO_GetInterruptStatus(AT91C_BASE_PIOA);
	__pio_irq_demux(pio);
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

void pio_irq_init(void)
{
	AT91F_PIOA_CfgPMC();
	AT91F_AIC_ConfigureIt(AT91C_ID_PIOA,
			      OPENPICC_IRQ_PRIO_PIO,
			      AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL, &pio_irq_demux);
	AT91F_AIC_EnableIt(AT91C_ID_PIOA);
}
