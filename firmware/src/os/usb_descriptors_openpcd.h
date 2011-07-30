#ifndef _USB_DESCRIPTORS_H
#define _USB_DESCRIPTORS_H

#include <usb_ch9.h>
#include <sys/types.h>
#include <openpcd.h>
#include <dfu/dfu.h>

#include "../config.h"

const struct usb_device_descriptor dev_descriptor = {
	.bLength = USB_DT_DEVICE_SIZE,
	.bDescriptorType = USB_DT_DEVICE,
	.bcdUSB = 0x0100,
	.bDeviceClass = USB_CLASS_VENDOR_SPEC,
	.bDeviceSubClass = 0xff,
	.bDeviceProtocol = 0xff,
	.bMaxPacketSize0 = 0x08,
	.idVendor = USB_VENDOR_ID,
	.idProduct = USB_PRODUCT_ID,
	.bcdDevice = 0x0030,	/* Version 0.3 */
#ifdef CONFIG_USB_STRING
	.iManufacturer = 4,
	.iProduct = 5,
	.iSerialNumber = 0,
#else
	.iManufacturer = 0,
	.iProduct = 0,
	.iSerialNumber = 0,
#endif
	.bNumConfigurations = 0x01,
};

struct _desc {
	struct usb_config_descriptor ucfg;
	struct usb_interface_descriptor uif;
	struct usb_endpoint_descriptor ep[3];
#ifdef CONFIG_DFU
	struct usb_interface_descriptor uif_dfu[3];
#endif
};

const struct _desc cfg_descriptor = {
	.ucfg = {
		 .bLength = USB_DT_CONFIG_SIZE,
		 .bDescriptorType = USB_DT_CONFIG,
		 .wTotalLength = USB_DT_CONFIG_SIZE +
#ifdef CONFIG_DFU
		 		 4 * USB_DT_INTERFACE_SIZE + 
				 3 * USB_DT_ENDPOINT_SIZE,
		 .bNumInterfaces = 4,
#else
		 		 1 * USB_DT_INTERFACE_SIZE + 
				 3 * USB_DT_ENDPOINT_SIZE,
		 .bNumInterfaces = 1,
#endif
		 .bConfigurationValue = 1,
#ifdef CONFIG_USB_STRING
		 .iConfiguration = 6,
#else
		 .iConfiguration = 0,
#endif
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
#ifdef CONFIG_USB_STRING
		.iInterface = 7,
#else
		.iInterface = 0,
#endif
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

#endif /* _USB_DESCRIPTORS_H */
