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

//#include "board.h"
#include <include/usb_ch9.h>
#include <include/types.h>
#include <include/lib_AT91SAM7S64.h>
#include "pcd_enumerate.h"
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
	.idVendor = 0x2342,
	.idProduct = 0x0001,
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
	.ep[0] = {
		  .bLength = USB_DT_ENDPOINT_SIZE,
		  .bDescriptorType = USB_DT_ENDPOINT,
		  .bEndpointAddress = 0x01,
		  .bmAttributes = USB_ENDPOINT_XFER_BULK,
		  .wMaxPacketSize = 64,
		  .bInterval = 0x10,	/* FIXME */
		  },
	.ep[1] = {
		  .bLength = USB_DT_ENDPOINT_SIZE,
		  .bDescriptorType = USB_DT_ENDPOINT,
		  .bEndpointAddress = 0x81,
		  .bmAttributes = USB_ENDPOINT_XFER_BULK,
		  .wMaxPacketSize = 64,
		  .bInterval = 0x10,	/* FIXME */
		  },
	.ep[2] = {
		  .bLength = USB_DT_ENDPOINT_SIZE,
		  .bDescriptorType = USB_DT_ENDPOINT,
		  .bEndpointAddress = 0x82,
		  .bmAttributes = USB_ENDPOINT_XFER_INT,
		  .wMaxPacketSize = 64,
		  .bInterval = 0x10,	/* FIXME */
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

/// mt u_int32_t currentReceiveBank = AT91C_UDP_RX_DATA_BK0;

static u_int32_t AT91F_UDP_Read(char *pData, u_int32_t length);
static u_int32_t AT91F_UDP_Write(const char *pData, u_int32_t length);
static void AT91F_CDC_Enumerate(void);

static void udp_irq(void)
{
	AT91PS_UDP pUDP = pCDC.pUdp;
	AT91_REG isr = pUDP->UDP_ISR;

	DEBUGP("udp_irq: ");

	if (isr & AT91C_UDP_ENDBUSRES) {
		DEBUGP("ENDBUSRES ");
		pUDP->UDP_ICR = AT91C_UDP_ENDBUSRES;
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
		DEBUGP("EP0INT ");
		pUDP->UDP_ICR = AT91C_UDP_EPINT0;
		AT91F_CDC_Enumerate();
	}
	if (isr & AT91C_UDP_EPINT1) {
		DEBUGP("EP1INT ");
	}
	if (isr & AT91C_UDP_EPINT2) {
		DEBUGP("EP2INT ");
	}
	DEBUGP("\n");
}

//*----------------------------------------------------------------------------
//* \fn    AT91F_CDC_Open
//* \brief
//*----------------------------------------------------------------------------
AT91PS_CDC AT91F_CDC_Open(AT91PS_UDP pUdp)
{
	pCdc->pUdp = pUdp;
	pCdc->currentConfiguration = 0;
	pCdc->currentConnection = 0;
	pCdc->currentRcvBank = AT91C_UDP_RX_DATA_BK0;

	AT91F_AIC_ConfigureIt(AT91C_BASE_AIC, AT91C_ID_UDP, AT91C_AIC_PRIOR_LOWEST, 
			      AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL, &udp_irq);
	AT91F_AIC_EnableIt(AT91C_BASE_AIC, AT91C_ID_UDP);

	/* End-of-Bus-Reset is always enabled */
	pCdc->pUdp->UDP_IER = (AT91C_UDP_EPINT0|AT91C_UDP_EPINT1|AT91C_UDP_EPINT2);

	return pCdc;
}

//*----------------------------------------------------------------------------
//* \fn    AT91F_UDP_IsConfigured
//* \brief Test if the device is configured and handle enumeration
//*----------------------------------------------------------------------------
u_int8_t AT91F_UDP_IsConfigured(void)
{
#if 0
	AT91PS_UDP pUDP = pCdc->pUdp;
	AT91_REG isr = pUDP->UDP_ISR;

	if (isr & AT91C_UDP_ENDBUSRES) {
		pUDP->UDP_ICR = AT91C_UDP_ENDBUSRES;
		// reset all endpoints
		pUDP->UDP_RSTEP = (unsigned int)-1;
		pUDP->UDP_RSTEP = 0;
		// Enable the function
		pUDP->UDP_FADDR = AT91C_UDP_FEN;
		// Configure endpoint 0
		pUDP->UDP_CSR[0] = (AT91C_UDP_EPEDS | AT91C_UDP_EPTYPE_CTRL);
		pCdc->currentConfiguration = 0;	/* +++ */
	} else if (isr & AT91C_UDP_EPINT0) {
		pUDP->UDP_ICR = AT91C_UDP_EPINT0;
		AT91F_CDC_Enumerate(pCdc);
	}
#endif
	return pCdc->currentConfiguration;
}

//*----------------------------------------------------------------------------
//* \fn    AT91F_UDP_Read
//* \brief Read available data from Endpoint OUT
//*----------------------------------------------------------------------------
static u_int32_t AT91F_UDP_Read(char *pData, u_int32_t length)
{
	AT91PS_UDP pUdp = pCdc->pUdp;
	u_int32_t packetSize, nbBytesRcv = 0, currentReceiveBank =
	    pCdc->currentRcvBank;

	while (length) {
		if (!AT91F_UDP_IsConfigured())
			break;
		if (pUdp->UDP_CSR[AT91C_EP_OUT] & currentReceiveBank) {
			packetSize =
			    MIN(pUdp->UDP_CSR[AT91C_EP_OUT] >> 16, length);
			length -= packetSize;
			if (packetSize < AT91C_EP_OUT_SIZE)
				length = 0;
			while (packetSize--)
				pData[nbBytesRcv++] =
				    pUdp->UDP_FDR[AT91C_EP_OUT];
			pUdp->UDP_CSR[AT91C_EP_OUT] &= ~(currentReceiveBank);
			if (currentReceiveBank == AT91C_UDP_RX_DATA_BK0)
				currentReceiveBank = AT91C_UDP_RX_DATA_BK1;
			else
				currentReceiveBank = AT91C_UDP_RX_DATA_BK0;

		}
	}
	pCdc->currentRcvBank = currentReceiveBank;
	return nbBytesRcv;

}

//*----------------------------------------------------------------------------
//* \fn    AT91F_CDC_Write
//* \brief Send through endpoint 2
//*----------------------------------------------------------------------------
static u_int32_t AT91F_UDP_Write(const char *pData, u_int32_t length)
{
	AT91PS_UDP pUdp = pCdc->pUdp;
	u_int32_t cpt = 0;

	// Send the first packet
	cpt = MIN(length, AT91C_EP_IN_SIZE);
	length -= cpt;
	while (cpt--)
		pUdp->UDP_FDR[AT91C_EP_IN] = *pData++;
	pUdp->UDP_CSR[AT91C_EP_IN] |= AT91C_UDP_TXPKTRDY;

	while (length) {
		// Fill the second bank
		cpt = MIN(length, AT91C_EP_IN_SIZE);
		length -= cpt;
		while (cpt--)
			pUdp->UDP_FDR[AT91C_EP_IN] = *pData++;
		// Wait for the the first bank to be sent
		while (!(pUdp->UDP_CSR[AT91C_EP_IN] & AT91C_UDP_TXCOMP))
			if (!AT91F_UDP_IsConfigured())
				return length;
		pUdp->UDP_CSR[AT91C_EP_IN] &= ~(AT91C_UDP_TXCOMP);
		while (pUdp->UDP_CSR[AT91C_EP_IN] & AT91C_UDP_TXCOMP) ;
		pUdp->UDP_CSR[AT91C_EP_IN] |= AT91C_UDP_TXPKTRDY;
	}
	// Wait for the end of transfer
	while (!(pUdp->UDP_CSR[AT91C_EP_IN] & AT91C_UDP_TXCOMP))
		if (!AT91F_UDP_IsConfigured())
			return length;
	pUdp->UDP_CSR[AT91C_EP_IN] &= ~(AT91C_UDP_TXCOMP);
	while (pUdp->UDP_CSR[AT91C_EP_IN] & AT91C_UDP_TXCOMP) ;

	return length;
}

//*----------------------------------------------------------------------------
//* \fn    AT91F_USB_SendData
//* \brief Send Data through the control endpoint
//*----------------------------------------------------------------------------
unsigned int csrTab[100];
unsigned char csrIdx = 0;

static void AT91F_USB_SendData(AT91PS_UDP pUdp, const char *pData, u_int32_t length)
{
	u_int32_t cpt = 0;
	AT91_REG csr;

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
				return;
			}
		} while (!(csr & AT91C_UDP_TXCOMP));

	} while (length);

	if (pUdp->UDP_CSR[0] & AT91C_UDP_TXCOMP) {
		pUdp->UDP_CSR[0] &= ~(AT91C_UDP_TXCOMP);
		while (pUdp->UDP_CSR[0] & AT91C_UDP_TXCOMP) ;
	}
}

//*----------------------------------------------------------------------------
//* \fn    AT91F_USB_SendZlp
//* \brief Send zero length packet through the control endpoint
//*----------------------------------------------------------------------------
void AT91F_USB_SendZlp(AT91PS_UDP pUdp)
{
	pUdp->UDP_CSR[0] |= AT91C_UDP_TXPKTRDY;
	while (!(pUdp->UDP_CSR[0] & AT91C_UDP_TXCOMP)) ;
	pUdp->UDP_CSR[0] &= ~(AT91C_UDP_TXCOMP);
	while (pUdp->UDP_CSR[0] & AT91C_UDP_TXCOMP) ;
}

//*----------------------------------------------------------------------------
//* \fn    AT91F_USB_SendStall
//* \brief Stall the control endpoint
//*----------------------------------------------------------------------------
void AT91F_USB_SendStall(AT91PS_UDP pUdp)
{
	pUdp->UDP_CSR[0] |= AT91C_UDP_FORCESTALL;
	while (!(pUdp->UDP_CSR[0] & AT91C_UDP_ISOERROR)) ;
	pUdp->UDP_CSR[0] &= ~(AT91C_UDP_FORCESTALL | AT91C_UDP_ISOERROR);
	while (pUdp->UDP_CSR[0] & (AT91C_UDP_FORCESTALL | AT91C_UDP_ISOERROR)) ;
}

//*----------------------------------------------------------------------------
//* \fn    AT91F_CDC_Enumerate
//* \brief This function is a callback invoked when a SETUP packet is received
//*----------------------------------------------------------------------------
static void AT91F_CDC_Enumerate(void)
{
	AT91PS_UDP pUDP = pCdc->pUdp;
	u_int8_t bmRequestType, bRequest;
	u_int16_t wValue, wIndex, wLength, wStatus;

	if (!(pUDP->UDP_CSR[0] & AT91C_UDP_RXSETUP))
		return;

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
		if (wValue == 0x100)	// Return Device Descriptor
			AT91F_USB_SendData(pUDP, (const char *) &devDescriptor,
					   MIN(sizeof(devDescriptor), wLength));
		else if (wValue == 0x200)	// Return Configuration Descriptor
			AT91F_USB_SendData(pUDP, (const char *) &cfgDescriptor,
					   MIN(sizeof(cfgDescriptor), wLength));
		else
			AT91F_USB_SendStall(pUDP);
		break;
	case STD_SET_ADDRESS:
		AT91F_USB_SendZlp(pUDP);
		pUDP->UDP_FADDR = (AT91C_UDP_FEN | wValue);
		pUDP->UDP_GLBSTATE = (wValue) ? AT91C_UDP_FADDEN : 0;
		break;
	case STD_SET_CONFIGURATION:
		pCdc->currentConfiguration = wValue;
		AT91F_USB_SendZlp(pUDP);
		pUDP->UDP_GLBSTATE =
		    (wValue) ? AT91C_UDP_CONFG : AT91C_UDP_FADDEN;
		pUDP->UDP_CSR[1] =
		    (wValue) ? (AT91C_UDP_EPEDS | AT91C_UDP_EPTYPE_BULK_OUT) :
		    0;
		pUDP->UDP_CSR[2] =
		    (wValue) ? (AT91C_UDP_EPEDS | AT91C_UDP_EPTYPE_BULK_IN) : 0;
		pUDP->UDP_CSR[3] =
		    (wValue) ? (AT91C_UDP_EPEDS | AT91C_UDP_EPTYPE_ISO_IN) : 0;
		break;
	case STD_GET_CONFIGURATION:
		AT91F_USB_SendData(pUDP, (char *)&(pCdc->currentConfiguration),
				   sizeof(pCdc->currentConfiguration));
		break;
	case STD_GET_STATUS_ZERO:
		wStatus = 0;
		AT91F_USB_SendData(pUDP, (char *)&wStatus, sizeof(wStatus));
		break;
	case STD_GET_STATUS_INTERFACE:
		wStatus = 0;
		AT91F_USB_SendData(pUDP, (char *)&wStatus, sizeof(wStatus));
		break;
	case STD_GET_STATUS_ENDPOINT:
		wStatus = 0;
		wIndex &= 0x0F;
		if ((pUDP->UDP_GLBSTATE & AT91C_UDP_CONFG) && (wIndex <= 3)) {
			wStatus =
			    (pUDP->UDP_CSR[wIndex] & AT91C_UDP_EPEDS) ? 0 : 1;
			AT91F_USB_SendData(pUDP, (char *)&wStatus,
					   sizeof(wStatus));
		} else if ((pUDP->UDP_GLBSTATE & AT91C_UDP_FADDEN)
			   && (wIndex == 0)) {
			wStatus =
			    (pUDP->UDP_CSR[wIndex] & AT91C_UDP_EPEDS) ? 0 : 1;
			AT91F_USB_SendData(pUDP, (char *)&wStatus,
					   sizeof(wStatus));
		} else
			AT91F_USB_SendStall(pUDP);
		break;
	case STD_SET_FEATURE_ZERO:
		AT91F_USB_SendStall(pUDP);
		break;
	case STD_SET_FEATURE_INTERFACE:
		AT91F_USB_SendZlp(pUDP);
		break;
	case STD_SET_FEATURE_ENDPOINT:
		wIndex &= 0x0F;
		if ((wValue == 0) && wIndex && (wIndex <= 3)) {
			pUDP->UDP_CSR[wIndex] = 0;
			AT91F_USB_SendZlp(pUDP);
		} else
			AT91F_USB_SendStall(pUDP);
		break;
	case STD_CLEAR_FEATURE_ZERO:
		AT91F_USB_SendStall(pUDP);
		break;
	case STD_CLEAR_FEATURE_INTERFACE:
		AT91F_USB_SendZlp(pUDP);
		break;
	case STD_CLEAR_FEATURE_ENDPOINT:
		wIndex &= 0x0F;
		if ((wValue == 0) && wIndex && (wIndex <= 3)) {
			if (wIndex == 1)
				pUDP->UDP_CSR[1] =
				    (AT91C_UDP_EPEDS |
				     AT91C_UDP_EPTYPE_BULK_OUT);
			else if (wIndex == 2)
				pUDP->UDP_CSR[2] =
				    (AT91C_UDP_EPEDS |
				     AT91C_UDP_EPTYPE_BULK_IN);
			else if (wIndex == 3)
				pUDP->UDP_CSR[3] =
				    (AT91C_UDP_EPEDS | AT91C_UDP_EPTYPE_ISO_IN);
			AT91F_USB_SendZlp(pUDP);
		} else
			AT91F_USB_SendStall(pUDP);
		break;
	default:
		AT91F_USB_SendStall(pUDP);
		break;
	}
}
