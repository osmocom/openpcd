#ifndef _DFU_H
#define _DFU_H

/* USB Device Firmware Update Implementation for OpenPCD
 * (C) 2006 by Harald Welte <hwelte@hmw-consulting.de>
 *
 * This ought to be compliant to the USB DFU Spec 1.0 as available from
 * http://www.usb.org/developers/devclass_docs/usbdfu10.pdf
 *
 */

#include <sys/types.h>
#include <usb_ch9.h>
#include <usb_dfu.h>

#include "dbgu.h"

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

#define __dfufunc __attribute__ ((long_call, section (".dfu.func")))
#define __dfustruct __attribute__ ((section (".dfu.struct"))) const
#define __dfufunctab  __attribute__ ((section (".dfu.functab")))
	
#if 0
extern void __dfufunc udp_ep0_send_data(const char *data, u_int32_t length);
extern void __dfufunc udp_ep0_send_zlp(void);
extern void __dfufunc udp_ep0_send_stall(void);
extern __dfustruct struct usb_device_descriptor dfu_dev_descriptor;
extern __dfustruct struct _dfu_desc dfu_cfg_descriptor;
extern void dfu_switch(void);
extern int __dfufunc dfu_ep0_handler(u_int8_t req_type, u_int8_t req,
				     u_int16_t val, u_int16_t len);
extern static u_int8_t dfu_state;
struct udp_pcd;
#endif


extern void udp_init(void);

struct _dfu_desc {
	struct usb_config_descriptor ucfg;
	struct usb_interface_descriptor uif[2];
	struct usb_dfu_func_descriptor func_dfu;
};

struct dfuapi {
	void (*ep0_send_data)(const char *data, u_int32_t len);
	void (*ep0_send_zlp)(void);
	void (*ep0_send_stall)(void);
	int  (*dfu_ep0_handler)(u_int8_t req_type, u_int8_t req,
				     u_int16_t val, u_int16_t len);
	void (*dfu_switch)(void);
	u_int8_t *dfu_state;
	const struct usb_device_descriptor *dfu_dev_descriptor;
	const struct _dfu_desc *dfu_cfg_descriptor;
};


#endif /* _DFU_H */
