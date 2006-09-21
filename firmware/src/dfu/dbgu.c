/* AT91SAM7 debug function implementation for OpenPCD
 * (C) 2006 by Harald Welte <hwelte@hmw-consulting.de>
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

#include <lib_AT91SAM7.h>
#include <board.h>
#include <dfu/dbgu.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef DEBUG

#define USART_SYS_LEVEL 4
void AT91F_DBGU_Ready(void)
{
	while (!(AT91C_BASE_DBGU->DBGU_CSR & AT91C_US_TXEMPTY)) ;
}

static void DBGU_irq_handler(void)
{
	static char value;

	AT91F_DBGU_Get(&value);
	switch (value) {
	default:
		AT91F_DBGU_Printk("\n\r");
	}
}

void AT91F_DBGU_Init(void)
{
	/* Open PIO for DBGU */
	AT91F_DBGU_CfgPIO();
	/* Enable Transmitter & receivier */
	((AT91PS_USART) AT91C_BASE_DBGU)->US_CR = AT91C_US_RSTTX | AT91C_US_RSTRX;

	/* Configure DBGU */
	AT91F_US_Configure((AT91PS_USART) AT91C_BASE_DBGU,	// DBGU base address
			   MCK, AT91C_US_ASYNC_MODE,	// Mode Register to be programmed
			   AT91C_DBGU_BAUD,	// Baudrate to be programmed
			   0);	// Timeguard to be programmed

	/* Enable Transmitter & receivier */
	((AT91PS_USART) AT91C_BASE_DBGU)->US_CR = AT91C_US_RXEN | AT91C_US_TXEN;

	/* Enable USART IT error and AT91C_US_ENDRX */
	AT91F_US_EnableIt((AT91PS_USART) AT91C_BASE_DBGU, AT91C_US_RXRDY);

	/* open interrupt */

	/* FIXME: This should be HIGH_LEVEL, but somehow this triggers an
	 * interrupt storm. no idea why it's working in 'os' but not in 'dfu'
	 * */
	AT91F_AIC_ConfigureIt(AT91C_BASE_AIC, AT91C_ID_SYS, USART_SYS_LEVEL,
			      AT91C_AIC_SRCTYPE_INT_POSITIVE_EDGE,
			      DBGU_irq_handler);
	AT91F_AIC_EnableIt(AT91C_BASE_AIC, AT91C_ID_SYS);

	AT91F_DBGU_Printk("\n\rsam7dfu (C) 2006 by Harald Welte\n\r");
}

void AT91F_DBGU_Printk(char *buffer)
{
	while (*buffer != '\0') {
		while (!AT91F_US_TxReady((AT91PS_USART) AT91C_BASE_DBGU)) ;
		AT91F_US_PutChar((AT91PS_USART) AT91C_BASE_DBGU, *buffer++);
	}
}

void AT91F_DBGU_Frame(char *buffer)
{
	unsigned char len;

	for (len = 0; buffer[len] != '\0'; len++) { }

	AT91F_US_SendFrame((AT91PS_USART) AT91C_BASE_DBGU, 
			   (unsigned char *)buffer, len, 0, 0);
}

int AT91F_DBGU_Get(char *val)
{
	if ((AT91F_US_RxReady((AT91PS_USART) AT91C_BASE_DBGU)) == 0)
		return (0);
	else {
		*val = AT91F_US_GetChar((AT91PS_USART) AT91C_BASE_DBGU);
		return (-1);
	}
}

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

static char dbg_buf[2048];
void debugp(const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	vsnprintf(dbg_buf, sizeof(dbg_buf)-1, format, ap);
	va_end(ap);

	dbg_buf[sizeof(dbg_buf)-1] = '\0';			
	//AT91F_DBGU_Frame(dbg_buf);
	AT91F_DBGU_Printk(dbg_buf);
}

#endif
