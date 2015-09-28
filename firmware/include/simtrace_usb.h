#ifndef SIMTRACE_USB_H
#define SIMTRACE_USB_H

//#include <stdint.h>

/* this is kept compatible with OpenPCD protocol */
struct simtrace_hdr {
	uint8_t cmd;
	uint8_t flags;
	uint8_t res[2];
	uint8_t data[0];
} __attribute__ ((packed));

enum simtrace_usb_msgt {
	SIMTRACE_MSGT_NULL,
	SIMTRACE_MSGT_DATA,
	SIMTRACE_MSGT_RESET,		/* reset was asserted, no more data */
	SIMTRACE_MSGT_STATS,		/* statistics */
};

/* flags for MSGT_DATA */
#define SIMTRACE_FLAG_ATR		0x01	/* ATR immediately after reset */
#define SIMTRACE_FLAG_WTIME_EXP		0x04	/* work waiting time expired */
#define SIMTRACE_FLAG_PPS_FIDI		0x08	/* Fi/Di values in res[2] */

struct simtrace_stats {
	uint32_t no_rctx;
	uint32_t rctx_sent;
	uint32_t rst;
	uint32_t pps;
	uint32_t bytes;
	uint32_t parity_err;
	uint32_t frame_err;
	uint32_t overrun;
} stats;

#endif /* SIMTRACE_USB_H */
