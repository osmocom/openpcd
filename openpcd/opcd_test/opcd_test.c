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

#include "../firmware/include/openpcd.h"
#include "opcd_usb.h"

static int get_number(const char *optarg, unsigned int min,
		      unsigned int max, unsigned int *num)
{
	char *endptr;
	unsigned long nbr = strtoul(optarg, &endptr, 0);
	//fprintf(stderr, "trying to read `%s' as number\n", optarg);

	if (nbr == 0 && optarg == endptr)
		return -EINVAL;

	if (nbr < min || nbr > max)
		return -ERANGE;

	*num = nbr;
	return 0;
}

static void print_welcome(void)
{
	printf("opcd_test - OpenPCD Test and Debug Program\n"
	       "(C) 2006 by Harald Welte <laforge@gnumonks.org>\n\n");
}
static void print_help(void)
{
	printf( "\t-l\t--led-set\tled {0,1}\n"
		"\t-w\t--reg-write\treg value\n"
		"\t-r\t--reg-read\treg\n"

		"\t-s\t--set-bits\treg\tmask\n"
		"\t-c\t--clear-bits\treg\tmask\n");
}


static struct option opts[] = {
	{ "led-set", 1, 0, 'l' },
	{ "reg-write", 1, 0, 'w' },
	{ "reg-read", 1, 0, 'r' },
	{ "fifo-write", 1, 0, 'W' },
	{ "fifo-read", 1, 0, 'R' },
	{ "set-bits", 1, 0, 's' },
	{ "clear-bits", 1, 0, 'c' },
	{ "usb-perf", 1, 0, 'u' },
	{ "help", 0, 0, 'h'},
};	

int main(int argc, char **argv)
{
	struct opcd_handle *od;
	int c;
	static char buf[8192];
	int buf_len = sizeof(buf);

	print_welcome();

	od = opcd_init();

	while (1) {
		int option_index = 0;

		c = getopt_long(argc, argv, "l:r:w:R:W:s:c:h?u:", opts,
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
			printf("setting LED %d to %s\n", i, j ? "on" : "off");
			opcd_send_command(od, OPENPCD_CMD_SET_LED, i, j, 0, NULL);
			break;
		case 'r':
			if (get_number(optarg, 0x00, OPENPCD_REG_MAX, &i) < 0)
				exit(2);
			printf("reading register 0x%02x: ");
			opcd_send_command(od, OPENPCD_CMD_READ_REG, i, 0, 0, NULL);
			opcd_recv_reply(od, buf, buf_len);
			break;
		case 'w':
			if (get_number(optarg, 0x00, OPENPCD_REG_MAX, &i) < 0) {
				fprintf(stderr, "can't read register\n");
				exit(2);
			}
			if (get_number(argv[optind], 0x00, 0xff, &j) < 0) {
				fprintf(stderr, "can't read value\n");
				exit(2);
			}
			fprintf(stdout, "setting register 0x%02x to 0x%02x\n", i, j);
			opcd_send_command(od, OPENPCD_CMD_WRITE_REG, i, j, 0, NULL);
			break;
		case 'R':
			if (get_number(optarg, 0x00, OPENPCD_REG_MAX, &i) < 0)
				exit(2);
			opcd_send_command(od, OPENPCD_CMD_READ_FIFO, 0, i, 0, NULL);
			opcd_recv_reply(od, buf, buf_len);
			break;
		case 'W':
			fprintf(stderr, "FIFO write not implemented yet\n");
			break;
		case 's':
			if (get_number(optarg, 0x00, OPENPCD_REG_MAX, &i) < 0)
				exit(2);
			if (get_number(argv[optind], 0x00, 0xff, &j) < 0)
				exit(2);
			opcd_send_command(od, OPENPCD_CMD_REG_BITS_SET, i, j, 0, NULL);
			break;
		case 'c':
			if (get_number(optarg, 0x00, OPENPCD_REG_MAX, &i) < 0)
				exit(2);
			if (get_number(argv[optind], 0x00, 0xff, &j) < 0)
				exit(2);
			opcd_send_command(od, OPENPCD_CMD_REG_BITS_CLEAR, i, j, 0, NULL);
			break;
		case 'u':
			if (get_number(optarg, 1, 255, &i) < 0)
				exit(2);
			opcd_usbperf(od, i);
			break;
		case 'h':
		case '?':
			print_help();
			exit(0);
			break;
		default:
			fprintf(stderr, "unknown key `%c'\n", c);
			print_help();
			exit(2);
			break;
		}
	}

	sleep(1);

	exit(0);
}
