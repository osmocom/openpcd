#ifndef _AUSB_USB_H
#define _AUSB_USB_H

#ifndef __user
#define __user
#endif/*__user*/

int __usb_bulk_write(usb_dev_handle *dev, int ep, char *bytes, int length,
		     int timeout);
int __usb_bulk_read(usb_dev_handle *dev, int ep, char *bytes, int length,
		    int timeout);
int __usb_reattach_kernel_driver_np(usb_dev_handle *dev, int interface);
#endif
