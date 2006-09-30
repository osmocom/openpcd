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

//#define DEBUG_UDP_IRQ
//#define DEBUG_UDP_IRQ_IN
//#define DEBUG_UDP_IRQ_OUT
#define DEBUG_UDP_EP0

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

static struct udp_pcd upcd;

const struct usb_device_descriptor dev_descriptor = {
	.bLength = USB_DT_DEVICE_SIZE,
	.bDescriptorType = USB_DT_DEVICE,
	.bcdUSB = 0x0200,
	.bDeviceClass = USB_CLASS_VENDOR_SPEC,
	.bDeviceSubClass = 0xff,
	.bDeviceProtocol = 0xff,
	.bMaxPacketSize0 = 0x08,
	.idVendor = USB_VENDOR_ID,
	.idProduct = USB_PRODUCT_ID,
	.bcdDevice = 0x0000,
	.iManufacturer = 3,
	.iProduct = 4,
	.iSerialNumber = 0x00,
	.bNumConfigurations = 0x01,
};

struct _desc {
	struct usb_config_descriptor ucfg;
	struct usb_interface_descriptor uif;
	struct usb_endpoint_descriptor ep[3];
#ifdef CONFIG_DFU
	struct usb_interface_descriptor uif_dfu[2];
#endif
};

const struct _desc cfg_descriptor = {
	.ucfg = {
		 .bLength = USB_DT_CONFIG_SIZE,
		 .bDescriptorType = USB_DT_CONFIG,
		 .wTotalLength = USB_DT_CONFIG_SIZE +
#ifdef CONFIG_DFU
		 		 3 * USB_DT_INTERFACE_SIZE + 
				 3 * USB_DT_ENDPOINT_SIZE,
		 .bNumInterfaces = 3,
#else
		 		 1 * USB_DT_INTERFACE_SIZE + 
				 3 * USB_DT_ENDPOINT_SIZE,
		 .bNumInterfaces = 1,
#endif
		 .bConfigurationValue = 1,
		 .iConfiguration = 5,
		 .bmAttributes = USB_CONFIG_ATT_ONE,
		 .bMaxPower = 250,	/* 500mA */
		 },
	.uif = {
		.bLength = USB_DT_INTERFACE_SIZE,
		.bDescriptorType = USB_DT_INTERFACE,
		.bInterfaceNumber = 0,
		.bAlternateSetting = 0,
		.bNumEndpoints = 3,
		.bInterfaceClass = USB_CLASS_VENDOR_SPEC,
		.bInterfaceSubClass = 0,
		.bInterfaceProtocol = 0xff,
		.iInterface = 6,
		},
	.ep= {
		{
			  .bLength = USB_DT_ENDPOINT_SIZE,
			  .bDescriptorType = USB_DT_ENDPOINT,
			  .bEndpointAddress = OPENPCD_OUT_EP,
			  .bmAttributes = USB_ENDPOINT_XFER_BULK,
			  .wMaxPacketSize = AT91C_EP_OUT_SIZE,
			  .bInterval = 0x00,
		  }, {
			  .bLength = USB_DT_ENDPOINT_SIZE,
			  .bDescriptorType = USB_DT_ENDPOINT,
			  .bEndpointAddress = OPENPCD_IN_EP,
			  .bmAttributes = USB_ENDPOINT_XFER_BULK,
			  .wMaxPacketSize = AT91C_EP_IN_SIZE,
			  .bInterval = 0x00,
		  }, {
			  .bLength = USB_DT_ENDPOINT_SIZE,
			  .bDescriptorType = USB_DT_ENDPOINT,
			  .bEndpointAddress = OPENPCD_IRQ_EP,
			  .bmAttributes = USB_ENDPOINT_XFER_INT,
			  .wMaxPacketSize = AT91C_EP_IN_SIZE,
			  .bInterval = 0xff,	/* FIXME */
		  },
	},
#ifdef CONFIG_DFU
	.uif_dfu = DFU_RT_IF_DESC,
#endif
};

struct epstate {
	u_int32_t state_busy;
	u_int32_t state_pending;
};

static const struct epstate epstate[] = {
	[0] =	{ .state_busy = RCTX_STATE_INVALID },
	[1] =	{ .state_busy = RCTX_STATE_INVALID },
	[2] =	{ .state_busy = RCTX_STATE_UDP_EP2_BUSY,
		  .state_pending = RCTX_STATE_UDP_EP2_PENDING },
	[3] =	{ .state_busy = RCTX_STATE_UDP_EP3_BUSY,
		  .state_pending = RCTX_STATE_UDP_EP3_PENDING },
};

static void reset_ep(unsigned int ep)
{
	AT91PS_UDP pUDP = upcd.pUdp;
	struct req_ctx *rctx;

	//pUDP->UDP_CSR[ep] = AT91C_UDP_EPEDS;

	atomic_set(&upcd.ep[ep].pkts_in_transit, 0);

	/* free all currently transmitting contexts */
	while ((rctx = req_ctx_find_get(0, epstate[ep].state_busy,
				       RCTX_STATE_FREE))) {}
	/* free all currently pending contexts */
	while ((rctx = req_ctx_find_get(0, epstate[ep].state_pending,
				       RCTX_STATE_FREE))) {}

	pUDP->UDP_RSTEP |= (1 << ep);
	pUDP->UDP_RSTEP &= ~(1 << ep);

	upcd.ep[ep].incomplete.rctx = NULL;
}

static void udp_ep0_handler(void);

void udp_unthrottle(void)
{
	AT91PS_UDP pUDP = upcd.pUdp;
	pUDP->UDP_IER = AT91C_UDP_EPINT1;
}

static int __ramfunc __udp_refill_ep(int ep)
{
	u_int16_t i;
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
	if (atomic_read(&upcd.ep[ep].pkts_in_transit) == 2) {
		return -EBUSY;
	}

	/* If we have an incompletely-transmitted req_ctx (>EP size),
	 * we need to transmit the rest and finish the transaction */
	if (upcd.ep[ep].incomplete.rctx) {
		rctx = upcd.ep[ep].incomplete.rctx;
		start = upcd.ep[ep].incomplete.bytes_sent;
	} else {
		/* get pending rctx and start transmitting from zero */
		rctx = req_ctx_find_get(0, epstate[ep].state_pending, 
					epstate[ep].state_busy);
		if (!rctx)
			return 0;
		start = 0;

		upcd.ep[ep].incomplete.bytes_sent = 0;
	}

	if (rctx->tot_len - start <= AT91C_EP_IN_SIZE)
		end = rctx->tot_len;
	else
		end = start + AT91C_EP_IN_SIZE;

	/* fill FIFO/DPR */
	DEBUGII("RCTX_tx(ep=%u,ctx=%u):%u ", ep, req_ctx_num(rctx),
		end - start);
	for (i = start; i < end; i++) 
		pUDP->UDP_FDR[ep] = rctx->data[i];

	if (atomic_inc_return(&upcd.ep[ep].pkts_in_transit) == 1) {
		/* not been transmitting before, start transmit */
		pUDP->UDP_CSR[ep] |= AT91C_UDP_TXPKTRDY;
	}

	if ((end - start < AT91C_EP_OUT_SIZE) ||
	    (((end - start) == 0) && end && (rctx->tot_len % AT91C_EP_OUT_SIZE) == 0)) {
		/* CASE 1: return context to pool, if
		 * - packet transfer < AT91C_EP_OUT_SIZE
		 * - after ZLP of transfer == AT91C_EP_OUT_SIZE
		 * - after ZLP of transfer % AT91C_EP_OUT_SIZE == 0
		 * - after last packet of transfer % AT91C_EP_OUT_SIZE != 0
		 */
		DEBUGII("RCTX(ep=%u,ctx=%u)_tx_done ", ep, req_ctx_num(rctx));
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
		DEBUGII("RCTX(ep=%u)_tx_cont ", ep);
	}

	return 1;
}

int __ramfunc udp_refill_ep(int ep)
{
	unsigned long flags;
	int ret;

	local_irq_save(flags);
	ret = __udp_refill_ep(ep);
	local_irq_restore(flags);

	return ret;
}

static void __ramfunc udp_irq(void)
{
	u_int32_t csr;
	AT91PS_UDP pUDP = upcd.pUdp;
	AT91_REG isr = pUDP->UDP_ISR;

	DEBUGI("udp_irq(imr=0x%04x, isr=0x%04x, state=%d): ", 
		pUDP->UDP_IMR, isr, upcd.state);

	if (isr & AT91C_UDP_ENDBUSRES) {
		DEBUGI("ENDBUSRES ");
		pUDP->UDP_ICR = AT91C_UDP_ENDBUSRES;
		pUDP->UDP_IER = AT91C_UDP_EPINT0;
		/* reset all endpoints */
		pUDP->UDP_RSTEP = (unsigned int)-1;
		pUDP->UDP_RSTEP = 0;
		/* Enable the function */
		pUDP->UDP_FADDR = AT91C_UDP_FEN;
		/* Configure endpoint 0 */
		pUDP->UDP_CSR[0] = (AT91C_UDP_EPEDS | AT91C_UDP_EPTYPE_CTRL);
		upcd.cur_config = 0;
		upcd.state = USB_STATE_DEFAULT;

		reset_ep(1);
		reset_ep(2);
		reset_ep(3);
		
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
		u_int32_t cur_rcv_bank = upcd.cur_rcv_bank;
		u_int16_t i, pkt_size;
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

			__udp_refill_ep(2);
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

			__udp_refill_ep(3);
		}
	}
	if (isr & AT91C_UDP_RXSUSP) {
		pUDP->UDP_ICR = AT91C_UDP_RXSUSP;
		DEBUGI("RXSUSP ");
		/* FIXME: implement suspend/resume */
	}
	if (isr & AT91C_UDP_RXRSM) {
		pUDP->UDP_ICR = AT91C_UDP_RXRSM;
		DEBUGI("RXRSM ");
		/* FIXME: implement suspend/resume */
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
		/* FIXME: implement suspend/resume */
	}
out:
	DEBUGI("END\r\n");
	AT91F_AIC_ClearIt(AT91C_BASE_AIC, AT91C_ID_UDP);
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
	AT91F_PIO_SetOutput(AT91C_BASE_PIOA, OPENPCD_PIO_UDP_PUP);
}

void udp_reset(void)
{
	volatile int i;

	AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, OPENPCD_PIO_UDP_PUP);
	for (i = 0; i < 0xffff; i++)
		;
	AT91F_PIO_SetOutput(AT91C_BASE_PIOA, OPENPCD_PIO_UDP_PUP);
}

#ifdef DEBUG_UDP_EP0
#define DEBUGE(x, args ...)	DEBUGP(x, ## args)
#else
#define DEBUGE(x, args ...)	do { } while (0)
#endif

/* Handle requests on the USB Control Endpoint */
static void udp_ep0_handler(void)
{
	AT91PS_UDP pUDP = upcd.pUdp;
	u_int8_t bmRequestType, bRequest;
	u_int16_t wValue, wIndex, wLength, wStatus;
	u_int32_t csr = pUDP->UDP_CSR[0];

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
		u_int8_t desc_type, desc_index;
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
						  MIN(dfu->dfu_dev_descriptor->bLength,
						      wLength));
			else
#endif
			udp_ep0_send_data((const char *) &dev_descriptor,
					   MIN(sizeof(dev_descriptor), wLength));
			break;
		case USB_DT_CONFIG:
			/* Return Configuration Descriptor */
#ifdef CONFIG_DFU
			if (*dfu->dfu_state != DFU_STATE_appIDLE)
				udp_ep0_send_data((const char *)
						  dfu->dfu_cfg_descriptor,
						  MIN(dfu->dfu_cfg_descriptor->ucfg.wTotalLength,
						      wLength));
			else
#endif
			udp_ep0_send_data((const char *) &cfg_descriptor,
					   MIN(sizeof(cfg_descriptor), wLength));
			break;
		case USB_DT_STRING:
			/* Return String descriptor */
			if (desc_index > ARRAY_SIZE(usb_strings))
				goto out_stall;
			DEBUGE("bLength=%u, wLength=%u\n", 
				usb_strings[desc_index]->bLength, wLength);
			udp_ep0_send_data((const char *) usb_strings[desc_index],
					  MIN(usb_strings[desc_index]->bLength, 
					      wLength));
			break;
		case USB_DT_CS_DEVICE:
			/* Return Function descriptor */
			udp_ep0_send_data((const char *) &dfu->dfu_cfg_descriptor->func_dfu,
					  MIN(sizeof(dfu->dfu_cfg_descriptor->func_dfu), wLength));
		case USB_DT_INTERFACE:
			/* Return Interface descriptor */
			if (desc_index > cfg_descriptor.ucfg.bNumInterfaces)
				goto out_stall;
			switch (desc_index) {
			case 0:
				udp_ep0_send_data((const char *) 
					&cfg_descriptor.uif,
					MIN(sizeof(cfg_descriptor.uif),
					    wLength));
				break;
#ifdef CONFIG_DFU
			case 1:
				udp_ep0_send_data((const char *) 
					&cfg_descriptor.uif_dfu[0],
					MIN(sizeof(cfg_descriptor.uif_dfu[0]),
					    wLength));
				break;
			case 2:
				udp_ep0_send_data((const char *) 
					&cfg_descriptor.uif_dfu[1],
					MIN(sizeof(cfg_descriptor.uif_dfu[1]),
					    wLength));
				break;

#endif
			default:
				goto out_stall;
				break;
			}
			break;
		default:
			goto out_stall;
		}
		break;
	case STD_SET_ADDRESS:
		DEBUGE("SET_ADDRESS ");
		if (wValue > 127) 
			goto out_stall;

		switch (upcd.state) {
		case USB_STATE_DEFAULT:
			if (wValue == 0) {
				udp_ep0_send_zlp();
				/* do nothing */
			} else {
				udp_ep0_send_zlp();
				pUDP->UDP_FADDR = (AT91C_UDP_FEN | wValue);
				pUDP->UDP_GLBSTATE = AT91C_UDP_FADDEN;
				upcd.state = USB_STATE_ADDRESS;
			}
			break;
		case USB_STATE_ADDRESS:
			if (wValue == 0) {
				udp_ep0_send_zlp();
				upcd.state = USB_STATE_DEFAULT;
			} else {
				udp_ep0_send_zlp();
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
		pUDP->UDP_IER = (AT91C_UDP_EPINT0|AT91C_UDP_EPINT1|
				 AT91C_UDP_EPINT2|AT91C_UDP_EPINT3);
		break;
	case STD_GET_CONFIGURATION:
		DEBUGE("GET_CONFIG ");
		switch (upcd.state) {
		case USB_STATE_ADDRESS:
		case USB_STATE_CONFIGURED:
			udp_ep0_send_data((char *)&(upcd.cur_config),
					   sizeof(upcd.cur_config));
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
		udp_ep0_send_data((char *)&(upcd.cur_altsett),
				  sizeof(upcd.cur_altsett));
		break;
	case STD_GET_STATUS_ZERO:
		DEBUGE("GET_STATUS_ZERO ");
		wStatus = 0;
		udp_ep0_send_data((char *)&wStatus, sizeof(wStatus));
		break;
	case STD_GET_STATUS_INTERFACE:
		DEBUGE("GET_STATUS_INTERFACE ");
		if (upcd.state == USB_STATE_DEFAULT ||
		    (upcd.state == USB_STATE_ADDRESS && wIndex != 0))
		    	goto out_stall;
		wStatus = 0;
		udp_ep0_send_data((char *)&wStatus, sizeof(wStatus));
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
			udp_ep0_send_data((char *)&wStatus,
					   sizeof(wStatus));
		} else if ((pUDP->UDP_GLBSTATE & AT91C_UDP_FADDEN)
			   && (wIndex == 0)) {
			wStatus =
			    (pUDP->UDP_CSR[wIndex] & AT91C_UDP_EPEDS) ? 0 : 1;
			udp_ep0_send_data((char *)&wStatus,
					   sizeof(wStatus));
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
		udp_ep0_send_zlp();
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

