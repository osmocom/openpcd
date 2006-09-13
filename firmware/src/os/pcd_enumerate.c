/* AT91SAM7 USB interface code for OpenPCD 
 *
 * (C) 2006 by Harald Welte <laforge@gnumonks.org>
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

#include <os/pcd_enumerate.h>
#include <os/req_ctx.h>
#include <dfu/dfu.h>
#include "../openpcd.h"
#include <os/dbgu.h>

#define DEBUG_UDP_IRQ
#define DEBUG_UDP_EP0

#define CONFIG_DFU

#define AT91C_EP_OUT 1
#define AT91C_EP_OUT_SIZE 0x40
#define AT91C_EP_IN  2
#define AT91C_EP_IN_SIZE 0x40
#define AT91C_EP_INT  3

#ifdef CONFIG_DFU
#define DFU_API_LOCATION	((const struct dfuapi *) 0x00102100)
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
	.idVendor = OPENPCD_VENDOR_ID,
	.idProduct = OPENPCD_PRODUCT_ID,
	.bcdDevice = 0x0000,
	.iManufacturer = 0x00,
	.iProduct = 0x00,
	.iSerialNumber = 0x00,
	.bNumConfigurations = 0x01,
};

struct _desc {
	struct usb_config_descriptor ucfg;
	struct usb_interface_descriptor uif;
	struct usb_endpoint_descriptor ep[3];
#ifdef CONFIG_DFU
	struct usb_interface_descriptor uif_dfu;
	struct usb_dfu_func_descriptor func_dfu;
#endif
};

const struct _desc cfg_descriptor = {
	.ucfg = {
		 .bLength = USB_DT_CONFIG_SIZE,
		 .bDescriptorType = USB_DT_CONFIG,
		 .wTotalLength = USB_DT_CONFIG_SIZE +
#ifdef CONFIG_DFU
		 		 2 * USB_DT_INTERFACE_SIZE + 
				 3 * USB_DT_ENDPOINT_SIZE +
				 USB_DT_DFU_SIZE,
		 .bNumInterfaces = 2,
#else
		 		 1 * USB_DT_INTERFACE_SIZE + 
				 3 * USB_DT_ENDPOINT_SIZE,
		 .bNumInterfaces = 1,
#endif
		 .bConfigurationValue = 1,
		 .iConfiguration = 0,
		 .bmAttributes = USB_CONFIG_ATT_ONE,
		 .bMaxPower = 100,	/* 200mA */
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
		.iInterface = 0,
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
	.func_dfu = DFU_FUNC_DESC,
#endif
};

static const struct usb_string_descriptor string0 = {
	.bLength = sizeof(string0),
	.bDescriptorType = USB_DT_STRING,
	.wData[0] = 0x0409,	/* English */
};


static void udp_ep0_handler(void);

void udp_unthrottle(void)
{
	AT91PS_UDP pUDP = upcd.pUdp;
	pUDP->UDP_IER = AT91C_UDP_EPINT1;
}

int udp_refill_ep(int ep, struct req_ctx *rctx)
{
	u_int16_t i;
	AT91PS_UDP pUDP = upcd.pUdp;

	if (!upcd.cur_config)
		return -ENXIO;
	
	if (rctx->tx.tot_len > AT91C_EP_IN_SIZE) {
		DEBUGPCRF("TOO LARGE!!!!!!!!!!!!!!!!!!!!!!!!!!! (%d > %d)",
			  rctx->tx.tot_len, AT91C_EP_IN_SIZE);
		return -EINVAL;
	}

	if (atomic_read(&upcd.ep[ep].pkts_in_transit) == 2)
		return -EBUSY;
		
	/* fill FIFO/DPR */
	for (i = 0; i < rctx->tx.tot_len; i++) 
		pUDP->UDP_FDR[ep] = rctx->tx.data[i];

	if (atomic_inc_return(&upcd.ep[ep].pkts_in_transit) == 1) {
		/* not been transmitting before, start transmit */
		pUDP->UDP_CSR[ep] |= AT91C_UDP_TXPKTRDY;
	}

	/* return rctx to pool of free contexts */
	req_ctx_put(rctx);

	return 0;
}

#ifdef DEBUG_UDP_IRQ
#define DEBUGI(x, args ...)	DEBUGP(x, ## args)
#else
#define DEBUGI(x, args ...)	do { } while (0)
#endif

static void udp_irq(void)
{
	u_int32_t csr;
	AT91PS_UDP pUDP = upcd.pUdp;
	AT91_REG isr = pUDP->UDP_ISR;

	DEBUGI("udp_irq(imr=0x%04x, isr=0x%04x): ", pUDP->UDP_IMR, isr);

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
		
#ifdef CONFIG_DFU
		if (*dfu->dfu_state == DFU_STATE_appDETACH) {
			/* now we need to switch to DFU mode */
			dfu->dfu_switch();
		}
#endif
	}

	if (isr & AT91C_UDP_EPINT0) {
		DEBUGI("EP0INT(Control) ");
		udp_ep0_handler();
	}
	if (isr & AT91C_UDP_EPINT1) {
		u_int32_t cur_rcv_bank = upcd.cur_rcv_bank;
		csr = pUDP->UDP_CSR[1];
		DEBUGI("EP1INT(Out, CSR=0x%08x) ", csr);
		if (cur_rcv_bank == AT91C_UDP_RX_DATA_BK1)
			DEBUGI("cur_bank=1 ");
		else if (cur_rcv_bank == AT91C_UDP_RX_DATA_BK0)
			DEBUGI("cur_bank=0 ");
		else
			DEBUGI("cur_bank INVALID ");

		if (csr & AT91C_UDP_RX_DATA_BK1)
			DEBUGI("BANK1 ");
		if (csr & AT91C_UDP_RX_DATA_BK0)
			DEBUGI("BANK0 ");

		if (csr & cur_rcv_bank) {
			u_int16_t pkt_recv = 0;
			u_int16_t pkt_size = csr >> 16;
			struct req_ctx *rctx = req_ctx_find_get(RCTX_STATE_FREE,
								RCTX_STATE_UDP_RCV_BUSY);

			if (rctx) {
				rctx->rx.tot_len = pkt_size;
				while (pkt_size--)
					rctx->rx.data[pkt_recv++] = pUDP->UDP_FDR[1];
				pUDP->UDP_CSR[1] &= ~cur_rcv_bank;
				if (cur_rcv_bank == AT91C_UDP_RX_DATA_BK0)
					cur_rcv_bank = AT91C_UDP_RX_DATA_BK1;
				else
					cur_rcv_bank = AT91C_UDP_RX_DATA_BK0;
				upcd.cur_rcv_bank = cur_rcv_bank;
				req_ctx_set_state(rctx, RCTX_STATE_UDP_RCV_DONE);
				DEBUGI("RCTX=%u ", req_ctx_num(rctx));
			} else {
				/* disable interrupts for now */
				pUDP->UDP_IDR = AT91C_UDP_EPINT1;
				DEBUGP("NO_RCTX_AVAIL! ");
			}
		}
	}
	if (isr & AT91C_UDP_EPINT2) {
		csr = pUDP->UDP_CSR[2];
		DEBUGI("EP2INT(In, CSR=0x%08x) ", csr);
		if (csr & AT91C_UDP_TXCOMP) {
			struct req_ctx *rctx;

			DEBUGI("ACK_TX_COMP ");
			/* acknowledge TX completion */
			pUDP->UDP_CSR[2] &= ~AT91C_UDP_TXCOMP;
			while (pUDP->UDP_CSR[2] & AT91C_UDP_TXCOMP) ;

			/* if we already have another packet in DPR, send it */
			if (atomic_dec_return(&upcd.ep[2].pkts_in_transit) == 1)
				pUDP->UDP_CSR[2] |= AT91C_UDP_TXPKTRDY;

			/* try to re-fill from pending rcts for EP2 */
			rctx = req_ctx_find_get(RCTX_STATE_UDP_EP2_PENDING,
						RCTX_STATE_UDP_EP2_BUSY);
			if (rctx)
				udp_refill_ep(2, rctx);
			else
				DEBUGI("NO_RCTX_pending ");
		}
	}
	if (isr & AT91C_UDP_EPINT3) {
		csr = pUDP->UDP_CSR[3];
		DEBUGI("EP3INT(Interrupt, CSR=0x%08x) ", csr);
		/* Transmit has completed, re-fill from pending rcts for EP3 */
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

	AT91F_AIC_ConfigureIt(AT91C_BASE_AIC, AT91C_ID_UDP,
			      OPENPCD_IRQ_PRIO_UDP,
			      AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL, udp_irq);
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

	/* Handle supported standard device request Cf Table 9-3 in USB
	 * speciication Rev 1.1 */
	switch ((bRequest << 8) | bmRequestType) {
	case STD_GET_DESCRIPTOR:
		DEBUGE("GET_DESCRIPTOR ");
		if (wValue == 0x100) {
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
		} else if (wValue == 0x200) {
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
		} else if (wValue == 0x300) {
			/* Return String descriptor */
			switch (wIndex) {
			case 0:
				udp_ep0_send_data((const char *) &string0,
						  MIN(sizeof(string0), wLength));
				break;
			default:
				/* FIXME: implement this */
				udp_ep0_send_stall();
				break;
			}
#if 0
		} else if (wValue == 0x400) {
			/* Return Interface descriptor */
			if (wIndex != 0x01)
				udp_ep0_send_stall();
			udp_ep0_send_data((const char *) 
					  &dfu_if_descriptor,
					  MIN(sizeof(dfu_if_descriptor),
					      wLength));
#endif
		} else
			udp_ep0_send_stall();
		break;
	case STD_SET_ADDRESS:
		DEBUGE("SET_ADDRESS ");
		udp_ep0_send_zlp();
		pUDP->UDP_FADDR = (AT91C_UDP_FEN | wValue);
		pUDP->UDP_GLBSTATE = (wValue) ? AT91C_UDP_FADDEN : 0;
		break;
	case STD_SET_CONFIGURATION:
		DEBUGE("SET_CONFIG ");
		if (wValue)
			DEBUGE("VALUE!=0 ");
		upcd.cur_config = wValue;
		udp_ep0_send_zlp();
		pUDP->UDP_GLBSTATE =
		    (wValue) ? AT91C_UDP_CONFG : AT91C_UDP_FADDEN;
		pUDP->UDP_CSR[1] =
		    (wValue) ? (AT91C_UDP_EPEDS | AT91C_UDP_EPTYPE_BULK_OUT) :
		    0;
		pUDP->UDP_CSR[2] =
		    (wValue) ? (AT91C_UDP_EPEDS | AT91C_UDP_EPTYPE_BULK_IN) : 0;
		pUDP->UDP_CSR[3] =
		    (wValue) ? (AT91C_UDP_EPEDS | AT91C_UDP_EPTYPE_INT_IN) : 0;
		pUDP->UDP_IER = (AT91C_UDP_EPINT0|AT91C_UDP_EPINT1|
				 AT91C_UDP_EPINT2|AT91C_UDP_EPINT3);
		break;
	case STD_GET_CONFIGURATION:
		DEBUGE("GET_CONFIG ");
		udp_ep0_send_data((char *)&(upcd.cur_config),
				   sizeof(upcd.cur_config));
		break;
	case STD_GET_STATUS_ZERO:
		DEBUGE("GET_STATUS_ZERO ");
		wStatus = 0;
		udp_ep0_send_data((char *)&wStatus, sizeof(wStatus));
		break;
	case STD_GET_STATUS_INTERFACE:
		DEBUGE("GET_STATUS_INTERFACE ");
		wStatus = 0;
		udp_ep0_send_data((char *)&wStatus, sizeof(wStatus));
		break;
	case STD_GET_STATUS_ENDPOINT:
		DEBUGE("GET_STATUS_ENDPOINT(EPidx=%u) ", wIndex&0x0f);
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
			udp_ep0_send_stall();
		break;
	case STD_SET_FEATURE_ZERO:
		DEBUGE("SET_FEATURE_ZERO ");
		udp_ep0_send_stall();
		break;
	case STD_SET_FEATURE_INTERFACE:
		DEBUGE("SET_FEATURE_INTERFACE ");
		udp_ep0_send_zlp();
		break;
	case STD_SET_FEATURE_ENDPOINT:
		DEBUGE("SET_FEATURE_ENDPOINT ");
		udp_ep0_send_zlp();
		wIndex &= 0x0F;
		if ((wValue == 0) && wIndex && (wIndex <= 3)) {
			pUDP->UDP_CSR[wIndex] = 0;
			udp_ep0_send_zlp();
		} else
			udp_ep0_send_stall();
		break;
	case STD_CLEAR_FEATURE_ZERO:
		DEBUGE("CLEAR_FEATURE_ZERO ");
		udp_ep0_send_stall();
		break;
	case STD_CLEAR_FEATURE_INTERFACE:
		DEBUGP("CLEAR_FEATURE_INTERFACE ");
		udp_ep0_send_zlp();
		break;
	case STD_CLEAR_FEATURE_ENDPOINT:
		DEBUGE("CLEAR_FEATURE_ENDPOINT(EPidx=%u) ", wIndex & 0x0f);
		wIndex &= 0x0F;
		if ((wValue == 0) && wIndex && (wIndex <= 3)) {
			struct req_ctx *rctx;
			if (wIndex == 1) {
				pUDP->UDP_CSR[1] =
				    (AT91C_UDP_EPEDS |
				     AT91C_UDP_EPTYPE_BULK_OUT);
				pUDP->UDP_RSTEP |= AT91C_UDP_EP1;
				pUDP->UDP_RSTEP &= ~AT91C_UDP_EP1;
			}
			else if (wIndex == 2) {
				pUDP->UDP_CSR[2] =
				    (AT91C_UDP_EPEDS |
				     AT91C_UDP_EPTYPE_BULK_IN);
				pUDP->UDP_RSTEP |= AT91C_UDP_EP2;
				pUDP->UDP_RSTEP &= ~AT91C_UDP_EP2;

				/* free all currently transmitting contexts */
				while (rctx = req_ctx_find_get(RCTX_STATE_UDP_EP2_BUSY,
							       RCTX_STATE_FREE)) {}
				atomic_set(&upcd.ep[wIndex].pkts_in_transit, 0);
			}
			else if (wIndex == 3) {
				pUDP->UDP_CSR[3] =
				    (AT91C_UDP_EPEDS |
				     AT91C_UDP_EPTYPE_INT_IN);
				pUDP->UDP_RSTEP |= AT91C_UDP_EP3;
				pUDP->UDP_RSTEP &= ~AT91C_UDP_EP3;

				/* free all currently transmitting contexts */
				while (rctx = req_ctx_find_get(RCTX_STATE_UDP_EP3_BUSY,
							       RCTX_STATE_FREE)) {}
				atomic_set(&upcd.ep[wIndex].pkts_in_transit, 0);
			}
			udp_ep0_send_zlp();
		} else
			udp_ep0_send_stall();
		break;
	case STD_SET_INTERFACE:
		DEBUGE("SET INTERFACE ");
		udp_ep0_send_stall();
		break;
	default:
		DEBUGE("DEFAULT(req=0x%02x, type=0x%02x) ", bRequest, bmRequestType);
#ifdef CONFIG_DFU
		if ((bmRequestType & 0x3f) == USB_TYPE_DFU) {
			dfu->dfu_ep0_handler(bmRequestType, bRequest, wValue, wLength);
		} else
#endif
		udp_ep0_send_stall();
		break;
	}
}

