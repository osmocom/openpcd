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
#include "dbgu.h"
#include "rc632.h"
#include "openpcd.h"
#include "led.h"
#include "main.h"
#include <asm/system.h>

#define USART_SYS_LEVEL 4
/*---------------------------- Global Variable ------------------------------*/
//*--------------------------1--------------------------------------------------
//* \fn    AT91F_DBGU_Printk
//* \brief This function is used to send a string through the DBGU channel
//*----------------------------------------------------------------------------
void AT91F_DBGU_Ready(void)
{
	while (!(AT91C_BASE_DBGU->DBGU_CSR & AT91C_US_TXEMPTY)) ;
}

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
	AT91F_DBGU_Ready();
	// Jump in reset
	pfct();
}

//*----------------------------------------------------------------------------
//* Function Name       : DBGU_irq_handler
//* Object              : C handler interrupt function called by the interrupts
//*                       assembling routine
//*----------------------------------------------------------------------------
static void DBGU_irq_handler(void)
{
	static char value;

	AT91F_DBGU_Get(&value);
	switch (value) {
	case '0':		//* info
		AT91F_DBGU_Frame("Set Pull up\n\r");
		// Set
		AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, OPENPCD_PIO_UDP_PUP);
		break;
	case '1':		//* info
		AT91F_PIO_SetOutput(AT91C_BASE_PIOA, OPENPCD_PIO_UDP_PUP);
		AT91F_DBGU_Printk("Clear Pull up\n\r");
		// Reset Application
		Send_reset();
		break;
	case '2':
		AT91F_DBGU_Printk("Toggling LED 1\n\r");
		led_toggle(1);
		break;
	case '3':
		AT91F_DBGU_Printk("Toggling LED 2\n\r");
		led_toggle(2);
		break;
	case '4':
		AT91F_DBGU_Printk("Testing RC632 : ");
		if (rc632_test(RAH) == 0)
			AT91F_DBGU_Printk("SUCCESS!\n\r");
		else
			AT91F_DBGU_Printk("ERROR!\n\r");
			
		break;
	case '5':
		rc632_reg_read(RAH, RC632_REG_RX_WAIT, &value);
		DEBUGPCR("Reading RC632 Reg RxWait: 0x%02xr", value);

		break;
	case '6':
		DEBUGPCR("Writing RC632 Reg RxWait: 0x55");
		rc632_reg_write(RAH, RC632_REG_RX_WAIT, 0x55);
		break;
	case '7':
		rc632_dump();
		break;
	default:
		if (_main_dbgu(value) < 0)
			AT91F_DBGU_Printk("\n\r");
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
	AT91F_AIC_ConfigureIt(AT91C_BASE_AIC, AT91C_ID_SYS, USART_SYS_LEVEL,
			      AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL,
			      DBGU_irq_handler);
	AT91F_AIC_EnableIt(AT91C_BASE_AIC, AT91C_ID_SYS);

	AT91F_DBGU_Printk
	    ("\n\r-I- OpenPCD test mode\n\r 0) Set Pull-up 1) Clear Pull-up "
	     "2) Toggle LED1 3) Toggle LED2 4) Test RC632\n\r"
	     "5) Read RxWait 6) Write RxWait 7) Dump RC632 Regs\n\r");
}

//*----------------------------------------------------------------------------
//* \fn    AT91F_DBGU_Printk
//* \brief This function is used to send a string through the DBGU channel (Very low level debugging)
//*----------------------------------------------------------------------------
void AT91F_DBGU_Printk(char *buffer)
{
	while (*buffer != '\0') {
		while (!AT91F_US_TxReady((AT91PS_USART) AT91C_BASE_DBGU)) ;
		AT91F_US_PutChar((AT91PS_USART) AT91C_BASE_DBGU, *buffer++);
	}
}

//*----------------------------------------------------------------------------
//* \fn    AT91F_DBGU_Frame
//* \brief This function is used to send a string through the DBGU channel
//*----------------------------------------------------------------------------
void AT91F_DBGU_Frame(char *buffer)
{
	unsigned char len;

	for (len = 0; buffer[len] != '\0'; len++) {
	}

	AT91F_US_SendFrame((AT91PS_USART) AT91C_BASE_DBGU, 
			   (unsigned char *)buffer, len, 0, 0);

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
	char buf[4096];
	char *next_inbyte;
	char *next_outbyte;
};
static struct dbgu dbgu;

void dbgu_rb_init(void)
{
	memset(dbgu.buf, 0, sizeof(dbgu.buf));
	dbgu.next_inbyte = &dbgu.buf[0];
	dbgu.next_outbyte = &dbgu.buf[0];
}

/* pull one char out of debug ring buffer */
static int dbgu_rb_pull(char *ret)
{
	unsigned long flags;

	local_irq_save(flags);

	if (dbgu.next_outbyte == dbgu.next_inbyte) {
		local_irq_restore(flags);
		return -1;
	}

	*ret = *dbgu.next_outbyte;

	dbgu.next_outbyte++;
	if (dbgu.next_outbyte == &dbgu.buf[0]+sizeof(dbgu.buf)) {
		//AT91F_DBGU_Printk("WRAP DURING PULL\r\n");
		dbgu.next_outbyte = &dbgu.buf[0];
	} else if (dbgu.next_outbyte > &dbgu.buf[0]+sizeof(dbgu.buf)) {
		//AT91F_DBGU_Printk("OUTBYTE > END_OF_BUF!!\r\n");
		dbgu.next_outbyte -= sizeof(dbgu.buf);
	}

	local_irq_restore(flags);

	return 0;
}

static void __rb_flush(void)
{
	char ch;
	while (dbgu_rb_pull(&ch) >= 0) {
		while (!AT91F_US_TxReady((AT91PS_USART) AT91C_BASE_DBGU)) ;
		AT91F_US_PutChar((AT91PS_USART) AT91C_BASE_DBGU, ch);
	}
}

/* flush pending data from debug ring buffer to serial port */
void dbgu_rb_flush(void)
{
	__rb_flush();
}

static void __dbgu_rb_append(char *data, int len)
{
	char *pos = dbgu.next_inbyte;

	dbgu.next_inbyte += len;
	if (dbgu.next_inbyte >= &dbgu.buf[0]+sizeof(dbgu.buf)) {
		AT91F_DBGU_Printk("WRAP DURING APPEND\r\n");
		dbgu.next_inbyte -= sizeof(dbgu.buf);
	}

	memcpy(pos, data, len);
}

void dbgu_rb_append(char *data, int len)
{
	unsigned long flags;
	int bytes_left;
	char *data_cur;
	
	local_irq_save(flags);

	bytes_left = &dbgu.buf[0]+sizeof(dbgu.buf)-dbgu.next_inbyte;
	data_cur = data;

	if (len > bytes_left) {
		AT91F_DBGU_Printk("LEN > BYTES_LEFT\r\n");
		__rb_flush();
		__dbgu_rb_append(data_cur, bytes_left);
		len -= bytes_left;
		data_cur += bytes_left;
	}
	__dbgu_rb_append(data_cur, len);

	local_irq_restore(flags);
}

static char dbg_buf[256];
void debugp(const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	vsnprintf(dbg_buf, sizeof(dbg_buf)-1, format, ap);
	va_end(ap);

	dbg_buf[sizeof(dbg_buf)-1] = '\0';			
	//AT91F_DBGU_Frame(dbg_buf);
	//AT91F_DBGU_Printk(dbg_buf);
	dbgu_rb_append(dbg_buf, strlen(dbg_buf));
}
#else
void dbgu_rb_flush(void) {}
void dbgu_rb_init(void) {}
#endif
