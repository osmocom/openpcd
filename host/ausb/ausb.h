#ifndef _AUSB_H
#define _AUSB_H

/* Wrapper/Extension code to libusb-0.1 to support asynchronous requests
 * on Linux platforns 
 *
 * (C) 2004 by Harald Welte <laforge@gnumonks.org>
 *
 * Distributed under the terms of GNU LGPL, Version 2.1
 */


#include <usb.h>
#include <linux/usbdevice_fs.h>

#define AUSB_USBDEVFS_URB_TYPES	4

/* structures */
struct ausb_callback {
	void (*handler)(struct usbdevfs_urb *uurb, void *userdata);
	void *userdata;
};

struct ausb_dev_handle {
	usb_dev_handle *uh;
	struct ausb_callback cb[AUSB_USBDEVFS_URB_TYPES];
};

typedef struct ausb_dev_handle ausb_dev_handle;

/* intitialization */ 
int ausb_init(void);
ausb_dev_handle *ausb_open(struct usb_device *dev);
int ausb_close(ausb_dev_handle *ah);
int ausb_register_callback(ausb_dev_handle *ah, unsigned char type,
			   void (*callback)(struct usbdevfs_urb *uurb,
					    void *userdata),
			   void *userdata);

/* asynchronous URB related functions */
void ausb_dump_urb(struct usbdevfs_urb *uurb);
void ausb_fill_int_urb(struct usbdevfs_urb *uurb, unsigned char endpoint,
		      void *buffer, int buffer_length);
int ausb_submit_urb(ausb_dev_handle *ah, struct usbdevfs_urb *uurb);
int ausb_discard_urb(ausb_dev_handle *ah, struct usbdevfs_urb *uurb);
struct usbdevfs_urb *ausb_get_urb(ausb_dev_handle *ah);

/* synchronous functions, mostly wrappers for libusb */
int ausb_claim_interface(ausb_dev_handle *ah, int interface);
int ausb_release_interface(ausb_dev_handle *ah, int interface);
int ausb_set_configuration(ausb_dev_handle *dev, int configuration);
int ausb_clear_halt(ausb_dev_handle *dev, unsigned int ep);
int ausb_reset(ausb_dev_handle *dev);
int ausb_resetep(ausb_dev_handle *dev, int ep);
int ausb_bulk_write(ausb_dev_handle *ah, int ep, char *bytes, int size, 
		    int timeout);
int ausb_bulk_read(ausb_dev_handle *ah, int ep, char *bytes, int size, 
		   int timeout);
#ifdef LIBUSB_HAS_GET_DRIVER_NP
int ausb_get_driver_np(ausb_dev_handle *ah, int interface, char *name,
		       unsigned int namelen);
#endif
#ifdef LIBUSB_HAS_DETACH_KERNEL_DRIVER_NP
int ausb_detach_kernel_driver_np(ausb_dev_handle *dev, int interface);
int ausb_reattach_kernel_driver_np(ausb_dev_handle *dev, int interface);
#endif

#endif /* _AUSB_H */
