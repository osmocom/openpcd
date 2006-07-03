
/* USB Interface descriptor in Runtime mode */
struct usb_interface_descriptor desc_if_rt = {
	.bLength		= USB_DT_INTERFACE_SIZE,
	.bDescriptorType	= USB_DT_INTERFACE,
	.bInterfaceNumber	= 0x01,
	.bAlternateSetting	= 0x00,
	.bNumEndpoints		= 0x00,
	.bInterfaceClass	= 0xfe,
	.bInterfaceSubClass	= 0x01,
	.bInterfaceProtocol	= 0x01,
	.iInterface		= FIXME,
};

/* USB DFU Device descriptor in DFU mode */
struct usb_device_descriptor desc_dev_dfu = {
	.bLength		= USB_DT_DEVICE_SIZE,
	.bDescriptorType	= USB_DT_DEVICE,
	.bcdUSB			= 0x0100,
	.bDeviceClass		= 0x00,
	.bDeviceSubClass	= 0x00,
	.bDeviceProtocol	= 0x00,
	.bMaxPacketSize0	= 8,
	.idVendor		= USB_VENDOR,
	.idProtuct		= USB_PRODUCT,
	.bcdDevice		= 0x0000,
	.iManufacturer		= FIXME,
	.iProduct		= FIXME,
	.iSerialNumber		= FIXME,
	.bNumConfigurations	= 0x01,
};

/* USB DFU Interface descriptor in DFU mode */
struct usb_interface_descriptor desc_if_dfu = {
	.bLength		= USB_DT_INTERFACE_SIZE,
	.bDescriptorType	= USB_DT_INTERFACE,
	.bInterfaceNumber	= 0x00,
	.bAlternateSetting	= 0x00,
	.bNumEndpoints		= 0x00,
	.bInterfaceClass	= 0xfe,
	.bInterfaceSubClass	= 0x01,
	.bInterfaceProtocol	= 0x02,
	.iInterface		= FIXME,
};


{
	switch () {
		case USB_REQ_DFU_DETACH:
			break;
		case USB_REQ_DFU_DNLOAD:
			break;
		case USB_REQ_DFU_GETSTATUS:
			break;
		case USB_REQ_DFU_CLRSTATUS:
			break;
		case USB_REQ_DFU_ABORT:
			break;
		case USB_REQ_GETSTATE:
			break;
		case USB_REQ_DFU_UPLOAD:
			break;
	}
}
