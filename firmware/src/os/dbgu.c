/* AT91SAM7 debug function implementation for OpenPCD
 * (C) 2006 by Harald Welte <hwelte@hmw-consulting.de>
 *
 * I based this on existing BSD-style licensed code, please see below.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by 
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*----------------------------------------------------------------------------
 *         ATMEL Microcontroller Software Support  -  ROUSSET  -
 *----------------------------------------------------------------------------
 * The software is delivered "AS IS" without warranty or condition of any
 * kind, either express, implied or statutory. This includes without
 * limitation any warranty or condition with respect to merchantability or
 * fitness for any particular purpose, or against the infringements of
 * intellectual property rights of others.
 *----------------------------------------------------------------------------
 * File Name           : Debug.c
 * Object              : Debug menu
 * Creation            : JPP   14/Sep/2004
 * 1.1 29/Aug/05 JPP   : Update AIC definion
 *----------------------------------------------------------------------------*/

// Include Standard files
#include <board.h>
#include <os/dbgu.h>
#include "../openpcd.h"
#include <os/led.h>
#include <os/main.h>
#include <os/system_irq.h>
#include <os/pcd_enumerate.h>
#include <asm/system.h>
#include <compile.h>

/* In case we find that while (); is non interruptible, we may need to
 * uncommend this line: */
// #define ALLOW_INTERRUPT_LOOP asm("nop;nop;nop;nop;nop;nop;nop;nop;")

#define ALLOW_INTERRUPT_LOOP

/* DEBUG_BUFFER_SIZE MUST BE A POWER OF 2 */
#define DEBUG_BUFFER_SIZE     (1 << 10)
#define DEBUG_BUFFER_MASK     (DEBUG_BUFFER_SIZE - 1)

#define USART_SYS_LEVEL 4
/*---------------------------- Global Variable ------------------------------*/

//*----------------------------------------------------------------------------
//* Function Name       : Send_reset
//* Object              : Acknoledeg AIC and send reset
//*----------------------------------------------------------------------------
static void Send_reset(void)
{
	void (*pfct) (void) = (void (*)(void))0x00000000;

	// Acknoledge the interrupt
	// Mark the End of Interrupt on the AIC
	AT91C_BASE_AIC->AIC_EOICR = 0;
	while (!(AT91C_BASE_DBGU->DBGU_CSR & AT91C_US_TXEMPTY)) ;
	// Jump in reset
	pfct();
}

//*----------------------------------------------------------------------------
//* Function Name       : DBGU_irq_handler
//* Object              : C handler interrupt function called by the sysirq
//*                       demultiplexer
//*----------------------------------------------------------------------------
static void DBGU_irq_handler(u_int32_t sr)
{
	static char value;

	AT91F_DBGU_Get(&value);
	switch (value) {
	case '0':		//* info
		AT91F_DBGU_Frame("Clear Pull up\n\r");
		// Set
		udp_pullup_on();
		break;
	case '1':		//* info
		udp_pullup_off();
		AT91F_DBGU_Frame("Set Pull up\n\r");
		// Reset Application
		Send_reset();
		break;
	case '2':
		AT91F_DBGU_Frame("Toggling LED 1\n\r");
		led_toggle(1);
		break;
	case '3':
		AT91F_DBGU_Frame("Toggling LED 2\n\r");
		led_toggle(2);
		break;
	case '9':
		AT91F_DBGU_Frame("Resetting SAM7\n\r");
		AT91F_RSTSoftReset(AT91C_BASE_RSTC, AT91C_RSTC_PROCRST|
				   AT91C_RSTC_PERRST|AT91C_RSTC_EXTRST);
		break;
	default:
		if (_main_dbgu(value) < 0)
			AT91F_DBGU_Frame("\n\r");
		break;
	}			// end switch
}

void dbgu_rb_init(void);
//*----------------------------------------------------------------------------
//* \fn    AT91F_DBGU_Init
//* \brief This function is used to send a string through the DBGU channel (Very low level debugging)
//*----------------------------------------------------------------------------
void AT91F_DBGU_Init(void)
{
	unsigned int rst_status = AT91F_RSTGetStatus(AT91C_BASE_RSTC);

	dbgu_rb_init();

	//* Open PIO for DBGU
	AT91F_DBGU_CfgPIO();
	//* Enable Transmitter & receivier
	((AT91PS_USART) AT91C_BASE_DBGU)->US_CR =
	    AT91C_US_RSTTX | AT91C_US_RSTRX;

	//* Configure DBGU
	AT91F_US_Configure((AT91PS_USART) AT91C_BASE_DBGU,	// DBGU base address
			   MCK, AT91C_US_ASYNC_MODE,	// Mode Register to be programmed
			   AT91C_DBGU_BAUD,	// Baudrate to be programmed
			   0);	// Timeguard to be programmed

	//* Enable Transmitter & receivier
	((AT91PS_USART) AT91C_BASE_DBGU)->US_CR = AT91C_US_RXEN | AT91C_US_TXEN;

	//* Enable USART IT error and AT91C_US_ENDRX
	AT91F_US_EnableIt((AT91PS_USART) AT91C_BASE_DBGU, AT91C_US_RXRDY);

	//* open interrupt
	sysirq_register(AT91SAM7_SYSIRQ_DBGU, &DBGU_irq_handler);

	debugp("\n\r");
	debugp("(C) 2006-2011 by Harald Welte <hwelte@hmw-consulting.de>\n\r"
			  "This software is FREE SOFTWARE licensed under GNU GPL\n\r");
	debugp("Version " COMPILE_SVNREV
			  " compiled " COMPILE_DATE
			  " by " COMPILE_BY "\n\r\n\r");
	debugp("\n\rDEBUG Interface:\n\r"
			  "0) Set Pull-up 1) Clear Pull-up 2) Toggle LED1 3) "
			  "Toggle LED2\r\n9) Reset\n\r");

	debugp("RSTC_SR=0x%08x\n\r", rst_status);
}

/*
 * Disable the PIO assignments for the DBGU
 */
void AT91F_DBGU_Fini(void)
{
	((AT91PS_USART) AT91C_BASE_DBGU)->US_CR = AT91C_US_RXDIS | AT91C_US_TXDIS;
	AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, AT91C_PA10_DTXD);
	AT91F_PIO_CfgInput(AT91C_BASE_PIOA, AT91C_PA9_DRXD);
	// Maybe FIXME, do more? -- Henryk Plötz <henryk@ploetzli.ch>
}

//*----------------------------------------------------------------------------
//* \fn    AT91F_DBGU_Frame
//* \brief This function is used to send a string through the DBGU channel (Very low level debugging)
//*----------------------------------------------------------------------------
void AT91F_DBGU_Frame(char *buffer)
{
	unsigned long intcFlags;
	unsigned int len = 0;
	while (buffer[len++]) ALLOW_INTERRUPT_LOOP;
	len--;
	local_irq_save(intcFlags);
	AT91C_BASE_DBGU->DBGU_PTCR = 1 << 9; // Disable transfer
	if (AT91C_BASE_DBGU->DBGU_TNCR) {
		AT91C_BASE_DBGU->DBGU_PTCR = 1 << 8;  // Resume transfer
		local_irq_restore(intcFlags);
		while ((AT91C_BASE_DBGU->DBGU_CSR & AT91C_US_TXBUFE) == 0) ALLOW_INTERRUPT_LOOP;
		local_irq_save(intcFlags);
		AT91C_BASE_DBGU->DBGU_PTCR = 1 << 9; // Disable transfer
		AT91C_BASE_DBGU->DBGU_TPR = (unsigned)buffer;
		AT91C_BASE_DBGU->DBGU_TCR = len;
	} else if (AT91C_BASE_DBGU->DBGU_TCR) {
		AT91C_BASE_DBGU->DBGU_TNPR = (unsigned)buffer;
		AT91C_BASE_DBGU->DBGU_TNCR = len;
	} else {
		AT91C_BASE_DBGU->DBGU_TPR = (unsigned)buffer;
		AT91C_BASE_DBGU->DBGU_TCR = len;
	}
	AT91C_BASE_DBGU->DBGU_PTCR = 1 << 8;  // Resume transfer
	local_irq_restore(intcFlags);
	/* Return ONLY after we complete Transfer */
	while ((AT91C_BASE_DBGU->DBGU_CSR & AT91C_US_TXBUFE) == 0) ALLOW_INTERRUPT_LOOP;
}

//*----------------------------------------------------------------------------
//* \fn    AT91F_US_Get
//* \brief Get a Char to USART
//*----------------------------------------------------------------------------
int AT91F_DBGU_Get(char *val)
{
	if ((AT91F_US_RxReady((AT91PS_USART) AT91C_BASE_DBGU)) == 0)
		return (0);
	else {
		*val = AT91F_US_GetChar((AT91PS_USART) AT91C_BASE_DBGU);
		return (-1);
	}
}

// mthomas: function not used in this application. avoid
//  linking huge newlib code for sscanf.

#ifdef DEBUG
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
const char *
hexdump(const void *data, unsigned int len)
{
	static char string[256];
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

struct dbgu {
	char buf[DEBUG_BUFFER_SIZE];
	/* Since debugp appears to require to be re-entrant, we need a
	 * bit more state variables
	 * in_head	Position where incoming *append* characters have
	 * 		finished copy to buffer
	 * in_tail	Position where NEW incoming *append* characters
	 * 		should COPY to
	 * out_head	Position where the LAST *possibly incomplete*
	 * 		dbgu_rb_flush started from
	 * out_tail	Position where the NEXT dbug_rb_flush should
	 * 		start from
	 * The position in the RING should be in this order:
	 * --> out_tail --> in_head --> in_tail --> out_head -->
	 */
	volatile unsigned int in_head, in_tail, out_head, out_tail;

	/* flush_stack is to indicate rb_flush is being executed, NOT to
	 * execute again */
	volatile unsigned int flush_stack;

	/* append_stack is to indicate the re-entrack stack order of the
	 * current dbgu_append call */
	volatile unsigned int append_stack;
};

static struct dbgu dbgu;

void dbgu_rb_init(void)
{
	dbgu.in_head = dbgu.in_tail = dbgu.out_head = dbgu.out_tail = 0;
	dbgu.flush_stack = dbgu.append_stack = 0;
}

/* flush pending data from debug ring buffer to serial port */

static void dbgu_rb_flush(void)
{
	unsigned long intcFlags;
	unsigned int flush_stack, start, len;
	if (dbgu.in_head == dbgu.out_tail)
		return;

	/* Transmit can ONLY be disabled when Interrupt is disabled.  We
	 * don't want to be interrupted while Transmit is disabled. */
	local_irq_save(intcFlags);
	flush_stack = dbgu.flush_stack;
	dbgu.flush_stack = 1;
	if (flush_stack) {
		local_irq_restore(intcFlags);
		return;
	}
	AT91C_BASE_DBGU->DBGU_PTCR = 1 << 9; // Disable transfer
	start = (unsigned)dbgu.buf + dbgu.out_tail;
	len = (dbgu.in_head - dbgu.out_tail) & DEBUG_BUFFER_MASK;
	if (dbgu.in_head > dbgu.out_tail || dbgu.in_head == 0) { // Just 1 fragmentf
		if (AT91C_BASE_DBGU->DBGU_TNCR) {
			if (AT91C_BASE_DBGU->DBGU_TNPR + AT91C_BASE_DBGU->DBGU_TNCR == start) {
				AT91C_BASE_DBGU->DBGU_TNCR += len;
			} else {
				AT91C_BASE_DBGU->DBGU_PTCR = 1 << 8;  // Resume transfer
				local_irq_restore(intcFlags);
				while ((AT91C_BASE_DBGU->DBGU_CSR & AT91C_US_TXBUFE) == 0) ALLOW_INTERRUPT_LOOP;
				dbgu.out_head = dbgu.out_tail; // in case we are interrupted, they may need more space
				/* Since the ONLY place where Transmit is started is dbgu_rb_flush and AT91F_DBGU_Frame and:
				 * 1) AT91F_DBGU_Frame always leave the dbgu in a TX completed state
				 * 2) dbgu_rb is non-reentrant by safeguard at the beginning of this routing
				 * We can assume that after this INTERRUPTIBLE while loop, there are NO data to be transmitted
				 */
				local_irq_save(intcFlags);
				AT91C_BASE_DBGU->DBGU_PTCR = 1 << 9; // Disable transfer
				AT91C_BASE_DBGU->DBGU_TPR = start;
				AT91C_BASE_DBGU->DBGU_TCR = len;
			}
		} else if (AT91C_BASE_DBGU->DBGU_TCR) {
		  if (AT91C_BASE_DBGU->DBGU_TPR + AT91C_BASE_DBGU->DBGU_TCR == start) {
		    dbgu.out_head = AT91C_BASE_DBGU->DBGU_TPR - (unsigned)dbgu.buf;
		    AT91C_BASE_DBGU->DBGU_TCR += len;
		  } else {
		    AT91C_BASE_DBGU->DBGU_TNPR = start;
		    AT91C_BASE_DBGU->DBGU_TNCR = len;
		  }
		} else {
		  AT91C_BASE_DBGU->DBGU_TPR = start;
		  AT91C_BASE_DBGU->DBGU_TCR = len;
		  dbgu.out_head = dbgu.out_tail;
		}
	} else { // 2 fragments
		if ((AT91C_BASE_DBGU->DBGU_CSR & AT91C_US_TXBUFE) == 0) {
			AT91C_BASE_DBGU->DBGU_PTCR = 1 << 8;  // Resume transfer
			local_irq_restore(intcFlags);
			while ((AT91C_BASE_DBGU->DBGU_CSR & AT91C_US_TXBUFE) == 0) ALLOW_INTERRUPT_LOOP;
			dbgu.out_head = dbgu.out_tail; // in case we are interrupted, they may need more space
			local_irq_save(intcFlags);
			AT91C_BASE_DBGU->DBGU_PTCR = 1 << 9; // Disable transfer
		}
		AT91C_BASE_DBGU->DBGU_TPR = start;
		AT91C_BASE_DBGU->DBGU_TCR = DEBUG_BUFFER_SIZE - dbgu.out_tail;
		AT91C_BASE_DBGU->DBGU_TNPR = (unsigned)dbgu.buf;
		AT91C_BASE_DBGU->DBGU_TNCR = dbgu.in_head;
		dbgu.out_head = dbgu.out_tail;
	}
	AT91C_BASE_DBGU->DBGU_PTCR = 1 << 8;  // Resume transfer
	dbgu.out_tail = dbgu.in_head;
	dbgu.flush_stack = 0;
	local_irq_restore(intcFlags);
}

void dbgu_rb_append(char *data, int len)
{
	/* Rules:
	 * 1) ONLY the LOWEST order of dbgu_rb_append CAN update in_head;
	 * 2) WHEN updateing in_head, always set it to the current in_tail (since all higher order dbgu_rb_append have completed
	 * 3) ONLY the LOWEST order of dbgu_rb_append MAY call dbgu_rb_flush
	 */
	unsigned long intcFlags;
	unsigned int append_stack, avail, local_head;
	if (len <= 0)
		return;
	
	local_irq_save(intcFlags);
	append_stack = dbgu.append_stack++;
	if (AT91C_BASE_DBGU->DBGU_CSR & AT91C_US_TXBUFE)
		dbgu.out_head = dbgu.out_tail;
	avail = (dbgu.out_head - 1 - dbgu.in_tail) & DEBUG_BUFFER_MASK;
	local_head = (unsigned)len;
	if (local_head > avail) {
		while ((AT91C_BASE_DBGU->DBGU_CSR & AT91C_US_TXBUFE) == 0);
		dbgu.out_head = dbgu.out_tail;
		avail = (dbgu.out_head - 1 - dbgu.in_tail) & DEBUG_BUFFER_MASK;
		if (local_head > avail) {
			local_head -= avail;
			AT91C_BASE_DBGU->DBGU_TPR = (unsigned)data;
			AT91C_BASE_DBGU->DBGU_TCR = local_head;
			data += local_head;
			len = avail;
		}
	}
	local_head = dbgu.in_tail;
	dbgu.in_tail += len;
	dbgu.in_tail &= DEBUG_BUFFER_MASK;
	local_irq_restore(intcFlags);
	if (dbgu.out_head <= local_head) {
		// We may have to wrap around: out_head won't change because NO call to flush will be made YET
		avail = DEBUG_BUFFER_SIZE - local_head;
		if (avail >= (unsigned)len) {
			memcpy(dbgu.buf + local_head, data, (size_t)len);
		} else {
			memcpy(dbgu.buf + local_head, data, (size_t)avail);
			memcpy(dbgu.buf, data + avail, (size_t)(len - avail));
		}
	} else {
		memcpy(dbgu.buf + local_head, data, len);
	}
	local_irq_save(intcFlags);
	dbgu.append_stack--;
	if (!append_stack)
		dbgu.in_head = dbgu.in_tail;
	local_irq_restore(intcFlags);
	if (!append_stack)
		dbgu_rb_flush();
}

static char dbg_buf[256];
static int line_num = 0;
void debugp(const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	sprintf(dbg_buf, "[%06X] ", line_num++);
	vsnprintf(dbg_buf + 9, sizeof(dbg_buf)-10, format, ap);
	va_end(ap);

	dbg_buf[sizeof(dbg_buf)-1] = '\0';			
	//AT91F_DBGU_Frame(dbg_buf);
#ifdef DEBUG_UNBUFFERED
	AT91F_DBGU_Printk(dbg_buf);
#else
	dbgu_rb_append(dbg_buf, strlen(dbg_buf));
#endif
}
#else
void dbgu_rb_flush(void) {}
void dbgu_rb_init(void) {}
#endif
