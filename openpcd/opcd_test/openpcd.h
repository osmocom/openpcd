#ifndef _OPENPCD_PROTO_H
#define _OPENPCD_PROTO_H

#include <sys/types.h>

struct openpcd_hdr {
	u_int8_t cmd;		/* command */
	u_int8_t flags;
	u_int8_t reg;		/* register */
	u_int8_t val;		/* value (in case of write *) */
	u_int16_t len;
	u_int16_t res;
	u_int8_t data[0];
} __attribute__ ((packed));

#define OPENPCD_CMD_WRITE_REG	0x01
#define OPENPCD_CMD_WRITE_FIFO	0x02
#define OPENPCD_CMD_WRITE_VFIFO	0x03
#define OPENPCD_CMD_READ_REG	0x11
#define OPENPCD_CMD_READ_FIFO	0x12
#define OPENPCD_CMD_READ_VFIFO	0x13
#define OPENPCD_CMD_SET_LED	0x21

#endif
