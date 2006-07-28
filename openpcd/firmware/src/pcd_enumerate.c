//*----------------------------------------------------------------------------
//*      ATMEL Microcontroller Software Support  -  ROUSSET  -
//*----------------------------------------------------------------------------
//* The software is delivered "AS IS" without warranty or condition of any
//* kind, either express, implied or statutory. This includes without
//* limitation any warranty or condition with respect to merchantability or
//* fitness for any particular purpose, or against the infringements of
//* intellectual property rights of others.
//*----------------------------------------------------------------------------
//* File Name           : cdc_enumerate.c
//* Object              : Handle CDC enumeration
//*
//* 1.0 Apr 20 200      : ODi Creation
//* 1.1 14/Sep/2004 JPP : Minor change
//* 1.1 15/12/2004  JPP : suppress warning
//*----------------------------------------------------------------------------

// 12. Apr. 2006: added modification found in the mikrocontroller.net gcc-Forum 
//                additional line marked with /* +++ */

#include <sys/types.h>
#include <usb_ch9.h>
#include <lib_AT91SAM7.h>
#include <openpcd.h>
#include <interrupt_utils.h>

#include "pcd_enumerate.h"
#include "openpcd.h"
#include "dbgu.h"

static struct _AT91S_CDC pCDC;
static AT91PS_CDC pCdc = &pCDC;

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

struct usb_device_descriptor devDescriptor = {
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
};

const struct _desc cfgDescriptor = {
	.ucfg = {
		 .bLength = USB_DT_CONFIG_SIZE,
		 .bDescriptorType = USB_DT_CONFIG,
		 .wTotalLength = USB_DT_CONFIG_SIZE +
		 USB_DT_INTERFACE_SIZE + 3 * USB_DT_ENDPOINT_SIZE,
		 .bNumInterfaces = 1,
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
			  .wMaxPacketSize = 64,
			  .bInterval = 0x10,	/* FIXME */
		  }, {
			  .bLength = USB_DT_ENDPOINT_SIZE,
			  .bDescriptorType = USB_DT_ENDPOINT,
			  .bEndpointAddress = OPENPCD_IN_EP,
			  .bmAttributes = USB_ENDPOINT_XFER_BULK,
			  .wMaxPacketSize = 64,
			  .bInterval = 0x10,	/* FIXME */
		  }, {
			  .bLength = USB_DT_ENDPOINT_SIZE,
			  .bDescriptorType = USB_DT_ENDPOINT,
			  .bEndpointAddress = OPENPCD_IRQ_EP,
			  .bmAttributes = USB_ENDPOINT_XFER_INT,
			  .wMaxPacketSize = 64,
			  .bInterval = 0x10,	/* FIXME */
		  },
	},
};

/* USB standard request code */

#define STD_GET_STATUS_ZERO           0x0080
#define STD_GET_STATUS_INTERFACE      0x0081
#define STD_GET_STATUS_ENDPOINT       0x0082

#define STD_CLEAR_FEATURE_ZERO        0x0100
#define STD_CLEAR_FEATURE_INTERFACE   0x0101
#define STD_CLEAR_FEATURE_ENDPOINT    0x0102

#define STD_SET_FEATURE_ZERO          0x0300
#define STD_SET_FEATURE_INTERFACE     0x0301
#define STD_SET_FEATURE_ENDPOINT      0x0302

#define STD_SET_ADDRESS               0x0500
#define STD_GET_DESCRIPTOR            0x0680
#define STD_SET_DESCRIPTOR            0x0700
#define STD_GET_CONFIGURATION         0x0880
#define STD_SET_CONFIGURATION         0x0900
#define STD_GET_INTERFACE             0x0A81
#define STD_SET_INTERFACE             0x0B01
#define STD_SYNCH_FRAME               0x0C82

static void udp_ep0_handler(void);

static void udp_irq(void)
{
	u_int32_t csr;
	AT91PS_UDP pUDP = pCDC.pUdp;
	AT91_REG isr = pUDP->UDP_ISR;

	DEBUGP("udp_irq(imr=0x%04x, isr=0x%04x): ", pUDP->UDP_IMR, isr);

	if (isr & AT91C_UDP_ENDBUSRES) {
		DEBUGP("ENDBUSRES ");
		pUDP->UDP_ICR = AT91C_UDP_ENDBUSRES;
		pUDP->UDP_IER = AT91C_UDP_EPINT0;
		// reset all endpoints
		pUDP->UDP_RSTEP = (unsigned int)-1;
		pUDP->UDP_RSTEP = 0;
		// Enable the function
		pUDP->UDP_FADDR = AT91C_UDP_FEN;
		// Configure endpoint 0
		pUDP->UDP_CSR[0] = (AT91C_UDP_EPEDS | AT91C_UDP_EPTYPE_CTRL);
		pCDC.currentConfiguration = 0;	/* +++ */
	}

	if (isr & AT91C_UDP_EPINT0) {
		DEBUGP("EP0INT(Control) ");
		udp_ep0_handler();
	}
	if (isr & AT91C_UDP_EPINT1) {
		u_int32_t cur_rcv_bank = pCDC.currentRcvBank;
		csr = pUDP->UDP_CSR[1];
		DEBUGP("EP1INT(Out, CSR=0x%08x) ", csr);
		if (cur_rcv_bank == AT91C_UDP_RX_DATA_BK1)
			DEBUGP("cur_bank=1 ");
		else if (cur_rcv_bank == AT91C_UDP_RX_DATA_BK0)
			DEBUGP("cur_bank=0 ");
		else
			DEBUGP("cur_bank INVALID ");

		if (csr & AT91C_UDP_RX_DATA_BK1)
			DEBUGP("BANK1 ");
		if (csr & AT91C_UDP_RX_DATA_BK0)
			DEBUGP("BANK0 ");

		if (csr & cur_rcv_bank) {
			u_int16_t pkt_recv = 0;
			u_int16_t pkt_size = csr >> 16;
			struct req_ctx *rctx = req_ctx_find_get();

			if (rctx) {
				rctx->rx.tot_len = pkt_size;
				while (pkt_size--)
					rctx->rx.data[pkt_recv++] = pUDP->UDP_FDR[1];
				pUDP->UDP_CSR[1] &= ~cur_rcv_bank;
				if (cur_rcv_bank == AT91C_UDP_RX_DATA_BK0)
					cur_rcv_bank = AT91C_UDP_RX_DATA_BK1;
				else
					cur_rcv_bank = AT91C_UDP_RX_DATA_BK0;
#if 0
				rctx->rx.data[MAX_REQSIZE+MAX_HDRSIZE-1] = '\0';
				DEBUGP(rctx->rx.data);
#endif
				pCDC.currentRcvBank = cur_rcv_bank;
			} else
				DEBUGP("NO RCTX AVAIL! ");
		}
	}
	if (isr & AT91C_UDP_EPINT2) {
		csr = pUDP->UDP_CSR[2];
		//pUDP->UDP_ICR = AT91C_UDP_EPINT2;
		DEBUGP("EP2INT(In) ");
	}
	if (isr & AT91C_UDP_EPINT3) {
		csr = pUDP->UDP_CSR[3];
		//pUDP->UDP_ICR = AT91C_UDP_EPINT3;
		DEBUGP("EP3INT(Interrupt) ");
	}

	if (isr & AT91C_UDP_RXSUSP) {
		pUDP->UDP_ICR = AT91C_UDP_RXSUSP;
		DEBUGP("RXSUSP ");
	}
	if (isr & AT91C_UDP_RXRSM) {
		pUDP->UDP_ICR = AT91C_UDP_RXRSM;
		DEBUGP("RXRSM ");
	}
	if (isr & AT91C_UDP_EXTRSM) {
		pUDP->UDP_ICR = AT91C_UDP_EXTRSM;
		DEBUGP("EXTRSM ");
	}
	if (isr & AT91C_UDP_SOFINT) {
		pUDP->UDP_ICR = AT91C_UDP_SOFINT;
		DEBUGP("SOFINT ");
	}
	if (isr & AT91C_UDP_WAKEUP) {
		pUDP->UDP_ICR = AT91C_UDP_WAKEUP;
		DEBUGP("WAKEUP ");
	}

	DEBUGP("END\r\n");
	AT91F_AIC_ClearIt(AT91C_BASE_AIC, AT91C_ID_UDP);
}

/* Open USB Device Port  */
static AT91PS_CDC AT91F_PCD_Open(AT91PS_UDP pUdp)
{
	DEBUGPCRF("entering");
	pCdc->pUdp = pUdp;
	pCdc->currentConfiguration = 0;
	pCdc->currentConnection = 0;
	pCdc->currentRcvBank = AT91C_UDP_RX_DATA_BK0;

	AT91F_AIC_ConfigureIt(AT91C_BASE_AIC, AT91C_ID_UDP,
			      OPENPCD_IRQ_PRIO_UDP,
			      AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL, udp_irq);
	AT91F_AIC_EnableIt(AT91C_BASE_AIC, AT91C_ID_UDP);

	/* End-of-Bus-Reset is always enabled */

	DEBUGPCRF("returning after enabling UDP irq");

	return pCdc;
}

void udp_init(void)
{
	/* Set the PLL USB Divider */
	AT91C_BASE_CKGR->CKGR_PLLR |= AT91C_CKGR_USBDIV_1;

	/* Enables the 48MHz USB clock UDPCK and System Peripheral USB Clock */
	AT91C_BASE_PMC->PMC_SCER = AT91C_PMC_UDP;
	AT91C_BASE_PMC->PMC_PCER = (1 << AT91C_ID_UDP);

	/* Enable UDP PullUp (USB_DP_PUP) : enable & Clear of the corresponding PIO
	 * Set in PIO mode and Configure in Output */
	AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, OPENPCD_PIO_UDP_PUP);
	/* Clear for set the Pul up resistor */
	AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, OPENPCD_PIO_UDP_PUP);

	/* CDC Open by structure initialization */
	AT91F_PCD_Open(AT91C_BASE_UDP);
}

/* Test if the device is configured and handle enumeration */
u_int8_t udp_is_configured(void)
{
	return pCdc->currentConfiguration;
}

/* Send data through endpoint 2/3 */
u_int32_t AT91F_UDP_Write(u_int8_t irq, const unsigned char *pData, u_int32_t length)
{
	AT91PS_UDP pUdp = pCdc->pUdp;
	u_int32_t cpt = 0;
	u_int8_t ep;

	DEBUGPCRF("enter(irq=%u, len=%u)", irq, length);

	if (irq)
		ep = 3;
	else
		ep = 2;

	// Send the first packet
	cpt = MIN(length, 64);
	length -= cpt;
	while (cpt--)
		pUdp->UDP_FDR[ep] = *pData++;
	DEBUGPCRF("sending first packet");
	pUdp->UDP_CSR[ep] |= AT91C_UDP_TXPKTRDY;

	while (length) {
		DEBUGPCRF("sending further packet");
		// Fill the second bank
		cpt = MIN(length, 64);
		length -= cpt;
		while (cpt--)
			pUdp->UDP_FDR[ep] = *pData++;
		DEBUGPCRF("waiting for end of further packet");
		// Wait for the the first bank to be sent
		while (!(pUdp->UDP_CSR[ep] & AT91C_UDP_TXCOMP))
			if (!udp_is_configured()) {
				DEBUGPCRF("return(!configured)");
				return length;
			}
		pUdp->UDP_CSR[ep] &= ~(AT91C_UDP_TXCOMP);
		while (pUdp->UDP_CSR[ep] & AT91C_UDP_TXCOMP) ;
		pUdp->UDP_CSR[ep] |= AT91C_UDP_TXPKTRDY;
	}
	// Wait for the end of transfer
	DEBUGPCRF("waiting for end of transfer");
	while (!(pUdp->UDP_CSR[ep] & AT91C_UDP_TXCOMP))
		if (!udp_is_configured()) {
			DEBUGPCRF("return(!configured)");
			return length;
		}
	pUdp->UDP_CSR[ep] &= ~(AT91C_UDP_TXCOMP);
	while (pUdp->UDP_CSR[ep] & AT91C_UDP_TXCOMP) ;

	DEBUGPCRF("return(normal, len=%u)", length);
	return length;
}

/* Send Data through the control endpoint */
static void udp_ep0_send_data(AT91PS_UDP pUdp, const char *pData, u_int32_t length)
{
	u_int32_t cpt = 0;
	AT91_REG csr;

	DEBUGP("send_data: %u bytes ", length);

	do {
		cpt = MIN(length, 8);
		length -= cpt;

		while (cpt--)
			pUdp->UDP_FDR[0] = *pData++;

		if (pUdp->UDP_CSR[0] & AT91C_UDP_TXCOMP) {
			pUdp->UDP_CSR[0] &= ~(AT91C_UDP_TXCOMP);
			while (pUdp->UDP_CSR[0] & AT91C_UDP_TXCOMP) ;
		}

		pUdp->UDP_CSR[0] |= AT91C_UDP_TXPKTRDY;
		do {
			csr = pUdp->UDP_CSR[0];

			// Data IN stage has been stopped by a status OUT
			if (csr & AT91C_UDP_RX_DATA_BK0) {
				pUdp->UDP_CSR[0] &= ~(AT91C_UDP_RX_DATA_BK0);
				DEBUGP("stopped by status out ");
				return;
			}
		} while (!(csr & AT91C_UDP_TXCOMP));

	} while (length);

	if (pUdp->UDP_CSR[0] & AT91C_UDP_TXCOMP) {
		pUdp->UDP_CSR[0] &= ~(AT91C_UDP_TXCOMP);
		while (pUdp->UDP_CSR[0] & AT91C_UDP_TXCOMP) ;
	}
}

/* Send zero length packet through the control endpoint */
static void udp_ep0_send_zlp(AT91PS_UDP pUdp)
{
	pUdp->UDP_CSR[0] |= AT91C_UDP_TXPKTRDY;
	while (!(pUdp->UDP_CSR[0] & AT91C_UDP_TXCOMP)) ;
	pUdp->UDP_CSR[0] &= ~(AT91C_UDP_TXCOMP);
	while (pUdp->UDP_CSR[0] & AT91C_UDP_TXCOMP) ;
}

/* Stall the control endpoint */
static void udp_ep0_send_stall(AT91PS_UDP pUdp)
{
	pUdp->UDP_CSR[0] |= AT91C_UDP_FORCESTALL;
	while (!(pUdp->UDP_CSR[0] & AT91C_UDP_ISOERROR)) ;
	pUdp->UDP_CSR[0] &= ~(AT91C_UDP_FORCESTALL | AT91C_UDP_ISOERROR);
	while (pUdp->UDP_CSR[0] & (AT91C_UDP_FORCESTALL | AT91C_UDP_ISOERROR)) ;
}

/* Handle requests on the USB Control Endpoint */
static void udp_ep0_handler(void)
{
	AT91PS_UDP pUDP = pCdc->pUdp;
	u_int8_t bmRequestType, bRequest;
	u_int16_t wValue, wIndex, wLength, wStatus;

	if (!(pUDP->UDP_CSR[0] & AT91C_UDP_RXSETUP)) {
		DEBUGP("no setup packet ");
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

	if (bmRequestType & 0x80) {
		pUDP->UDP_CSR[0] |= AT91C_UDP_DIR;
		while (!(pUDP->UDP_CSR[0] & AT91C_UDP_DIR)) ;
	}
	pUDP->UDP_CSR[0] &= ~AT91C_UDP_RXSETUP;
	while ((pUDP->UDP_CSR[0] & AT91C_UDP_RXSETUP)) ;

	// Handle supported standard device request Cf Table 9-3 in USB specification Rev 1.1
	switch ((bRequest << 8) | bmRequestType) {
	case STD_GET_DESCRIPTOR:
		DEBUGP("GET_DESCRIPTOR ");
		if (wValue == 0x100)	// Return Device Descriptor
			udp_ep0_send_data(pUDP, (const char *) &devDescriptor,
					   MIN(sizeof(devDescriptor), wLength));
		else if (wValue == 0x200)	// Return Configuration Descriptor
			udp_ep0_send_data(pUDP, (const char *) &cfgDescriptor,
					   MIN(sizeof(cfgDescriptor), wLength));
		else
			udp_ep0_send_stall(pUDP);
		break;
	case STD_SET_ADDRESS:
		DEBUGP("SET_ADDRESS ");
		udp_ep0_send_zlp(pUDP);
		pUDP->UDP_FADDR = (AT91C_UDP_FEN | wValue);
		pUDP->UDP_GLBSTATE = (wValue) ? AT91C_UDP_FADDEN : 0;
		break;
	case STD_SET_CONFIGURATION:
		DEBUGP("SET_CONFIG ");
		if (wValue)
			DEBUGP("VALUE!=0 ");
		pCdc->currentConfiguration = wValue;
		udp_ep0_send_zlp(pUDP);
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
				  0);//AT91C_UDP_EPINT2|AT91C_UDP_EPINT3);
		break;
	case STD_GET_CONFIGURATION:
		DEBUGP("GET_CONFIG ");
		udp_ep0_send_data(pUDP, (char *)&(pCdc->currentConfiguration),
				   sizeof(pCdc->currentConfiguration));
		break;
	case STD_GET_STATUS_ZERO:
		DEBUGP("GET_STATUS_ZERO ");
		wStatus = 0;
		udp_ep0_send_data(pUDP, (char *)&wStatus, sizeof(wStatus));
		break;
	case STD_GET_STATUS_INTERFACE:
		DEBUGP("GET_STATUS_INTERFACE ");
		wStatus = 0;
		udp_ep0_send_data(pUDP, (char *)&wStatus, sizeof(wStatus));
		break;
	case STD_GET_STATUS_ENDPOINT:
		DEBUGP("GET_STATUS_ENDPOINT(EPidx=%u) ", wIndex&0x0f);
		wStatus = 0;
		wIndex &= 0x0F;
		if ((pUDP->UDP_GLBSTATE & AT91C_UDP_CONFG) && (wIndex <= 3)) {
			wStatus =
			    (pUDP->UDP_CSR[wIndex] & AT91C_UDP_EPEDS) ? 0 : 1;
			udp_ep0_send_data(pUDP, (char *)&wStatus,
					   sizeof(wStatus));
		} else if ((pUDP->UDP_GLBSTATE & AT91C_UDP_FADDEN)
			   && (wIndex == 0)) {
			wStatus =
			    (pUDP->UDP_CSR[wIndex] & AT91C_UDP_EPEDS) ? 0 : 1;
			udp_ep0_send_data(pUDP, (char *)&wStatus,
					   sizeof(wStatus));
		} else
			udp_ep0_send_stall(pUDP);
		break;
	case STD_SET_FEATURE_ZERO:
		DEBUGP("SET_FEATURE_ZERO ");
		udp_ep0_send_stall(pUDP);
		break;
	case STD_SET_FEATURE_INTERFACE:
		DEBUGP("SET_FEATURE_INTERFACE ");
		udp_ep0_send_zlp(pUDP);
		break;
	case STD_SET_FEATURE_ENDPOINT:
		DEBUGP("SET_FEATURE_ENDPOINT ");
		udp_ep0_send_zlp(pUDP);
		wIndex &= 0x0F;
		if ((wValue == 0) && wIndex && (wIndex <= 3)) {
			pUDP->UDP_CSR[wIndex] = 0;
			udp_ep0_send_zlp(pUDP);
		} else
			udp_ep0_send_stall(pUDP);
		break;
	case STD_CLEAR_FEATURE_ZERO:
		DEBUGP("CLEAR_FEATURE_ZERO ");
		udp_ep0_send_stall(pUDP);
		break;
	case STD_CLEAR_FEATURE_INTERFACE:
		DEBUGP("CLEAR_FEATURE_INTERFACE ");
		udp_ep0_send_zlp(pUDP);
		break;
	case STD_CLEAR_FEATURE_ENDPOINT:
		DEBUGP("CLEAR_FEATURE_ENDPOINT(EPidx=%u) ", wIndex & 0x0f);
		wIndex &= 0x0F;
		if ((wValue == 0) && wIndex && (wIndex <= 3)) {
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
			}
			else if (wIndex == 3) {
				pUDP->UDP_CSR[3] =
				    (AT91C_UDP_EPEDS |
				     AT91C_UDP_EPTYPE_INT_IN);
				pUDP->UDP_RSTEP |= AT91C_UDP_EP3;
				pUDP->UDP_RSTEP &= ~AT91C_UDP_EP3;
			}
			udp_ep0_send_zlp(pUDP);
		} else
			udp_ep0_send_stall(pUDP);
		break;
	case STD_SET_INTERFACE:
		DEBUGP("SET INTERFACE ");
		udp_ep0_send_stall(pUDP);
		break;
	default:
		DEBUGP("DEFAULT(req=0x%02x, type=0x%02x) ", bRequest, bmRequestType);
		udp_ep0_send_stall(pUDP);
		break;
	}
}
