/* Wrapper/Extension code to libusb-0.1 to support asynchronous requests
 * on Linux platforns 
 *
 * (C) 2004-2005 by Harald Welte <laforge@gnumonks.org>
 *
 * Distributed and licensed under the terms of GNU LGPL, Version 2.1
 */

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <sys/utsname.h>
#include <sys/ioctl.h>

#include "ausb.h"
#include "usb.h"

#ifdef DEBUG_AUSB
#define DEBUGP(x, args...)	fprintf(stderr, "%s:%s():%u " x, __FILE__, \
					__FUNCTION__, __LINE__, ## args)
#else
#define DEBUGP(x, args...);
#endif

#define SIGAUSB		(SIGRTMIN+1)

static int ausb_get_fd(ausb_dev_handle *ah)
{
	return *((int *)ah->uh);
}


static int kernel_2_5;

/* FIXME: this has to go */
static struct ausb_dev_handle *last_handle = NULL;

static int is_kernel_2_5()
{
	struct utsname uts;
	int ret;

	uname(&uts);

	ret = (strncmp(uts.release, "2.5.", 4) == 0) ||
	       (strncmp(uts.release, "2.6.", 4) == 0);

	return ret;
}

void ausb_dump_urb(struct usbdevfs_urb *uurb)
{
	DEBUGP("urb(%p): type=%u, endpoint=0x%x, status=%d, flags=0x%x, number_of_packets=%d, error_count=%d\n", uurb, uurb->type, uurb->endpoint, uurb->status, uurb->flags, uurb->number_of_packets, uurb->error_count);
}

void ausb_fill_int_urb(struct usbdevfs_urb *uurb, unsigned char endpoint,
		      void *buffer, int buffer_length)
				    
{
	uurb->type = kernel_2_5 ? USBDEVFS_URB_TYPE_INTERRUPT : USBDEVFS_URB_TYPE_BULK;
	uurb->endpoint = endpoint; /* | USB_DIR_IN; */
	uurb->flags = kernel_2_5 ? 0 : 1 ; /* USBDEVFS_URB_QUEUE_BULK; */
	uurb->buffer = buffer;
	uurb->buffer_length = buffer_length;
	uurb->signr = SIGAUSB;
	uurb->start_frame = -1;
}

int ausb_submit_urb(ausb_dev_handle *ah, struct usbdevfs_urb *uurb)
{
	int ret;

	DEBUGP("ah=%p\n", ah);
	ausb_dump_urb(uurb);

	/* save ausb_dev_handle in opaque usercontext field */
	uurb->usercontext = ah;

	do {
		ret = ioctl(ausb_get_fd(ah), USBDEVFS_SUBMITURB, uurb);
	} while (ret < 0 && errno == EINTR);

	return ret;
}

int ausb_discard_urb(ausb_dev_handle *ah, struct usbdevfs_urb *uurb)
{
	int ret;

	DEBUGP("ah=%p, uurb=%p\n");
	ausb_dump_urb(uurb);

	do {
		ret = ioctl(ausb_get_fd(ah), USBDEVFS_DISCARDURB, uurb);
	} while (ret < 0 && errno == EINTR);

	return ret;
}

struct usbdevfs_urb *ausb_get_urb(ausb_dev_handle *ah)
{
	int ret;
	struct usbdevfs_urb *uurb;

	DEBUGP("entering\n");

	do {
		ret = ioctl(ausb_get_fd(ah), USBDEVFS_REAPURBNDELAY, &uurb);
	} while (ret < 0 && errno == EINTR);

	if (ret < 0 && errno == EAGAIN) {
		DEBUGP("ioctl returned %d (errno=%s)\n", ret,
			strerror(errno));
		return NULL;
	}

	DEBUGP("returning %p\n", uurb);
	return uurb;
}

static void handle_urb(struct usbdevfs_urb *uurb)
{
	struct ausb_dev_handle *ah = uurb->usercontext;

	DEBUGP("called, ah=%p\n", ah);

	if (uurb->type >= AUSB_USBDEVFS_URB_TYPES) {
		DEBUGP("unknown uurb type %u\n", uurb->type);
		return;
	}

	if (!ah) {
		DEBUGP("cant't call handler because missing ah ptr\n");
		return;
	}

	if (!ah->cb[uurb->type].handler) {
		DEBUGP("received URB type %u, but no handler\n", uurb->type);
		return;
	}
	ah->cb[uurb->type].handler(uurb, ah->cb[uurb->type].userdata);
}

static void sigact_rtmin(int sig, siginfo_t *siginfo, void *v)
{
	int count;
	struct usbdevfs_urb *urb;

	if (sig != SIGAUSB)
		return;
 
	//DEBUGP("errno=%d, si_addr=%p\n", siginfo->errno, siginfo->si_addr);

	DEBUGP("last_handle=%p\n", last_handle);

	if (!last_handle)
		return;

	for (count = 0; ; count++) {
		urb = ausb_get_urb(last_handle);

		if (urb == NULL) {
			DEBUGP("ausb_get_urb() returned urb==NULL\n");
			break;
		}

		DEBUGP("calling handle_urb(%p)\n", urb);
		handle_urb(urb);
	}
}


int ausb_init(void)
{
	struct sigaction act;

	DEBUGP("entering\n");

	memset(&act, 0, sizeof(act));
	act.sa_sigaction = sigact_rtmin;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_SIGINFO;

	sigaction(SIGAUSB, &act, NULL);

	kernel_2_5 = is_kernel_2_5();

	DEBUGP("kernel 2.5+ = %d\n", kernel_2_5);

	usb_init();
	usb_find_busses();
	usb_find_devices();
	return 1;
}

ausb_dev_handle *ausb_open(struct usb_device *dev)
{
	ausb_dev_handle *dh = malloc(sizeof *dh);

	DEBUGP("entering, dh=%p\n", dh);

	memset(dh, 0, sizeof(*dh));

	dh->uh = usb_open(dev);

	if (!dh->uh) {
		DEBUGP("usb_open() failed\n");
		free(dh);
		return NULL;
	}

	last_handle = dh;
	DEBUGP("last_handle = %p\n", last_handle);

	return dh;
}

int ausb_register_callback(ausb_dev_handle *ah, unsigned char type,
			   void (*callback)(struct usbdevfs_urb *uurb,
					    void *userdata),
			   void *userdata)
{
	DEBUGP("registering callback for type %u:%p\n", type, callback);

	if (type >= AUSB_USBDEVFS_URB_TYPES) {
		errno = EINVAL;
		return -1;
	}

	if (!kernel_2_5 && type == USBDEVFS_URB_TYPE_INTERRUPT)
		type = USBDEVFS_URB_TYPE_BULK;

	ah->cb[type].handler = callback;
	ah->cb[type].userdata = userdata;

	return 0;
}

int ausb_claim_interface(ausb_dev_handle *ah, int interface)
{
	DEBUGP("entering\n");
	return usb_claim_interface(ah->uh, interface);
}

int ausb_release_interface(ausb_dev_handle *ah, int interface)
{
	DEBUGP("entering\n");
	/* what the hell? */
	if (ah == last_handle)
		last_handle = NULL;
	return usb_release_interface(ah->uh, interface);
}

int ausb_set_configuration(ausb_dev_handle *ah, int configuration)
{
	DEBUGP("entering\n");
	return usb_set_configuration(ah->uh, configuration);
}

int ausb_bulk_write(ausb_dev_handle *ah, int ep, char *bytes, int size, int timeout)
{
	DEBUGP("entering (ah=%p, ep=0x%x, bytes=%p, size=%d, timeout=%d\n",
		ah, ep, bytes, size, timeout);
	return __usb_bulk_write(ah->uh, ep, bytes, size, timeout);
}

int ausb_bulk_read(ausb_dev_handle *ah, int ep, char *bytes, int size, int timeout)
{
	DEBUGP("entering (ah=%p, ep=0x%x, bytes=%p, size=%d, timeout=%d\n",
		ah, ep, bytes, size, timeout);
	return __usb_bulk_read(ah->uh, ep, bytes, size, timeout);
}

int ausb_clear_halt(ausb_dev_handle *ah, unsigned int ep)
{
	DEBUGP("entering (ah=%p, ep=0x%x)\n", ah, ep);
	return usb_clear_halt(ah->uh, ep);
}

int ausb_reset(ausb_dev_handle *ah)
{
	DEBUGP("entering (ah=%p)\n", ah);
	return usb_reset(ah->uh);
}

int ausb_resetep(ausb_dev_handle *ah, int ep)
{
	DEBUGP("entering (ah=%pm ep=0x%x)\n", ah, ep);
	return usb_resetep(ah->uh, ep);
}

#ifdef LIBUSB_HAS_GET_DRIVER_NP
int ausb_get_driver_np(ausb_dev_handle *ah, int interface, char *name,
		       unsigned int namelen)
{
	DEBUGP("entering\n");
	return usb_get_driver_np(ah->uh, interface, name, namelen);
}
#endif

#ifdef LIBUSB_HAS_DETACH_KERNEL_DRIVER_NP
int ausb_detach_kernel_driver_np(ausb_dev_handle *ah, int interface)
{
	DEBUGP("entering\n");
	return usb_detach_kernel_driver_np(ah->uh, interface);
}
int ausb_reattach_kernel_driver_np(ausb_dev_handle *ah, int interface)
{
	DEBUGP("entering\n");
	return __usb_reattach_kernel_driver_np(ah->uh, interface);
}
#endif

int ausb_close(struct ausb_dev_handle *ah)
{
	DEBUGP("entering\n");

	if (ah == last_handle)
		last_handle = NULL;

	return usb_close(ah->uh);
}

void ausb_fini(void)
{
	DEBUGP("entering\n");
	sigaction(SIGAUSB, NULL, NULL);
}
