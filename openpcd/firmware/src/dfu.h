#ifndef _USB_DFU_H
#define _USB_DFU_H

#define USB_DT_DFU			0x21

struct usb_dfu_func_descriptor {
	__u8		bLength;
	__u8		bDescriptorType;
	__u8		bmAttributs;
#define USB_DFU_CAN_DOWNLOAD	(1 << 0)
#define USB_DFU_CAN_UPLOAD	(1 << 1)
#define USB_DFU_MANIFEST_TOL	(1 << 2)
#define USB_DFU_WILL_DETACH	(1 << 3)
	__le16		wDetachTimeOut;
	__le16		wTransferSize;
	__le16		bcdDFUVersion;
} __attribute__ ((packed));

#define USB_DT_DFU_SIZE			9


/* DFU class-specific requests (Section 3, DFU Rev 1.1) */
#define USB_REQ_DFU_DETACH	0x00
#define USB_REQ_DFU_DNLOAD	0x01
#define USB_REQ_DFU_UPLOAD	0x02
#define USB_REQ_DFU_GETSTATUS	0x03
#define USB_REQ_DFU_CLRSTATUS	0x04
#define USB_REQ_DFU_GETSTATE	0x05
#define USB_REQ_DFU_ABORT	0x06

#endif /* _USB_DFU_H
