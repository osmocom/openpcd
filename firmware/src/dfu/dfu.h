#ifndef _DFU_H
#define _DFU_H

/* USB Device Firmware Update Implementation for OpenPCD
 * (C) 2006 by Harald Welte <hwelte@hmw-consulting.de>
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
 
 */

#include <sys/types.h>
#include <usb_ch9.h>
#include <usb_dfu.h>

/* USB DFU functional descriptor */
#define DFU_FUNC_DESC  {						\
	.bLength		= USB_DT_DFU_SIZE,			\
	.bDescriptorType	= USB_DT_DFU,				\
	.bmAttributes		= USB_DFU_CAN_UPLOAD | USB_DFU_CAN_DOWNLOAD, \
	.wDetachTimeOut		= 0xff00,				\
	.wTransferSize		= AT91C_IFLASH_PAGE_SIZE,		\
	.bcdDFUVersion		= 0x0100,				\
}

/* USB Interface descriptor in Runtime mode */
#define DFU_RT_IF_DESC	{						\
	.bLength		= USB_DT_INTERFACE_SIZE,		\
	.bDescriptorType	= USB_DT_INTERFACE,			\
	.bInterfaceNumber	= 0x01,					\
	.bAlternateSetting	= 0x00,					\
	.bNumEndpoints		= 0x00,					\
	.bInterfaceClass	= 0xfe,					\
	.bInterfaceSubClass	= 0x01,					\
	.bInterfaceProtocol	= 0x01,					\
	.iInterface		= 1,					\
}

#define __dfufunctab  __attribute__ ((section (".dfu.functab")))
#define __dfudata __attribute__ ((section (".data.shared")))
#define __dfufunc 
#define __dfustruct const

#define DFU_API_LOCATION	((const struct dfuapi *) 0x00103fd0)

struct _dfu_desc {
	struct usb_config_descriptor ucfg;
	struct usb_interface_descriptor uif[2];
	struct usb_dfu_func_descriptor func_dfu;
};

struct dfuapi {
	void (*udp_init)(void);
	void (*ep0_send_data)(const char *data, u_int32_t len);
	void (*ep0_send_zlp)(void);
	void (*ep0_send_stall)(void);
	int  (*dfu_ep0_handler)(u_int8_t req_type, u_int8_t req,
				     u_int16_t val, u_int16_t len);
	void (*dfu_switch)(void);
	u_int32_t *dfu_state;
	const struct usb_device_descriptor *dfu_dev_descriptor;
	const struct _dfu_desc *dfu_cfg_descriptor;
};


#endif /* _DFU_H */
