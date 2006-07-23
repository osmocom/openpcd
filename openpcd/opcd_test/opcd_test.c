/* opcd_test - Low-Level test program for OpenPCD
 * (C) 2006 by Harald Welte <laforge@gnumonks.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 
 *  as published by the Free Software Foundation
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#define _GNU_SOURCE
#include <getopt.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#include <usb.h>

#include "openpcd.h"

const char *
hexdump(const void *data, unsigned int len)
{
	static char string[65535];
	unsigned char *d = (unsigned char *) data;
	unsigned int i, left;

	string[0] = '\0';
	left = sizeof(string);
	for (i = 0; len--; i += 3) {
		if (i >= sizeof(string) -4)
			break;
		snprintf(string+i, 4, " %02x", *d++);
	}
	return string;
}

#define OPCD_VENDOR_ID	0x2342
#define OPCD_PRODUCT_ID	0x0001
#define OPCD_OUT_EP	0x01
#define OPCD_IN_EP	0x81

static struct usb_dev_handle *hdl;
static struct usb_device *find_opcd_device(void)
{
	struct usb_bus *bus;

	for (bus = usb_busses; bus; bus = bus->next) {
		struct usb_device *dev;
		for (dev = bus->devices; dev; dev = dev->next) {
			if (dev->descriptor.idVendor == OPCD_VENDOR_ID
			    && dev->descriptor.idProduct == OPCD_PRODUCT_ID
			    && dev->descriptor.iManufacturer == 0
			    && dev->descriptor.iProduct == 0
			    && dev->descriptor.bNumConfigurations == 1
			    && dev->config->bNumInterfaces == 1
			    && dev->config->iConfiguration == 0)
				return dev;
		}
	}
	return NULL;
}

static int opcd_recv_reply(void)
{
	char buf[8192];
	int ret, i;

	memset(buf, 0, sizeof(buf));

	ret = usb_bulk_read(hdl, OPCD_IN_EP, buf, sizeof(buf), 0);

	for (i = 0; i < ret; i ++)
		if (buf[i] == 0x03)
			buf[i] = 0x00;

	printf("RX: %s\n", buf, hexdump(buf, ret));

	return ret;
}


static int opcd_send_command(u_int8_t cmd, u_int8_t reg, u_int8_t val, u_int16_t len,
			     const unsigned char *data)
{
	unsigned char buf[8192];
	struct openpcd_hdr *ohdr = (struct openpcd_hdr *)buf;
	int cur = 0;
	int ret;

	memset(buf, 0, sizeof(buf));

	ohdr->cmd = cmd;
	ohdr->reg = reg;
	ohdr->val = val;
	ohdr->len = len;
	if (data && len)
		memcpy(ohdr->data, data, len);
	
	cur = sizeof(*ohdr) + len;

	printf("TX: %s\n", buf,  hexdump(buf, cur));

	ret = usb_bulk_write(hdl, OPCD_OUT_EP, buf, cur, 0);
	if (ret < 0)
		return ret;

	/* this usleep is required in order to make the process work.
	 * apparently some race condition in the bootloader if we feed
	 * data too fast */
	usleep(5000);

	//return ezx_blob_recv_reply();
}

static struct option opts[] = {
	{ "led-set", 1, 0, 'l' },
	{ "reg-write", 1, 0, 'w' },
	{ "reg-read", 1, 0, 'r' },
	{ "fifo-write", 1, 0, 'W' },
	{ "fifo-read", 1, 0, 'R' },
};	

static int get_number(const char *optarg, unsigned int min,
		      unsigned int max, unsigned int *num)
{
	char *endptr;
	unsigned long nbr = strtoul(optarg, &endptr, 10);

	if (nbr == 0 && optarg == endptr)
		return -EINVAL;

	if (nbr <= min || nbr >= max)
		return -ERANGE;

	*num = nbr;
	return 0;
}


int main(int argc, char **argv)
{
	struct usb_device *dev;
	int c;

	usb_init();
	if (!usb_find_busses())
		exit(1);
	if (!usb_find_devices())
		exit(1);

	dev = find_opcd_device();
	if (!dev) {
		printf("Cannot find OpenPCD device. Are you sure it is connected?\n");
		exit(1);
	}

	hdl = usb_open(dev);
	if (!hdl) {
		printf("Unable to open usb device: %s\n", usb_strerror());
		exit(1);
	}

	if (usb_claim_interface(hdl, 0) < 0) {
		printf("Unable to claim usb interface 1 of device: %s\n", usb_strerror());
		exit(1);
	}

	while (1) {
		int option_index = 0;

		c = getopt_long(argc, argv, "l:r:w:R:W:", opts,
				&option_index);

		if (c == -1)
			break;

		switch (c) {
			unsigned int i,j;
		case 'l':
			if (get_number(optarg, 1, 2, &i) < 0)
				exit(2);
			if (get_number(argv[optind], 0, 1, &j) < 0)
				exit(2);
			opcd_send_command(OPENPCD_CMD_SET_LED, i, j, 0, NULL);
			opcd_recv_reply();
			break;
		case 'r':
			if (get_number(optarg, 0x00, 0x3f, &i) < 0)
				exit(2);
			opcd_send_command(OPENPCD_CMD_READ_REG, i, 0, 0, NULL);
			opcd_recv_reply();
			break;
		case 'w':
			if (get_number(optarg, 0x00, 0x3f, &i) < 0)
				exit(2);
			if (get_number(argv[optind], 0x00, 0xff, &j) < 0)
				exit(2);
			fprintf(stdout, "setting register 0x%02x to 0x%02x\n", i, j);
			opcd_send_command(OPENPCD_CMD_WRITE_REG, i, j, 0, NULL);
			opcd_recv_reply();
			break;
		case 'R':
			if (get_number(optarg, 0x00, 0x3f, &i) < 0)
				exit(2);
			opcd_send_command(OPENPCD_CMD_READ_FIFO, 0, i, 0, NULL);
			opcd_recv_reply();
			break;
		case 'W':
			fprintf(stderr, "FIFO write not implemented yet\n");
			break;
		default:
			fprintf(stderr, "unknown key `%c'\n", c);
			break;
		}
	}

#if 0
	//opcd_send_command(OPENPCD_CMD_SET_LED, 2, 1, 0, NULL);

	opcd_send_command(OPENPCD_CMD_WRITE_REG, 0x1b, 0x11, 0, NULL);
	opcd_recv_reply();

	opcd_send_command(OPENPCD_CMD_READ_REG, 0x1b, 0x00, 0, NULL);
	opcd_recv_reply();

	opcd_send_command(OPENPCD_CMD_WRITE_REG, 0x1b, 0x33, 0, NULL);
	opcd_recv_reply();

	opcd_send_command(OPENPCD_CMD_READ_REG, 0x1b, 0x00, 0, NULL);
	opcd_recv_reply();

	opcd_send_command(OPENPCD_CMD_WRITE_REG, 0x15, 0x33, 0, NULL);
#endif

	exit(0);
}
