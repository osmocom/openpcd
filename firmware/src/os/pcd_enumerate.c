/* AT91SAM7 USB interface code for OpenPCD 
 * (C) 2006 by Harald Welte <laforge@gnumonks.org>
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
 * based on existing AT91SAM7 UDP CDC ACM example code, licensed as followed:
 *----------------------------------------------------------------------------
 *      ATMEL Microcontroller Software Support  -  ROUSSET  -
 *----------------------------------------------------------------------------
 * The software is delivered "AS IS" without warranty or condition of any
 * kind, either express, implied or statutory. This includes without
 * limitation any warranty or condition with respect to merchantability or
 * fitness for any particular purpose, or against the infringements of
 * intellectual property rights of others.
 *----------------------------------------------------------------------------
 */

#include <errno.h>
#include <usb_ch9.h>
#include <sys/types.h>
#include <asm/atomic.h>
#include <lib_AT91SAM7.h>
#include <openpcd.h>

#include <usb_strings_app.h>

#include <os/pcd_enumerate.h>
#include <os/req_ctx.h>
#include <dfu/dfu.h>
#include "../openpcd.h"
#include <os/dbgu.h>

#include "../config.h"

//#define DEBUG_UDP_IRQ
//#define DEBUG_UDP_IRQ_IN
//#define DEBUG_UDP_IRQ_OUT
//#define DEBUG_UDP_EP0

#ifdef DEBUG_UDP_IRQ
#define DEBUGI(x, args ...)	DEBUGP(x, ## args)
#else
#define DEBUGI(x, args ...)	do { } while (0)
#endif

#ifdef DEBUG_UDP_IRQ_IN
#define DEBUGII(x, args ...)	DEBUGP(x, ## args)
#else
#define DEBUGII(x, args ...)	do { } while (0)
#endif

#ifdef DEBUG_UDP_IRQ_OUT
#define DEBUGIO(x, args ...)	DEBUGP(x, ## args)
#else
#define DEBUGIO(x, args ...)	do { } while (0)
#endif

#ifdef DEBUG_UDP_EP0
#define DEBUGE(x, args ...)	DEBUGP(x, ## args)
#else
#define DEBUGE(x, args ...)	do { } while (0)
#endif

#define CONFIG_DFU

#ifdef CONFIG_DFU
static const struct dfuapi *dfu = DFU_API_LOCATION;
#define udp_init		dfu->udp_init
#define udp_ep0_send_data	dfu->ep0_send_data
#define udp_ep0_send_zlp	dfu->ep0_send_zlp
#define udp_ep0_send_stall	dfu->ep0_send_stall
#else
#error non-DFU builds currently not supported (yet) again
#endif

#ifdef CONFIG_USB_HID
#include "usb_descriptors_hid.h"
#else
#include "usb_descriptors_openpcd.h"
#endif

static struct udp_pcd upcd;

struct epstate {
	uint32_t state_busy;
	uint32_t state_pending;
};

static const struct epstate epstate[] = {
	[0] =	{ .state_busy = RCTX_STATE_UDP_EP0_BUSY,
		  .state_pending = RCTX_STATE_UDP_EP0_PENDING },
	[1] =	{ .state_busy = RCTX_STATE_UDP_EP1_BUSY,
		  .state_pending = RCTX_STATE_UDP_EP1_PENDING },
	[2] =	{ .state_busy = RCTX_STATE_UDP_EP2_BUSY,
		  .state_pending = RCTX_STATE_UDP_EP2_PENDING },
	[3] =	{ .state_busy = RCTX_STATE_UDP_EP3_BUSY,
		  .state_pending = RCTX_STATE_UDP_EP3_PENDING },
};

static void reset_ep(unsigned int ep)
{
	AT91PS_UDP pUDP = upcd.pUdp;
	struct req_ctx *rctx;

	atomic_set(&upcd.ep[ep].pkts_in_transit, 0);

	/* free all currently transmitting contexts */
	while ((rctx = req_ctx_find_get(0, epstate[ep].state_busy,
				       RCTX_STATE_FREE))) {}
	/* free all currently pending contexts */
	while ((rctx = req_ctx_find_get(0, epstate[ep].state_pending,
				       RCTX_STATE_FREE))) {}

	pUDP->UDP_RSTEP |= (1 << ep);
	pUDP->UDP_RSTEP &= ~(1 << ep);
	pUDP->UDP_CSR[ep] = AT91C_UDP_EPEDS;

	upcd.ep[ep].incomplete.rctx = NULL;
}

static void udp_ep0_handler(void);

void udp_unthrottle(void)
{
	AT91PS_UDP pUDP = upcd.pUdp;
	pUDP->UDP_IER = AT91C_UDP_EPINT1;
}

int udp_refill_ep(int ep)
{
	uint16_t i;
	AT91PS_UDP pUDP = upcd.pUdp;
	struct req_ctx *rctx;
	unsigned int start, end;

	/* If we're not configured by the host yet, there is no point
	 * in trying to send data to it... */
	if (!upcd.cur_config) {
		return -ENXIO;
	}
	
	/* If there are already two packets in transit, the DPR of
	 * the SAM7 UDC doesn't have space for more data */
	if (atomic_read(&upcd.ep[ep].pkts_in_transit) == 2)
		return -EBUSY;

	/* disable endpoint interrup */
	pUDP->UDP_IDR |= 1 << ep;

	/* If we have an incompletely-transmitted req_ctx (>EP size),
	 * we need to transmit the rest and finish the transaction */
	if (upcd.ep[ep].incomplete.rctx) {
		rctx = upcd.ep[ep].incomplete.rctx;
		start = upcd.ep[ep].incomplete.bytes_sent;
	} else {
		/* get pending rctx and start transmitting from zero */
		rctx = req_ctx_find_get(0, epstate[ep].state_pending, 
					epstate[ep].state_busy);
		if (!rctx) {
			/* re-enable endpoint interrupt */
			pUDP->UDP_IER |= 1 << ep;
			return 0;
		}
		if (rctx->tot_len == 0) {
			/* re-enable endpoint interrupt */
			pUDP->UDP_IER |= 1 << ep;
			req_ctx_put(rctx);
			return 0;
		}
		DEBUGPCR("USBT(D=%08X, L=%04u, P=$02u) H4/T4: %02X %02X %02X %02X / %02X %02X %02X %02X",
			 rctx->data, rctx->tot_len, req_ctx_count(epstate[ep].state_pending),
			 rctx->data[4], rctx->data[5], rctx->data[6], rctx->data[7],
			 rctx->data[rctx->tot_len - 4], rctx->data[rctx->tot_len - 3],
			 rctx->data[rctx->tot_len - 2], rctx->data[rctx->tot_len - 1]);

		start = 0;

		upcd.ep[ep].incomplete.bytes_sent = 0;
	}

	if (rctx->tot_len - start <= AT91C_EP_IN_SIZE)
		end = rctx->tot_len;
	else
		end = start + AT91C_EP_IN_SIZE;

	/* fill FIFO/DPR */
	for (i = start; i < end; i++) 
		pUDP->UDP_FDR[ep] = rctx->data[i];

	if (atomic_inc_return(&upcd.ep[ep].pkts_in_transit) == 1) {
		/* not been transmitting before, start transmit */
		pUDP->UDP_CSR[ep] |= AT91C_UDP_TXPKTRDY;
	}

	if (end == rctx->tot_len) {
		/* CASE 1: return context to pool, if
		 * - packet transfer < AT91C_EP_OUT_SIZE
		 * - after ZLP of transfer == AT91C_EP_OUT_SIZE
		 * - after ZLP of transfer % AT91C_EP_OUT_SIZE == 0
		 * - after last packet of transfer % AT91C_EP_OUT_SIZE != 0
		 */
		upcd.ep[ep].incomplete.rctx = NULL;
		req_ctx_put(rctx);
	} else {
		/* CASE 2: mark transfer as incomplete, if
		 * - after data of transfer == AT91C_EP_OUT_SIZE
		 * - after data of transfer > AT91C_EP_OUT_SIZE
		 * - after last packet of transfer % AT91C_EP_OUT_SIZE == 0
	         */
		upcd.ep[ep].incomplete.rctx = rctx;
		upcd.ep[ep].incomplete.bytes_sent += end - start;
	}

	/* re-enable endpoint interrupt */
	pUDP->UDP_IER |= 1 << ep;

	return 1;
}

static void udp_irq(void)
{
	uint32_t csr;
	AT91PS_UDP pUDP = upcd.pUdp;
	AT91_REG isr = pUDP->UDP_ISR;

	DEBUGI("udp_irq(imr=0x%04x, isr=0x%04x, state=%d): ", 
		pUDP->UDP_IMR, isr, upcd.state);

	if (isr & AT91C_UDP_ENDBUSRES) {
		DEBUGI("ENDBUSRES ");
		pUDP->UDP_ICR = AT91C_UDP_ENDBUSRES;
		pUDP->UDP_IER = AT91C_UDP_EPINT0;
		reset_ep(0);
		reset_ep(1);
		reset_ep(2);
		reset_ep(3);

		/* Enable the function */
		pUDP->UDP_FADDR = AT91C_UDP_FEN;

		/* Configure endpoint 0 */
		pUDP->UDP_CSR[0] = (AT91C_UDP_EPEDS | AT91C_UDP_EPTYPE_CTRL);
		upcd.cur_config = 0;
		upcd.state = USB_STATE_DEFAULT;
		
#ifdef CONFIG_DFU
		if (*dfu->dfu_state == DFU_STATE_appDETACH) {
			DEBUGI("DFU_SWITCH ");
			/* now we need to switch to DFU mode */
			dfu->dfu_switch();
			goto out;
		}
#endif
	}

	if (isr & AT91C_UDP_EPINT0) {
		DEBUGI("EP0INT(Control) ");
		udp_ep0_handler();
	}
	if (isr & AT91C_UDP_EPINT1) {
		uint32_t cur_rcv_bank = upcd.cur_rcv_bank;
		uint16_t i, pkt_size;
		struct req_ctx *rctx;

		csr = pUDP->UDP_CSR[1];
		pkt_size = csr >> 16;

		DEBUGI("EP1INT(Out, CSR=0x%08x) ", csr);
		if (cur_rcv_bank == AT91C_UDP_RX_DATA_BK1)
			DEBUGIO("cur_bank=1 ");
		else if (cur_rcv_bank == AT91C_UDP_RX_DATA_BK0)
			DEBUGIO("cur_bank=0 ");
		else
			DEBUGIO("cur_bank INVALID ");

		if (csr & AT91C_UDP_RX_DATA_BK1)
			DEBUGIO("BANK1 ");
		if (csr & AT91C_UDP_RX_DATA_BK0)
			DEBUGIO("BANK0 ");

		if (!(csr & cur_rcv_bank))
			goto cont_ep2;

		if (upcd.ep[1].incomplete.rctx) {
			DEBUGIO("continue_incompl_RCTX ");
			rctx = upcd.ep[1].incomplete.rctx;
		} else {
			/* allocate new req_ctx  */
			DEBUGIO("alloc_new_RCTX ");
		
			/* whether to get a big or a small req_ctx */
			if (pkt_size >= AT91C_EP_IN_SIZE)
				rctx = req_ctx_find_get(1, RCTX_STATE_FREE,
						 RCTX_STATE_UDP_RCV_BUSY);
			else 
				rctx = req_ctx_find_get(0, RCTX_STATE_FREE,
						 RCTX_STATE_UDP_RCV_BUSY);

			if (!rctx) {
				/* disable interrupts for now */
				pUDP->UDP_IDR = AT91C_UDP_EPINT1;
				DEBUGP("NO_RCTX_AVAIL! ");
				goto cont_ep2;
			}
			rctx->tot_len = 0;
		}
		DEBUGIO("RCTX=%u ", req_ctx_num(rctx));

		if (rctx->size - rctx->tot_len < pkt_size) {
			DEBUGIO("RCTX too small, truncating !!!\n");
			pkt_size = rctx->size - rctx->tot_len;
		}

		for (i = 0; i < pkt_size; i++)
			rctx->data[rctx->tot_len++] = pUDP->UDP_FDR[1];

		pUDP->UDP_CSR[1] &= ~cur_rcv_bank;

		/* toggle current receive bank */
		if (cur_rcv_bank == AT91C_UDP_RX_DATA_BK0)
			cur_rcv_bank = AT91C_UDP_RX_DATA_BK1;
		else
			cur_rcv_bank = AT91C_UDP_RX_DATA_BK0;
		upcd.cur_rcv_bank = cur_rcv_bank;

		DEBUGIO("rctxdump(%s) ", hexdump(rctx->data, rctx->tot_len));

		/* if this is the last packet in transfer, hand rctx up the
		 * stack */
		if (pkt_size < AT91C_EP_IN_SIZE) {
			DEBUGIO("RCTX_rx_done ");
			req_ctx_set_state(rctx, RCTX_STATE_UDP_RCV_DONE);
			upcd.ep[1].incomplete.rctx = NULL;
		} else {
			DEBUGIO("RCTX_rx_cont ");
			upcd.ep[1].incomplete.rctx = rctx;
		}
	}
cont_ep2:
	if (isr & AT91C_UDP_EPINT2) {
		csr = pUDP->UDP_CSR[2];
		DEBUGI("EP2INT(In, CSR=0x%08x) ", csr);
		if (csr & AT91C_UDP_TXCOMP) {
			DEBUGII("ACK_TX_COMP ");
			/* acknowledge TX completion */
			pUDP->UDP_CSR[2] &= ~AT91C_UDP_TXCOMP;
			while (pUDP->UDP_CSR[2] & AT91C_UDP_TXCOMP) ;

			/* if we already have another packet in DPR, send it */
			if (atomic_dec_return(&upcd.ep[2].pkts_in_transit) == 1)
				pUDP->UDP_CSR[2] |= AT91C_UDP_TXPKTRDY;

			udp_refill_ep(2);
		}
	}
	if (isr & AT91C_UDP_EPINT3) {
		csr = pUDP->UDP_CSR[3];
		DEBUGII("EP3INT(Interrupt, CSR=0x%08x) ", csr);
		/* Transmit has completed, re-fill from pending rcts for EP3 */
		if (csr & AT91C_UDP_TXCOMP) {
			pUDP->UDP_CSR[3] &= ~AT91C_UDP_TXCOMP;
			while (pUDP->UDP_CSR[3] & AT91C_UDP_TXCOMP) ;

			/* if we already have another packet in DPR, send it */
			if (atomic_dec_return(&upcd.ep[3].pkts_in_transit) == 1)
				pUDP->UDP_CSR[3] |= AT91C_UDP_TXPKTRDY;

			udp_refill_ep(3);
		}
	}
	if (isr & AT91C_UDP_RXSUSP) {
		pUDP->UDP_ICR = AT91C_UDP_RXSUSP;
		DEBUGI("RXSUSP ");
#ifdef CONFIG_USB_SUSPEND
		upcd.state = USB_STATE_SUSPENDED;
		/* FIXME: implement suspend/resume correctly. This
		 * involves saving the pre-suspend state, and calling back
		 * into the main application program to ask it to power down
		 * all peripherals, switching to slow clock, ... */
#endif
	}
	if (isr & AT91C_UDP_RXRSM) {
		pUDP->UDP_ICR = AT91C_UDP_RXRSM;
		DEBUGI("RXRSM ");
#ifdef CONFIG_USB_SUSPEND
		if (upcd.state == USB_STATE_SUSPENDED)
			upcd.state = USB_STATE_CONFIGURED;
		/* FIXME: implement suspend/resume */
#endif
	}
	if (isr & AT91C_UDP_EXTRSM) {
		pUDP->UDP_ICR = AT91C_UDP_EXTRSM;
		DEBUGI("EXTRSM ");
		/* FIXME: implement suspend/resume */
	}
	if (isr & AT91C_UDP_SOFINT) {
		pUDP->UDP_ICR = AT91C_UDP_SOFINT;
		DEBUGI("SOFINT ");
	}
	if (isr & AT91C_UDP_WAKEUP) {
		pUDP->UDP_ICR = AT91C_UDP_WAKEUP;
		DEBUGI("WAKEUP ");
	}
out:
	DEBUGI("END\r\n");
	AT91F_AIC_ClearIt(AT91C_BASE_AIC, AT91C_ID_UDP);
}

void udp_pullup_on(void)
{
#ifdef PCD
	AT91F_PIO_SetOutput(AT91C_BASE_PIOA, OPENPCD_PIO_UDP_PUP);
#endif
	AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, OPENPCD_PIO_UDP_PUPv4);
}

void udp_pullup_off(void)
{
#ifdef PCD
	AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, OPENPCD_PIO_UDP_PUP);
#endif
	AT91F_PIO_SetOutput(AT91C_BASE_PIOA, OPENPCD_PIO_UDP_PUPv4);
}

/* Open USB Device Port  */
void udp_open(void)
{
	DEBUGPCRF("entering");
	udp_init();
	upcd.pUdp = AT91C_BASE_UDP;
	upcd.cur_config = 0;
	upcd.cur_rcv_bank = AT91C_UDP_RX_DATA_BK0;
	/* This should start with USB_STATE_NOTATTACHED, but we're a pure
	 * bus powered device and thus start with powered */
	upcd.state = USB_STATE_POWERED;

	AT91F_AIC_ConfigureIt(AT91C_BASE_AIC, AT91C_ID_UDP,
			      OPENPCD_IRQ_PRIO_UDP,
			      AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL, &udp_irq);
	AT91F_AIC_EnableIt(AT91C_BASE_AIC, AT91C_ID_UDP);

	/* End-of-Bus-Reset is always enabled */

	/* Set the Pull up resistor */
	udp_pullup_on();
}

void udp_reset(void)
{
	volatile int i;

	udp_pullup_off();
	for (i = 0; i < 0xffff; i++)
		;
	udp_pullup_on();
}

/* Handle requests on the USB Control Endpoint */
static void udp_ep0_handler(void)
{
	AT91PS_UDP pUDP = upcd.pUdp;
	uint8_t bmRequestType, bRequest;
	uint16_t wValue, wIndex, wLength, wStatus;
	uint32_t csr = pUDP->UDP_CSR[0];

	DEBUGE("CSR=0x%04x ", csr);

	if (csr & AT91C_UDP_STALLSENT) {
		DEBUGE("ACK_STALLSENT ");
		pUDP->UDP_CSR[0] = ~AT91C_UDP_STALLSENT;
	}

	if (csr & AT91C_UDP_RX_DATA_BK0) {
		DEBUGE("ACK_BANK0 ");
		pUDP->UDP_CSR[0] &= ~AT91C_UDP_RX_DATA_BK0;
	}

	if (!(csr & AT91C_UDP_RXSETUP)) {
		DEBUGE("no setup packet ");
		return;
	}

	DEBUGE("len=%d ", csr >> 16);
	if (csr >> 16  == 0) {
		DEBUGE("empty packet ");
		return;
	}

	bmRequestType = pUDP->UDP_FDR[0];
	bRequest = pUDP->UDP_FDR[0];
	wValue = (pUDP->UDP_FDR[0] & 0xFF);
	wValue |= (pUDP->UDP_FDR[0] << 8);
	wIndex = (pUDP->UDP_FDR[0] & 0xFF);
	wIndex |= (pUDP->UDP_FDR[0] << 8);
	wLength = (pUDP->UDP_FDR[0] & 0xFF);
	wLength |= (pUDP->UDP_FDR[0] << 8);

	DEBUGE("bmRequestType=0x%2x ", bmRequestType);

	if (bmRequestType & 0x80) {
		DEBUGE("DATA_IN=1 ");
		pUDP->UDP_CSR[0] |= AT91C_UDP_DIR;
		while (!(pUDP->UDP_CSR[0] & AT91C_UDP_DIR)) ;
	}
	pUDP->UDP_CSR[0] &= ~AT91C_UDP_RXSETUP;
	while ((pUDP->UDP_CSR[0] & AT91C_UDP_RXSETUP)) ;

	DEBUGE("dfu_state = %u ", *dfu->dfu_state);
	/* Handle supported standard device request Cf Table 9-3 in USB
	 * speciication Rev 1.1 */
	switch ((bRequest << 8) | bmRequestType) {
		uint8_t desc_type, desc_index;
	case STD_GET_DESCRIPTOR:
		DEBUGE("GET_DESCRIPTOR(wValue=0x%04x, wIndex=0x%04x) ",
			wValue, wIndex);
		desc_type = wValue >> 8;
		desc_index = wValue & 0xff;
		switch (desc_type) {
		case USB_DT_DEVICE:
			/* Return Device Descriptor */
#ifdef CONFIG_DFU
			if (*dfu->dfu_state != DFU_STATE_appIDLE)
				udp_ep0_send_data((const char *) 
						  dfu->dfu_dev_descriptor,
						  dfu->dfu_dev_descriptor->bLength,
						  wLength);
			else
#endif
			udp_ep0_send_data((const char *) &dev_descriptor,
					  sizeof(dev_descriptor), wLength);
			break;
		case USB_DT_CONFIG:
			/* Return Configuration Descriptor */
#ifdef CONFIG_DFU
			if (*dfu->dfu_state != DFU_STATE_appIDLE)
				udp_ep0_send_data((const char *)
						  dfu->dfu_cfg_descriptor,
						  dfu->dfu_cfg_descriptor->ucfg.wTotalLength,
						  wLength);
			else
#endif
			udp_ep0_send_data((const char *) &cfg_descriptor,
					   sizeof(cfg_descriptor), wLength);
			break;
		case USB_DT_STRING:
#ifdef CONFIG_USB_STRING
			/* Return String descriptor */
			if (desc_index > ARRAY_SIZE(usb_strings))
				goto out_stall;
			DEBUGE("bLength=%u, wLength=%u\n", 
				usb_strings[desc_index]->bLength, wLength);
			udp_ep0_send_data((const char *) usb_strings[desc_index],
					  usb_strings[desc_index]->bLength,
					  wLength);
#else
			goto out_stall;
#endif
			break;
		case USB_DT_CS_DEVICE:
			/* Return Function descriptor */
			udp_ep0_send_data((const char *) &dfu->dfu_cfg_descriptor->func_dfu,
					  sizeof(dfu->dfu_cfg_descriptor->func_dfu), wLength);
			break;
		case USB_DT_INTERFACE:
			/* Return Interface descriptor */
			if (desc_index > cfg_descriptor.ucfg.bNumInterfaces)
				goto out_stall;
			switch (desc_index) {
			case 0:
				udp_ep0_send_data((const char *)
						&cfg_descriptor.uif,
						sizeof(cfg_descriptor.uif),
						wLength);
				break;
	#ifdef CONFIG_DFU
			case 1:
				udp_ep0_send_data((const char *)
						&cfg_descriptor.uif_dfu[0],
						sizeof(cfg_descriptor.uif_dfu[0]),
						wLength);
				break;
			case 2:
				udp_ep0_send_data((const char *)
						&cfg_descriptor.uif_dfu[1],
						sizeof(cfg_descriptor.uif_dfu[1]),
						wLength);
				break;
			case 3:
				udp_ep0_send_data((const char *)
						&cfg_descriptor.uif_dfu[2],
						sizeof(cfg_descriptor.uif_dfu[2]),
						wLength);
				break;
	#endif
			default:
				goto out_stall;
				break;
			}
			break;
		default:
			goto out_stall;
			break;
		}
		break;
	case STD_SET_ADDRESS:
		DEBUGE("SET_ADDRESS ");
		if (wValue > 127)
			goto out_stall;
		
		switch (upcd.state) {
		case USB_STATE_DEFAULT:
			udp_ep0_send_zlp();
			if (wValue == 0) {
				/* do nothing */
			} else {
				pUDP->UDP_FADDR = (AT91C_UDP_FEN | wValue);
				pUDP->UDP_GLBSTATE = AT91C_UDP_FADDEN;
				upcd.state = USB_STATE_ADDRESS;
			}
			break;
		case USB_STATE_ADDRESS:
			udp_ep0_send_zlp();
			if (wValue == 0) {
				upcd.state = USB_STATE_DEFAULT;
			} else {
				pUDP->UDP_FADDR = (AT91C_UDP_FEN | wValue);
			}
			break;
		default:
			goto out_stall;
			break;
		}
		break;
	case STD_SET_CONFIGURATION:
		DEBUGE("SET_CONFIG ");
		if (upcd.state != USB_STATE_ADDRESS &&
		    upcd.state != USB_STATE_CONFIGURED) {
		    	goto out_stall;
		}
		if ((wValue & 0xff) == 0) {
			DEBUGE("VALUE==0 ");
			upcd.state = USB_STATE_ADDRESS;
			pUDP->UDP_GLBSTATE = AT91C_UDP_FADDEN;
			pUDP->UDP_CSR[1] = 0;
			pUDP->UDP_CSR[2] = 0;
			pUDP->UDP_CSR[3] = 0;
		} else if ((wValue & 0xff) <=
					dev_descriptor.bNumConfigurations) {
			DEBUGE("VALUE!=0 ");
			upcd.state = USB_STATE_CONFIGURED;
			pUDP->UDP_GLBSTATE = AT91C_UDP_CONFG;
			pUDP->UDP_CSR[1] = AT91C_UDP_EPEDS |
					   AT91C_UDP_EPTYPE_BULK_OUT;
			pUDP->UDP_CSR[2] = AT91C_UDP_EPEDS |
					   AT91C_UDP_EPTYPE_BULK_IN;
			pUDP->UDP_CSR[3] = AT91C_UDP_EPEDS |
					   AT91C_UDP_EPTYPE_INT_IN;
		} else {
			/* invalid configuration */
			goto out_stall;
			break;
		}
		upcd.cur_config = wValue;
		udp_ep0_send_zlp();
		pUDP->UDP_IER = (AT91C_UDP_EPINT0 | AT91C_UDP_EPINT1 |
				 AT91C_UDP_EPINT2 | AT91C_UDP_EPINT3);
		break;
	case STD_GET_CONFIGURATION:
		DEBUGE("GET_CONFIG ");
		switch (upcd.state) {
		case USB_STATE_ADDRESS:
		case USB_STATE_CONFIGURED:
			/* Table 9.4 wLength One */
			udp_ep0_send_data((char *)&(upcd.cur_config),
					   sizeof(upcd.cur_config), 1);
			break;
		default:
			goto out_stall;
			break;
		}
		break;
	case STD_GET_INTERFACE:
		DEBUGE("GET_INTERFACE ");
		if (upcd.state != USB_STATE_CONFIGURED)
			goto out_stall;
		/* Table 9.4 wLength One */
		udp_ep0_send_data((char *)&(upcd.cur_altsett),
				  sizeof(upcd.cur_altsett), 1);
		break;
	case STD_GET_STATUS_ZERO:
		DEBUGE("GET_STATUS_ZERO ");
		wStatus = 0;
		/* Table 9.4 wLength Two */
		udp_ep0_send_data((char *)&wStatus, sizeof(wStatus), 2);
		break;
	case STD_GET_STATUS_INTERFACE:
		DEBUGE("GET_STATUS_INTERFACE ");
		if (upcd.state == USB_STATE_DEFAULT ||
		    (upcd.state == USB_STATE_ADDRESS && wIndex != 0))
			goto out_stall;
		wStatus = 0;
		/* Table 9.4 wLength Two */
		udp_ep0_send_data((char *)&wStatus, sizeof(wStatus), 2);
		break;
	case STD_GET_STATUS_ENDPOINT:
		DEBUGE("GET_STATUS_ENDPOINT(EPidx=%u) ", wIndex&0x0f);
		if (upcd.state == USB_STATE_DEFAULT ||
		    (upcd.state == USB_STATE_ADDRESS && wIndex != 0))
			goto out_stall;
		wStatus = 0;
		wIndex &= 0x0F;
		if ((pUDP->UDP_GLBSTATE & AT91C_UDP_CONFG) && (wIndex <= 3)) {
			wStatus =
			    (pUDP->UDP_CSR[wIndex] & AT91C_UDP_EPEDS) ? 0 : 1;
			/* Table 9.4 wLength Two */
			udp_ep0_send_data((char *)&wStatus, sizeof(wStatus), 2);
		} else if ((pUDP->UDP_GLBSTATE & AT91C_UDP_FADDEN)
			   && (wIndex == 0)) {
			wStatus =
			    (pUDP->UDP_CSR[wIndex] & AT91C_UDP_EPEDS) ? 0 : 1;
			/* Table 9.4 wLength Two */
			udp_ep0_send_data((char *)&wStatus, sizeof(wStatus), 2);
		} else
			goto out_stall;
		break;
	case STD_SET_FEATURE_ZERO:
		DEBUGE("SET_FEATURE_ZERO ");
		if (upcd.state == USB_STATE_ADDRESS &&
		    (wIndex & 0xff) != 0)
			goto out_stall;
		/* FIXME: implement this */
		goto out_stall;
		break;
	case STD_SET_FEATURE_INTERFACE:
		DEBUGE("SET_FEATURE_INTERFACE ");
		if (upcd.state == USB_STATE_ADDRESS &&
		    (wIndex & 0xff) != 0)
			goto out_stall;
		udp_ep0_send_zlp();
		break;
	case STD_SET_FEATURE_ENDPOINT:
		DEBUGE("SET_FEATURE_ENDPOINT ");
		if (upcd.state == USB_STATE_ADDRESS &&
		    (wIndex & 0xff) != 0)
			goto out_stall;
		if (wValue != USB_ENDPOINT_HALT)
			goto out_stall;
		udp_ep0_send_zlp();
		wIndex &= 0x0F;
		if ((wValue == 0) && wIndex && (wIndex <= 3)) {
			pUDP->UDP_CSR[wIndex] = 0;
			udp_ep0_send_zlp();
		} else
			goto out_stall;
		break;
	case STD_CLEAR_FEATURE_ZERO:
		DEBUGE("CLEAR_FEATURE_ZERO ");
		goto out_stall;
		break;
	case STD_CLEAR_FEATURE_INTERFACE:
		DEBUGP("CLEAR_FEATURE_INTERFACE ");
		udp_ep0_send_zlp();
		break;
	case STD_CLEAR_FEATURE_ENDPOINT:
		DEBUGE("CLEAR_FEATURE_ENDPOINT(EPidx=%u) ", wIndex & 0x0f);
		if (wValue != USB_ENDPOINT_HALT)
			goto out_stall;
		wIndex &= 0x0F;
		if ((wValue == 0) && wIndex && (wIndex <= 3)) {
			reset_ep(wIndex);
			udp_ep0_send_zlp();
		} else
			goto out_stall;
		break;
	case STD_SET_INTERFACE:
		DEBUGE("SET INTERFACE ");
		if (upcd.state != USB_STATE_CONFIGURED)
			goto out_stall;
		if (wIndex > cfg_descriptor.ucfg.bNumInterfaces)
			goto out_stall;
		upcd.cur_interface = wIndex;
		upcd.cur_altsett = wValue;
		/* USB spec mandates that if we only support one altsetting in
		 * the given interface, we shall respond with STALL in the
		 * status stage */
		udp_ep0_send_stall();
		break;
	default:
		DEBUGE("DEFAULT(req=0x%02x, type=0x%02x) ", 
			bRequest, bmRequestType);
#ifdef CONFIG_DFU
		if ((bmRequestType & 0x3f) == USB_TYPE_DFU) {
			dfu->dfu_ep0_handler(bmRequestType, bRequest, wValue,
					     wLength);
		} else
#endif
		goto out_stall;
		break;
	}
	return;
out_stall:
	DEBUGE("STALL!! ");
	udp_ep0_send_stall();
}

