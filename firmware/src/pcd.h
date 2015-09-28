#ifndef _OPENPCD_H
#define _OPENPCD_H
/* pcd.h - OpenPCD USB protocol definitions
 * (C) 2006 Harald Welte <laforge@gnumonks.org>
 */

#include <sys/types.h>

struct opcd_cmd_hdr {
	uint8_t cmd;
	uint8_t arg1;
	uint16_t arg2;
} __attribute__ ((packed));

enum opcd_cmd {
	OPCD_CMD_REG_READ	= 0x01,	/* Transparent Read of RC632 REG */
	OPCD_CMD_REG_WRITE	= 0x02, /* Transparent Write to RC632 REG */

	OPCD_CMD_FIFO_READ	= 0x03, /* Transparent Read fron RC632 FIFO */
	OPCD_CMD_FIFO_WRITE	= 0x04, /* Transparent Write to RC632 FIFO */

	OPCD_CMD_VFIFO_READ	= 0x05, /* Read bytes from virtual FIFO */
	OPCD_CMD_VFIFO_WRITE	= 0x06, /* Write bytes to virtual FIFO */
	OPCD_CMD_VFIFO_MODE	= 0x07, /* Set Virtual FIFO mode */

	OPCD_CMD_REG_SETBIT	= 0x08,	/* Set a bit in RC632 Register */
	OPCD_CMD_REG_CLRBIT	= 0x09, /* Clear a bit in RC632 Register */
};

struct opcd_status_hdr {
	uint8_t cause,		/* interrupt cause register RC632 */
	uint8_t prim_status,	/* primary status register RC632 */ 
} __attribute__ ((packed));

#endif /* _OPENPCD_H */
