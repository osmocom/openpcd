/* USB Device Firmware Update Implementation for OpenPCD
 * (C) 2006-2011 by Harald Welte <hwelte@hmw-consulting.de>
 *
 * This ought to be compliant to the USB DFU Spec 1.0 as available from
 * http://www.usb.org/developers/devclass_docs/usbdfu10.pdf
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
#include <usb_ch9.h>
#include <usb_dfu.h>
#include <board.h>
#include <lib_AT91SAM7.h>

#include <usb_strings_dfu.h>

#include <dfu/dfu.h>
#include <dfu/dbgu.h>
#include <os/flash.h>
#include <os/pcd_enumerate.h>
#include "../openpcd.h"

#include <compile.h>

#define SAM7DFU_SIZE	0x4000
#define SAM7DFU_RAM_SIZE	0x2000

/* If debug is enabled, we need to access debug functions from flash
 * and therefore have to omit flashing */
//#define DEBUG_DFU_NOFLASH

#ifdef DEBUG
#define DEBUG_DFU_EP0
//#define DEBUG_DFU_RECV
#endif

#ifdef DEBUG_DFU_EP0
#define DEBUGE DEBUGP
#else
#define DEBUGE(x, args ...)
#endif

#ifdef DEBUG_DFU_RECV
#define DEBUGR DEBUGP
#else
#define DEBUGR(x, args ...)
#endif

#define RET_NOTHING	0
#define RET_ZLP		1
#define RET_STALL	2

#define led1on()	AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, OPENPCD_PIO_LED1)
#define led1off()	AT91F_PIO_SetOutput(AT91C_BASE_PIOA, OPENPCD_PIO_LED1)

#define led2on()	AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, OPENPCD_PIO_LED2)
#define led2off()	AT91F_PIO_SetOutput(AT91C_BASE_PIOA, OPENPCD_PIO_LED2)

static int past_manifest = 0;
static int switch_to_ram = 0; /* IRQ handler requests main to jump to RAM */
static u_int16_t usb_if_nr = 0;	/* last SET_INTERFACE */
static u_int16_t usb_if_alt_nr = 0; /* last SET_INTERFACE AltSetting */
static u_int16_t usb_if_alt_nr_dnload = 0; /* AltSetting during last dnload */

static void __dfufunc udp_init(void)
{
	/* Set the PLL USB Divider */
	AT91C_BASE_CKGR->CKGR_PLLR |= AT91C_CKGR_USBDIV_1;

	/* Enables the 48MHz USB clock UDPCK and System Peripheral USB Clock */
	AT91C_BASE_PMC->PMC_SCER = AT91C_PMC_UDP;
	AT91C_BASE_PMC->PMC_PCER = (1 << AT91C_ID_UDP);

	/* Enable UDP PullUp (USB_DP_PUP) : enable & Clear of the
	 * corresponding PIO Set in PIO mode and Configure in Output */
#if defined(PCD)
	AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, OPENPCD_PIO_UDP_PUP);
#endif
	AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, OPENPCD_PIO_UDP_PUPv4);
}

static void __dfufunc udp_ep0_send_zlp(void);

/* Send Data through the control endpoint */
static void __dfufunc udp_ep0_send_data(const char *pData, u_int32_t length)
{
	AT91PS_UDP pUdp = AT91C_BASE_UDP;
	u_int32_t cpt = 0, len_remain = length;
	AT91_REG csr;

	DEBUGE("send_data: %u bytes ", length);

	do {
		cpt = MIN(len_remain, 8);
		len_remain -= cpt;

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

	} while (len_remain);

	if (pUdp->UDP_CSR[0] & AT91C_UDP_TXCOMP) {
		pUdp->UDP_CSR[0] &= ~(AT91C_UDP_TXCOMP);
		while (pUdp->UDP_CSR[0] & AT91C_UDP_TXCOMP) ;
	}

	if ((length % 8) == 0) {
		/* if the length is a multiple of the EP size, we need
		 * to send another ZLP (zero-length packet) to tell the
		 * host the transfer has completed.  */
		DEBUGE("set_txpktrdy_zlp ");
		udp_ep0_send_zlp();
	}
}

static void udp_ep0_recv_clean(void)
{
	unsigned int i;
	u_int8_t dummy;
	const AT91PS_UDP pUdp = AT91C_BASE_UDP;

	while (!(pUdp->UDP_CSR[0] & AT91C_UDP_RX_DATA_BK0)) ;

	for (i = 0; i < (pUdp->UDP_CSR[0] >> 16); i++)
		dummy = pUdp->UDP_FDR[0];

	pUdp->UDP_CSR[0] &= ~(AT91C_UDP_RX_DATA_BK0);
}

/* receive data from EP0 */
static int __dfufunc udp_ep0_recv_data(u_int8_t *data, u_int16_t len)
{
	AT91PS_UDP pUdp = AT91C_BASE_UDP;
	AT91_REG csr;
	u_int16_t i, num_rcv;
	u_int32_t num_rcv_total = 0;

	do {
		/* FIXME: do we need to check whether we've been interrupted
		 * by a RX SETUP stage? */
		do {
			csr = pUdp->UDP_CSR[0];
			DEBUGR("CSR=%08x ", csr);
		} while (!(csr & AT91C_UDP_RX_DATA_BK0)) ;
	
		num_rcv = pUdp->UDP_CSR[0] >> 16;

		/* make sure we don't read more than requested */
		if (num_rcv_total + num_rcv > len)
			num_rcv = num_rcv_total - len;

		DEBUGR("num_rcv = %u ", num_rcv);
		for (i = 0; i < num_rcv; i++)
			*data++ = pUdp->UDP_FDR[0];
		pUdp->UDP_CSR[0] &= ~(AT91C_UDP_RX_DATA_BK0);

		num_rcv_total += num_rcv;

		/* we need to continue to pull data until we either receive 
		 * a packet < endpoint size or == 0 */
	} while (num_rcv == 8 && num_rcv_total < len);

	DEBUGE("ep0_rcv_returning(%u total) ", num_rcv_total);

	return num_rcv_total;
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


static int first_download = 1;
static u_int8_t *ptr, *ptr_max;
static __dfudata u_int8_t dfu_status;
__dfudata u_int32_t dfu_state = DFU_STATE_appIDLE;
static u_int32_t pagebuf32[AT91C_IFLASH_PAGE_SIZE/4];

static void chk_first_dnload_set_ptr(void)
{
	if (!first_download)
		return;

	switch (usb_if_alt_nr) {
	case 0:
		ptr = (u_int8_t *) AT91C_IFLASH + SAM7DFU_SIZE;
		ptr_max = AT91C_IFLASH + AT91C_IFLASH_SIZE - ENVIRONMENT_SIZE;
		break;
	case 1:
		ptr = (u_int8_t *) AT91C_IFLASH;
		ptr_max = AT91C_IFLASH + SAM7DFU_SIZE;
		break;
	case 2:
		ptr = (u_int8_t *) AT91C_ISRAM + SAM7DFU_RAM_SIZE;
		ptr_max = AT91C_ISRAM + AT91C_ISRAM_SIZE;
		break;
	}
	first_download = 0;
}

static int __dfufunc handle_dnload_flash(u_int16_t val, u_int16_t len)
{
	volatile u_int32_t *p;
	u_int8_t *pagebuf = (u_int8_t *) pagebuf32;
	int i;

	DEBUGE("download ");

	if (len > AT91C_IFLASH_PAGE_SIZE) {
		/* Too big. Not that we'd really care, but it's a
		 * DFU protocol violation */
		DEBUGP("length exceeds flash page size ");
	    	dfu_state = DFU_STATE_dfuERROR;
		dfu_status = DFU_STATUS_errADDRESS;
		return RET_STALL;
	}
	if (len & 0x3) {
		/* reject non-four-byte-aligned writes */
		DEBUGP("not four-byte-aligned length ");
		dfu_state = DFU_STATE_dfuERROR;
		dfu_status = DFU_STATUS_errADDRESS;
		return RET_STALL;
	}
	chk_first_dnload_set_ptr();
	p = (volatile u_int32_t *)ptr;

	if (len == 0) {
		DEBUGP("zero-size write -> MANIFEST_SYNC ");
		if (((unsigned long)p % AT91C_IFLASH_PAGE_SIZE) != 0)
			flash_page(p);
		dfu_state = DFU_STATE_dfuMANIFEST_SYNC;
		first_download = 1;
		return RET_ZLP;
	}

	/* check if we would exceed end of memory */
	if (ptr + len > ptr_max) {
		DEBUGP("end of write exceeds flash end ");
		dfu_state = DFU_STATE_dfuERROR;
		dfu_status = DFU_STATUS_errADDRESS;
		return RET_STALL;
	}

	DEBUGP("try_to_recv=%u ", len);
	udp_ep0_recv_data(pagebuf, len);

	DEBUGR(hexdump(pagebuf, len));

#ifndef DEBUG_DFU_NOFLASH
	DEBUGP("copying ");
	/* we can only access the write buffer with correctly aligned
	 * 32bit writes ! */
	for (i = 0; i < len/4; i++) {
		*p++ = pagebuf32[i];
		/* If we have filled a page buffer, flash it */
		if (((unsigned long)p % AT91C_IFLASH_PAGE_SIZE) == 0) {
			DEBUGP("page_full  ");
			flash_page(p-1);
		}
	}
	ptr = (u_int8_t *) p;
#endif

	return RET_ZLP;
}

static int __dfufunc handle_dnload_ram(u_int16_t val, u_int16_t len)
{
	DEBUGE("download ");

	if (len > AT91C_IFLASH_PAGE_SIZE) {
		/* Too big. Not that we'd really care, but it's a
		 * DFU protocol violation */
		DEBUGP("length exceeds flash page size ");
		dfu_state = DFU_STATE_dfuERROR;
		dfu_status = DFU_STATUS_errADDRESS;
		return RET_STALL;
	}
	chk_first_dnload_set_ptr();

	if (len == 0) {
		DEBUGP("zero-size write -> MANIFEST_SYNC ");
		dfu_state = DFU_STATE_dfuMANIFEST_SYNC;
		first_download = 1;
		return RET_ZLP;
	}

	/* check if we would exceed end of memory */
	if (ptr + len >= ptr_max) {
		DEBUGP("end of write exceeds RAM end ");
		dfu_state = DFU_STATE_dfuERROR;
		dfu_status = DFU_STATUS_errADDRESS;
		return RET_STALL;
	}

	/* drectly copy into RAM */
	DEBUGP("try_to_recv=%u ", len);
	udp_ep0_recv_data(ptr, len);

	DEBUGR(hexdump(ptr, len));

	ptr += len;

	return RET_ZLP;
}

static int __dfufunc handle_dnload(u_int16_t val, u_int16_t len)
{
	usb_if_alt_nr_dnload = usb_if_alt_nr;
	switch (usb_if_alt_nr) {
	case 2:
		return handle_dnload_ram(val, len);
	default:
		return handle_dnload_flash(val, len);
	}
}

#define AT91C_IFLASH_END ((u_int8_t *)AT91C_IFLASH + AT91C_IFLASH_SIZE)
static __dfufunc int handle_upload(u_int16_t val, u_int16_t len)
{
	DEBUGE("upload ");
	if (len > AT91C_IFLASH_PAGE_SIZE) {
		/* Too big */
		dfu_state = DFU_STATE_dfuERROR;
		dfu_status = DFU_STATUS_errADDRESS;
		udp_ep0_send_stall();
		return -EINVAL;
	}
	chk_first_dnload_set_ptr();

	if (ptr + len > AT91C_IFLASH_END) {
		len = AT91C_IFLASH_END - (u_int8_t *)ptr;
		first_download = 1;
	}

	udp_ep0_send_data((char *)ptr, len);
	ptr+= len;

	return len;
}

static __dfufunc void handle_getstatus(void)
{
	struct dfu_status dstat;
	u_int32_t fsr = AT91F_MC_EFC_GetStatus(AT91C_BASE_MC);

	DEBUGE("getstatus(fsr=0x%08x) ", fsr);

	switch (dfu_state) {
	case DFU_STATE_dfuDNLOAD_SYNC:
	case DFU_STATE_dfuDNBUSY:
		if (fsr & AT91C_MC_PROGE) {
			DEBUGE("errPROG ");
			dfu_status = DFU_STATUS_errPROG;
			dfu_state = DFU_STATE_dfuERROR;
		} else if (fsr & AT91C_MC_LOCKE) {
			DEBUGE("errWRITE ");
			dfu_status = DFU_STATUS_errWRITE;
			dfu_state = DFU_STATE_dfuERROR;
		} else if (fsr & AT91C_MC_FRDY) {
			DEBUGE("DNLOAD_IDLE ");
			dfu_state = DFU_STATE_dfuDNLOAD_IDLE;
		} else {
			DEBUGE("DNBUSY ");
			dfu_state = DFU_STATE_dfuDNBUSY;
		}
		break;
	case DFU_STATE_dfuMANIFEST_SYNC:
		dfu_state = DFU_STATE_dfuMANIFEST;
		break;
	}

	/* send status response */
	dstat.bStatus = dfu_status;
	dstat.bState = dfu_state;
	dstat.iString = 0;
	/* FIXME: set dstat.bwPollTimeout */

	udp_ep0_send_data((char *)&dstat, sizeof(dstat));
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
	int rc, ret = RET_NOTHING;

	DEBUGE("old_state = %u ", dfu_state);

	switch (dfu_state) {
	case DFU_STATE_appIDLE:
		switch (req) {
		case USB_REQ_DFU_GETSTATUS:
			handle_getstatus();
			break;
		case USB_REQ_DFU_GETSTATE:
			handle_getstate();
			break;
		case USB_REQ_DFU_DETACH:
			dfu_state = DFU_STATE_appDETACH;
			ret = RET_ZLP;
			goto out;
			break;
		default:
			ret = RET_STALL;
		}
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
			ret = RET_STALL;
			goto out;
			break;
		}
		/* FIXME: implement timer to return to appIDLE */
		break;
	case DFU_STATE_dfuIDLE:
		switch (req) {
		case USB_REQ_DFU_DNLOAD:
			if (len == 0) {
				dfu_state = DFU_STATE_dfuERROR;
				ret = RET_STALL;
				goto out;
			}
			dfu_state = DFU_STATE_dfuDNLOAD_SYNC;
			ptr = (u_int8_t *) AT91C_IFLASH + SAM7DFU_SIZE;
			ret = handle_dnload(val, len);
			break;
		case USB_REQ_DFU_UPLOAD:
			ptr = (u_int8_t *) AT91C_IFLASH + SAM7DFU_SIZE;
			dfu_state = DFU_STATE_dfuUPLOAD_IDLE;
			handle_upload(val, len);
			break;
		case USB_REQ_DFU_ABORT:
			/* no zlp? */
			ret = RET_ZLP;
			break;
		case USB_REQ_DFU_GETSTATUS:
			handle_getstatus();
			break;
		case USB_REQ_DFU_GETSTATE:
			handle_getstate();
			break;
		default:
			dfu_state = DFU_STATE_dfuERROR;
			ret = RET_STALL;
			goto out;
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
			ret = RET_STALL;
			goto out;
		}
		break;
	case DFU_STATE_dfuDNBUSY:
		switch (req) {
		case USB_REQ_DFU_GETSTATUS:
			/* FIXME: only accept getstatus if bwPollTimeout
			 * has elapsed */
			handle_getstatus();
			break;
		default:
			dfu_state = DFU_STATE_dfuERROR;
			ret = RET_STALL;
			goto out;
		}
		break;
	case DFU_STATE_dfuDNLOAD_IDLE:
		switch (req) {
		case USB_REQ_DFU_DNLOAD:
			dfu_state = DFU_STATE_dfuDNLOAD_SYNC;
			ret = handle_dnload(val, len);
			break;
		case USB_REQ_DFU_ABORT:
			dfu_state = DFU_STATE_dfuIDLE;
			ret = RET_ZLP;
			break;
		case USB_REQ_DFU_GETSTATUS:
			handle_getstatus();
			break;
		case USB_REQ_DFU_GETSTATE:
			handle_getstate();
			break;
		default:
			dfu_state = DFU_STATE_dfuERROR;
			ret = RET_STALL;
			break;
		}
		break;
	case DFU_STATE_dfuMANIFEST_SYNC:
		switch (req) {
		case USB_REQ_DFU_GETSTATUS:
			handle_getstatus();
			break;
		case USB_REQ_DFU_GETSTATE:
			handle_getstate();
			break;
		default:
			dfu_state = DFU_STATE_dfuERROR;
			ret = RET_STALL;
			break;
		}
		break;
	case DFU_STATE_dfuMANIFEST:
		switch (req) {
		case USB_REQ_DFU_GETSTATUS:
			/* we don't want to change to WAIT_RST, as it
			 * would mean that we can not support another
			 * DFU transaction before doing the actual
			 * reset.  Instead, we switch to idle and note
			 * that we've already been through MANIFST in
			 * the global variable 'past_manifest'.
			 */
			//dfu_state = DFU_STATE_dfuMANIFEST_WAIT_RST;
			dfu_state = DFU_STATE_dfuIDLE;
			past_manifest = 1;
			handle_getstatus();
			break;
		case USB_REQ_DFU_GETSTATE:
			handle_getstate();
			break;
		default:
			dfu_state = DFU_STATE_dfuERROR;
			ret = RET_STALL;
			break;
		}
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
			ret = RET_ZLP;
			break;
		case USB_REQ_DFU_GETSTATUS:
			handle_getstatus();
			break;
		case USB_REQ_DFU_GETSTATE:
			handle_getstate();
			break;
		default:
			dfu_state = DFU_STATE_dfuERROR;
			ret = RET_STALL;
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
			dfu_status = DFU_STATUS_OK;
			/* no zlp? */
			ret = RET_ZLP;
			break;
		default:
			dfu_state = DFU_STATE_dfuERROR;
			ret = RET_STALL;
			break;
		}
		break;
	}

out:
	DEBUGE("new_state = %u\r\n", dfu_state);

	switch (ret) {
	case RET_NOTHING:
		break;
	case RET_ZLP:
		udp_ep0_send_zlp();
		break;
	case RET_STALL:
		udp_ep0_send_stall();
		break;
	}
	return 0;
}

static u_int8_t cur_config;

/* USB DFU Device descriptor in DFU mode */
__dfustruct const struct usb_device_descriptor dfu_dev_descriptor = {
	.bLength		= USB_DT_DEVICE_SIZE,
	.bDescriptorType	= USB_DT_DEVICE,
	.bcdUSB			= 0x0100,
	.bDeviceClass		= 0x00,
	.bDeviceSubClass	= 0x00,
	.bDeviceProtocol	= 0x00,
	.bMaxPacketSize0	= 8,
	.idVendor		= USB_VENDOR_ID,
	.idProduct		= USB_PRODUCT_ID,
	.bcdDevice		= 0x0000,
#ifdef CONFIG_USB_STRING
	.iManufacturer		= 1,
	.iProduct		= 2,
#else
	.iManufacturer		= 0,
	.iProduct		= 0,
#endif
	.iSerialNumber		= 0x00,
	.bNumConfigurations	= 0x01,
};

/* USB DFU Config descriptor in DFU mode */
__dfustruct const struct _dfu_desc dfu_cfg_descriptor = {
	.ucfg = {
		.bLength = USB_DT_CONFIG_SIZE,
		.bDescriptorType = USB_DT_CONFIG,
		.wTotalLength = USB_DT_CONFIG_SIZE + 
				3* USB_DT_INTERFACE_SIZE +
				USB_DT_DFU_SIZE,
		.bNumInterfaces = 1,
		.bConfigurationValue = 1,
#ifdef CONFIG_USB_STRING
		.iConfiguration = 3,
#else
		.iConfiguration = 0,
#endif
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
#ifdef CONFIG_USB_STRING
		.iInterface		= 4,
#else
		.iInterface		= 0,
#endif
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
#ifdef CONFIG_USB_STRING
		.iInterface		= 5,
#else
		.iInterface		= 0,
#endif
		}, 
	.uif[2] = {
		.bLength		= USB_DT_INTERFACE_SIZE,
		.bDescriptorType	= USB_DT_INTERFACE,
		.bInterfaceNumber	= 0x00,
		.bAlternateSetting	= 0x02,
		.bNumEndpoints		= 0x00,
		.bInterfaceClass	= 0xfe,
		.bInterfaceSubClass	= 0x01,
		.bInterfaceProtocol	= 0x02,
#ifdef CONFIG_USB_STRING
		.iInterface		= 6,
#else
		.iInterface		= 0,
#endif
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
		DEBUGE("no setup packet\r\n");
		return;
	}

	DEBUGE("len=%d ", csr >> 16);
	if (csr >> 16  == 0) {
		DEBUGE("empty packet\r\n");
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
		u_int8_t desc_type, desc_index;
	case STD_GET_DESCRIPTOR:
		DEBUGE("GET_DESCRIPTOR ");
		desc_type = wValue >> 8;
		desc_index = wValue & 0xff;
		switch (desc_type) {
		case USB_DT_DEVICE:
			/* Return Device Descriptor */
			udp_ep0_send_data((const char *) 
					  &dfu_dev_descriptor,
					  MIN(sizeof(dfu_dev_descriptor),
					      wLength));
			break;
		case USB_DT_CONFIG:
			/* Return Configuration Descriptor */
			udp_ep0_send_data((const char *)
					  &dfu_cfg_descriptor,
					  MIN(sizeof(dfu_cfg_descriptor),
					      wLength));
			break;
		case USB_DT_STRING:
			/* Return String Descriptor */
			if (desc_index > ARRAY_SIZE(usb_strings)) {
				udp_ep0_send_stall();
				break;
			}
			DEBUGE("bLength=%u, wLength=%u ", 
				usb_strings[desc_index]->bLength, wLength);
			udp_ep0_send_data((const char *) usb_strings[desc_index],
					  MIN(usb_strings[desc_index]->bLength, 
					      wLength));
			break;
		case USB_DT_CS_DEVICE:
			/* Return Function descriptor */
			udp_ep0_send_data((const char *) &dfu_cfg_descriptor.func_dfu,
					  MIN(sizeof(dfu_cfg_descriptor.func_dfu),
					      wLength));
			break;
		default:
			udp_ep0_send_stall();
			break;
		}
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
		DEBUGE("SET INTERFACE(if=%d, alt=%d) ", wIndex, wValue);
		/* store the interface number somewhere, once
		 * we need to support DFU flashing DFU */
		usb_if_alt_nr = wValue;
		usb_if_nr = wIndex;
		udp_ep0_send_zlp();
		break;
	default:
		DEBUGE("DEFAULT(req=0x%02x, type=0x%02x) ",
			bRequest, bmRequestType);
		if ((bmRequestType & 0x3f) == USB_TYPE_DFU) {
			dfu_ep0_handler(bmRequestType, bRequest,
					wValue, wLength);
		} else
			udp_ep0_send_stall();
		break;
	}
	DEBUGE("\r\n");
}

/* minimal USB IRQ handler in DFU mode */
static __dfufunc void dfu_udp_irq(void)
{
	AT91PS_UDP pUDP = AT91C_BASE_UDP;
	AT91_REG isr = pUDP->UDP_ISR;
	led1on();

	if (isr & AT91C_UDP_ENDBUSRES) {
		led2on();
		pUDP->UDP_IER = AT91C_UDP_EPINT0;
		/* reset all endpoints */
		pUDP->UDP_RSTEP = (unsigned int)-1;
		pUDP->UDP_RSTEP = 0;
		/* Enable the function */
		pUDP->UDP_FADDR = AT91C_UDP_FEN;
		/* Configure endpoint 0 */
		pUDP->UDP_CSR[0] = (AT91C_UDP_EPEDS | AT91C_UDP_EPTYPE_CTRL);
		cur_config = 0;

		if (dfu_state == DFU_STATE_dfuMANIFEST_WAIT_RST ||
		    dfu_state == DFU_STATE_dfuMANIFEST ||
		    past_manifest) {
			AT91F_DBGU_Printk("sam7dfu: switching to APP mode\r\n");
			switch (usb_if_alt_nr_dnload) {
			case 2:
				switch_to_ram = 1;
				break;
			default:
				/* reset back into the main application */
				AT91F_RSTSoftReset(AT91C_BASE_RSTC,
							  AT91C_RSTC_PROCRST|
					   		  AT91C_RSTC_PERRST|
							  AT91C_RSTC_EXTRST);
				break;
			}
		}
	}

	if (isr & AT91C_UDP_EPINT0)
		dfu_udp_ep0_handler();

	/* clear all interrupts */
	pUDP->UDP_ICR = isr;

	AT91F_AIC_ClearIt(AT91C_BASE_AIC, AT91C_ID_UDP);

	led1off();
}

/* this is only called once before DFU mode, no __dfufunc required */
static void dfu_switch(void)
{
	AT91PS_AIC pAic = AT91C_BASE_AIC;

	DEBUGE("\r\nsam7dfu: switching to DFU mode\r\n");

	dfu_state = DFU_STATE_appDETACH;
	AT91F_RSTSoftReset(AT91C_BASE_RSTC, AT91C_RSTC_PROCRST|
			   AT91C_RSTC_PERRST|AT91C_RSTC_EXTRST);

	/* We should never reach here, but anyway avoid returning to the
	 * caller since he doesn't expect us to do so */
	while (1) ;
}

void __dfufunc dfu_main(void)
{
	AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, OPENPCD_PIO_LED1);
	AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, OPENPCD_PIO_LED2);
	led1off();
	led2off();

	AT91F_DBGU_Init();
	AT91F_DBGU_Printk("\n\r\n\rsam7dfu - AT91SAM7 USB DFU bootloader\n\r"
		 "(C) 2006-2011 by Harald Welte <hwelte@hmw-consulting.de>\n\r"
		 "This software is FREE SOFTWARE licensed under GNU GPL\n\r");
	AT91F_DBGU_Printk("Version " COMPILE_SVNREV 
			  " compiled " COMPILE_DATE 
			  " by " COMPILE_BY "\n\r\n\r");

	udp_init();

	dfu_state = DFU_STATE_dfuIDLE;

	/* This implements 
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
#if defined(PCD)
	AT91F_PIO_SetOutput(AT91C_BASE_PIOA, OPENPCD_PIO_UDP_PUP);
#endif
	AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, OPENPCD_PIO_UDP_PUPv4);

	flash_init();

	AT91F_DBGU_Printk("You may now start the DFU up/download process\r\n");
	/* do nothing, since all of DFU is interrupt driven */
	int i = 0;
	while (1) {
	    /* Occasionally reset watchdog */
	    i = (i+1) % 10000;
	    if( i== 0) {
		AT91F_WDTRestart(AT91C_BASE_WDTC);
	    }
	    if (switch_to_ram) {
		void (*ram_app_entry)(void);
		int i;
		for (i = 0; i < 32; i++)
			AT91F_AIC_DisableIt(AT91C_BASE_AIC, i);
		/* jump into RAM */
		AT91F_DBGU_Printk("JUMP TO RAM\r\n");
		ram_app_entry = AT91C_ISRAM + SAM7DFU_RAM_SIZE;
		ram_app_entry();
	    }
	}
}

const struct dfuapi __dfufunctab dfu_api = {
	.udp_init		= &udp_init,
	.ep0_send_data		= &udp_ep0_send_data,
	.ep0_send_zlp		= &udp_ep0_send_zlp,
	.ep0_send_stall		= &udp_ep0_send_stall,
	.dfu_ep0_handler	= &dfu_ep0_handler,
	.dfu_switch		= &dfu_switch,
	.dfu_state		= &dfu_state,
	.dfu_dev_descriptor	= &dfu_dev_descriptor,
	.dfu_cfg_descriptor	= &dfu_cfg_descriptor,
};

/* just for testing */
int foo = 12345;
