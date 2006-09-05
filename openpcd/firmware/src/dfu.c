/* USB Device Firmware Update Implementation for OpenPCD
 * (C) 2006 by Harald Welte <hwelte@hmw-consulting.de>
 *
 * This ought to be compliant to the USB DFU Spec 1.0 as available from
 * http://www.usb.org/developers/devclass_docs/usbdfu10.pdf
 *
 */

#include <errno.h>
#include <usb_ch9.h>
#include <usb_dfu.h>
#include <lib_AT91SAM7.h>

#include "dfu.h"
#include "pcd_enumerate.h"
#include "openpcd.h"

/* If debug is enabled, we need to access debug functions from flash
 * and therefore have to omit flashing */
#define DEBUG_DFU

#ifdef DEBUG_DFU
#define DEBUGE DEBUGP
#define DEBUGI DEBUGP
#else
#define DEBUGE(x, args ...)
#define DEBUGI(x, args ...)
#endif

/* this is only called once before DFU mode, no __dfufunc required */
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
}

/* Send Data through the control endpoint */
static void __dfufunc udp_ep0_send_data(const char *pData, u_int32_t length)
{
	AT91PS_UDP pUdp = AT91C_BASE_UDP;
	u_int32_t cpt = 0;
	AT91_REG csr;

	DEBUGE("send_data: %u bytes ", length);

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

			/* Data IN stage has been stopped by a status OUT */
			if (csr & AT91C_UDP_RX_DATA_BK0) {
				pUdp->UDP_CSR[0] &= ~(AT91C_UDP_RX_DATA_BK0);
				DEBUGE("stopped by status out ");
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
static void __dfufunc udp_ep0_send_zlp(void)
{
	AT91PS_UDP pUdp = AT91C_BASE_UDP;
	pUdp->UDP_CSR[0] |= AT91C_UDP_TXPKTRDY;
	while (!(pUdp->UDP_CSR[0] & AT91C_UDP_TXCOMP)) ;
	pUdp->UDP_CSR[0] &= ~(AT91C_UDP_TXCOMP);
	while (pUdp->UDP_CSR[0] & AT91C_UDP_TXCOMP) ;
}

/* Stall the control endpoint */
static void __dfufunc udp_ep0_send_stall(void)
{
	AT91PS_UDP pUdp = AT91C_BASE_UDP;
	pUdp->UDP_CSR[0] |= AT91C_UDP_FORCESTALL;
	while (!(pUdp->UDP_CSR[0] & AT91C_UDP_ISOERROR)) ;
	pUdp->UDP_CSR[0] &= ~(AT91C_UDP_FORCESTALL | AT91C_UDP_ISOERROR);
	while (pUdp->UDP_CSR[0] & (AT91C_UDP_FORCESTALL | AT91C_UDP_ISOERROR)) ;
}


static u_int8_t status;
static u_int8_t *ptr;
static u_int8_t dfu_state;

static int __dfufunc handle_dnload(u_int16_t val, u_int16_t len)
{
	volatile u_int32_t *p = (volatile u_int32_t *)ptr;

	DEBUGE("download ");

	if (len > AT91C_IFLASH_PAGE_SIZE) {
		/* Too big */
	    	dfu_state = DFU_STATE_dfuERROR;
		status = DFU_STATUS_errADDRESS;
		udp_ep0_send_stall();
		return -EINVAL;
	}
	if (len & 0x3) {
		dfu_state = DFU_STATE_dfuERROR;
		status = DFU_STATUS_errADDRESS;
		udp_ep0_send_stall();
		return -EINVAL;
	}
	if (len == 0) {
		dfu_state = DFU_STATE_dfuMANIFEST_SYNC;
		return 0;
	}
#if 0
	for (i = 0; i < len/4; i++)
		p[i] =
#endif
	
	/* FIXME: get data packet from FIFO, (erase+)write flash */
#ifndef DEBUG_DFU

#endif

	return 0;
}

static __dfufunc int handle_upload(u_int16_t val, u_int16_t len)
{
	DEBUGE("upload ");
	if (len > AT91C_IFLASH_PAGE_SIZE
	    || ptr > AT91C_IFLASH_SIZE) {
		/* Too big */
		dfu_state = DFU_STATE_dfuERROR;
		status = DFU_STATUS_errADDRESS;
		udp_ep0_send_stall();
		return -EINVAL;
	}

	if (ptr + len > AT91C_IFLASH_SIZE) 
		len = AT91C_IFLASH_SIZE - (u_int32_t) ptr;
		
	udp_ep0_send_data(ptr, len);
	ptr+= len;

	return len;
}

static __dfufunc void handle_getstatus(void)
{
	struct dfu_status dstat;

	DEBUGE("getstatus ");

	/* send status response */
	dstat.bStatus = status;
	dstat.bState = dfu_state;
	dstat.iString = 0;
	udp_ep0_send_data(&dstat, sizeof(dstat));
}

static void __dfufunc handle_getstate(void)
{
	u_int8_t u8 = dfu_state;
	DEBUGE("getstate ");
	udp_ep0_send_data((char *)&u8, sizeof(u8));
}

/* callback function for DFU requests */
int __dfufunc dfu_ep0_handler(u_int8_t req_type, u_int8_t req,
		    u_int16_t val, u_int16_t len)
{
	int rc;

	DEBUGE("old_state = %u ", dfu_state);

	switch (dfu_state) {
	case DFU_STATE_appIDLE:
		if (req != USB_REQ_DFU_DETACH)
			goto send_stall;
		dfu_state = DFU_STATE_appDETACH;
		goto send_zlp;
		break;
	case DFU_STATE_appDETACH:
		switch (req) {
		case USB_REQ_DFU_GETSTATUS:
			handle_getstatus();
			break;
		case USB_REQ_DFU_GETSTATE:
			handle_getstate();
			break;
		default:
			dfu_state = DFU_STATE_appIDLE;
			goto send_stall;
			break;
		}
		/* FIXME: implement timer to return to appIDLE */
		break;
	case DFU_STATE_dfuIDLE:
		switch (req) {
		case USB_REQ_DFU_DNLOAD:
			if (len == 0) {
				dfu_state = DFU_STATE_dfuERROR;
				goto send_stall;
			}
			handle_dnload(val, len);
			break;
		case USB_REQ_DFU_UPLOAD:
			ptr = 0;
			dfu_state = DFU_STATE_dfuUPLOAD_IDLE;
			handle_upload(val, len);
			break;
		case USB_REQ_DFU_ABORT:
			/* no zlp? */
			goto send_zlp;
			break;
		case USB_REQ_DFU_GETSTATUS:
			handle_getstatus();
			break;
		case USB_REQ_DFU_GETSTATE:
			handle_getstate();
			break;
		default:
			dfu_state = DFU_STATE_dfuERROR;
			goto send_stall;
			break;
		}
		break;
	case DFU_STATE_dfuDNLOAD_SYNC:
		switch (req) {
		case USB_REQ_DFU_GETSTATUS:
			handle_getstatus();
			/* FIXME: state transition depending on block completeness */
			break;
		case USB_REQ_DFU_GETSTATE:
			handle_getstate();
			break;
		default:
			dfu_state = DFU_STATE_dfuERROR;
			goto send_stall;
		}
		break;
	case DFU_STATE_dfuDNBUSY:
		dfu_state = DFU_STATE_dfuERROR;
		goto send_stall;
		break;
	case DFU_STATE_dfuDNLOAD_IDLE:
		switch (req) {
		case USB_REQ_DFU_DNLOAD:
			if (handle_dnload(val, len))
			/* FIXME: state transition */
			break;
		case USB_REQ_DFU_ABORT:
			dfu_state = DFU_STATE_dfuIDLE;
			goto send_zlp;
			break;
		case USB_REQ_DFU_GETSTATUS:
			handle_getstatus();
			break;
		case USB_REQ_DFU_GETSTATE:
			handle_getstate();
			break;
		default:
			dfu_state = DFU_STATE_dfuERROR;
			goto send_stall;
			break;
		}
		break;
	case DFU_STATE_dfuMANIFEST_SYNC:
		switch (req) {
		case USB_REQ_DFU_GETSTATUS:
			break;
		case USB_REQ_DFU_GETSTATE:
			handle_getstate();
			break;
		default:
			dfu_state = DFU_STATE_dfuERROR;
			goto send_stall;
			break;
		}
		break;
	case DFU_STATE_dfuMANIFEST:
		dfu_state = DFU_STATE_dfuERROR;
		goto send_stall;
		break;
	case DFU_STATE_dfuMANIFEST_WAIT_RST:
		/* we should never go here */
		break;
	case DFU_STATE_dfuUPLOAD_IDLE:
		switch (req) {
		case USB_REQ_DFU_UPLOAD:
			/* state transition if less data then requested */
			rc = handle_upload(val, len);
			if (rc >= 0 && rc < len)
				dfu_state = DFU_STATE_dfuIDLE;
			break;
		case USB_REQ_DFU_ABORT:
			dfu_state = DFU_STATE_dfuIDLE;
			/* no zlp? */
			goto send_zlp;
			break;
		case USB_REQ_DFU_GETSTATUS:
			handle_getstatus();
			break;
		case USB_REQ_DFU_GETSTATE:
			handle_getstate();
			break;
		default:
			dfu_state = DFU_STATE_dfuERROR;
			goto send_stall;
			break;
		}
		break;
	case DFU_STATE_dfuERROR:
		switch (req) {
		case USB_REQ_DFU_GETSTATUS:
			handle_getstatus();
			break;
		case USB_REQ_DFU_GETSTATE:
			handle_getstate();
			break;
		case USB_REQ_DFU_CLRSTATUS:
			dfu_state = DFU_STATE_dfuIDLE;
			/* no zlp? */
			goto send_zlp;
			break;
		default:
			dfu_state = DFU_STATE_dfuERROR;
			goto send_stall;
			break;
		}
		break;
	}

	DEBUGE("OK new_state = %u\r\n", dfu_state);
	return 0;

send_stall:
	udp_ep0_send_stall();
	DEBUGE("STALL new_state = %u\r\n", dfu_state);
	return -EINVAL;

send_zlp:
	udp_ep0_send_zlp();
	DEBUGE("ZLP new_state = %u\r\n", dfu_state);
	return 0;
}
static u_int8_t cur_config;

/* USB DFU Device descriptor in DFU mode */
__dfustruct struct usb_device_descriptor dfu_dev_descriptor = {
	.bLength		= USB_DT_DEVICE_SIZE,
	.bDescriptorType	= USB_DT_DEVICE,
	.bcdUSB			= 0x0100,
	.bDeviceClass		= 0x00,
	.bDeviceSubClass	= 0x00,
	.bDeviceProtocol	= 0x00,
	.bMaxPacketSize0	= 8,
	.idVendor		= OPENPCD_VENDOR_ID,
	.idProduct		= OPENPCD_PRODUCT_ID,
	.bcdDevice		= 0x0000,
	.iManufacturer		= 0x00,
	.iProduct		= 0x00,
	.iSerialNumber		= 0x00,
	.bNumConfigurations	= 0x01,
};

/* USB DFU Config descriptor in DFU mode */
__dfustruct struct _dfu_desc dfu_cfg_descriptor = {
	.ucfg = {
		.bLength = USB_DT_CONFIG_SIZE,
		.bDescriptorType = USB_DT_CONFIG,
		.wTotalLength = USB_DT_CONFIG_SIZE + 
				2* USB_DT_INTERFACE_SIZE +
				USB_DT_DFU_SIZE,
		.bNumInterfaces = 1,
		.bConfigurationValue = 1,
		.iConfiguration = 0,
		.bmAttributes = USB_CONFIG_ATT_ONE,
		.bMaxPower = 100,
		},
	.uif[0] = {
		.bLength		= USB_DT_INTERFACE_SIZE,
		.bDescriptorType	= USB_DT_INTERFACE,
		.bInterfaceNumber	= 0x00,
		.bAlternateSetting	= 0x00,
		.bNumEndpoints		= 0x00,
		.bInterfaceClass	= 0xfe,
		.bInterfaceSubClass	= 0x01,
		.bInterfaceProtocol	= 0x02,
		.iInterface		= 0,
		}, 
	.uif[1] = {
		.bLength		= USB_DT_INTERFACE_SIZE,
		.bDescriptorType	= USB_DT_INTERFACE,
		.bInterfaceNumber	= 0x00,
		.bAlternateSetting	= 0x01,
		.bNumEndpoints		= 0x00,
		.bInterfaceClass	= 0xfe,
		.bInterfaceSubClass	= 0x01,
		.bInterfaceProtocol	= 0x02,
		.iInterface		= 0,
		}, 

	.func_dfu = DFU_FUNC_DESC,
};


/* minimal USB EP0 handler in DFU mode */
static __dfufunc void dfu_udp_ep0_handler(void)
{
	AT91PS_UDP pUDP = AT91C_BASE_UDP;
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
			udp_ep0_send_data((const char *) 
					  &dfu_dev_descriptor,
					  MIN(sizeof(dfu_dev_descriptor),
					      wLength));
		} else if (wValue == 0x200) {
			/* Return Configuration Descriptor */
			udp_ep0_send_data((const char *)
					  &dfu_cfg_descriptor,
					  MIN(sizeof(dfu_cfg_descriptor),
					      wLength));
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
		cur_config = wValue;
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
		udp_ep0_send_data((char *)&(cur_config),
				   sizeof(cur_config));
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
		if ((pUDP->UDP_GLBSTATE & AT91C_UDP_CONFG) && (wIndex == 0)) {
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
		udp_ep0_send_stall();
		break;
	case STD_CLEAR_FEATURE_ZERO:
		DEBUGE("CLEAR_FEATURE_ZERO ");
		udp_ep0_send_stall();
		break;
	case STD_CLEAR_FEATURE_INTERFACE:
		DEBUGE("CLEAR_FEATURE_INTERFACE ");
		udp_ep0_send_zlp();
		break;
	case STD_CLEAR_FEATURE_ENDPOINT:
		DEBUGE("CLEAR_FEATURE_ENDPOINT(EPidx=%u) ", wIndex & 0x0f);
		udp_ep0_send_stall();
		break;
	case STD_SET_INTERFACE:
		DEBUGE("SET INTERFACE ");
		udp_ep0_send_stall();
		break;
	default:
		DEBUGE("DEFAULT(req=0x%02x, type=0x%02x) ", bRequest, bmRequestType);
		if ((bmRequestType & 0x3f) == USB_TYPE_DFU) {
			dfu_ep0_handler(bmRequestType, bRequest, wValue, wLength);
		} else
			udp_ep0_send_stall();
		break;
	}
}

/* minimal USB IRQ handler in DFU mode */
static __dfufunc void dfu_udp_irq(void)
{
	AT91PS_UDP pUDP = AT91C_BASE_UDP;
	AT91_REG isr = pUDP->UDP_ISR;

	if (isr & AT91C_UDP_ENDBUSRES) {
		pUDP->UDP_IER = AT91C_UDP_EPINT0;
		/* reset all endpoints */
		pUDP->UDP_RSTEP = (unsigned int)-1;
		pUDP->UDP_RSTEP = 0;
		/* Enable the function */
		pUDP->UDP_FADDR = AT91C_UDP_FEN;
		/* Configure endpoint 0 */
		pUDP->UDP_CSR[0] = (AT91C_UDP_EPEDS | AT91C_UDP_EPTYPE_CTRL);
		cur_config = 0;
	}

	if (isr & AT91C_UDP_EPINT0)
		dfu_udp_ep0_handler();

	/* clear all interrupts */
	pUDP->UDP_ICR = isr;

	AT91F_AIC_ClearIt(AT91C_BASE_AIC, AT91C_ID_UDP);
}

/* this is only called once before DFU mode, no __dfufunc required */
static void dfu_switch(void)
{
	AT91PS_AIC pAic = AT91C_BASE_AIC;

	DEBUGE("Switching to DFU mode ");

	pAic->AIC_SVR[AT91C_ID_UDP] = (unsigned int) &dfu_udp_irq;
	dfu_state = DFU_STATE_dfuIDLE;
}

void __dfufunc dfu_main(void)
{
	/*
	AT91F_AIC_ConfigureIt(AT91C_BASE_AIC, AT91C_ID_UDP,
			      OPENPCD_IRQ_PRIO_UDP,
			      AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL, dfu_udp_irq);
	*/

	AT91PS_AIC pAic = AT91C_BASE_AIC;
	pAic->AIC_IDCR = 1 << AT91C_ID_UDP;
	pAic->AIC_SVR[AT91C_ID_UDP] = (unsigned int) &dfu_udp_irq;
	pAic->AIC_SMR[AT91C_ID_UDP] = AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL | 
					OPENPCD_IRQ_PRIO_UDP;
	pAic->AIC_ICCR = 1 << AT91C_ID_UDP;

	AT91F_AIC_EnableIt(AT91C_BASE_AIC, AT91C_ID_UDP);

	/* End-of-Bus-Reset is always enabled */

	/* Clear for set the Pull up resistor */
	AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, OPENPCD_PIO_UDP_PUP);

	/* do nothing, since all of DFU is interrupt driven */
	while (1) ;
}

struct dfuapi __dfufunctab dfu_api = {
	.ep0_send_data		= &udp_ep0_send_data,
	.ep0_send_zlp		= &udp_ep0_send_zlp,
	.ep0_send_stall		= &udp_ep0_send_stall,
	.dfu_ep0_handler	= &dfu_ep0_handler,
	.dfu_switch		= &dfu_switch,
	.dfu_state		= &dfu_state,
	.dfu_dev_descriptor	= &dfu_dev_descriptor,
	.dfu_cfg_descriptor	= &dfu_cfg_descriptor,
};

int foo = 12345;
