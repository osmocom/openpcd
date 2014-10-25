//*----------------------------------------------------------------------------
//*         ATMEL Microcontroller Software Support  -  ROUSSET  -
//*----------------------------------------------------------------------------
//* The software is delivered "AS IS" without warranty or condition of any
//* kind, either express, implied or statutory. This includes without
//* limitation any warranty or condition with respect to merchantability or
//* fitness for any particular purpose, or against the infringements of
//* intellectual property rights of others.
//*----------------------------------------------------------------------------
//* File Name           : Debug.h
//* Object              : Debug menu
//* Creation            : JPP   02/Sep/2004
//*----------------------------------------------------------------------------

#ifndef dbgu_h
#define dbgu_h

#define AT91C_DBGU_BAUD 115200

//* ----------------------- External Function Prototype -----------------------

extern const char *hexdump(const void *data, unsigned int len);
void AT91F_DBGU_Init(void);
void AT91F_DBGU_Fini(void);
void AT91F_DBGU_Frame(char *buffer);
#define AT91F_DBGU_Printk(x) AT91F_DBGU_Frame(x)
int AT91F_DBGU_Get( char *val);
#ifndef __WinARM__
void AT91F_DBGU_scanf(char * type,unsigned int * val);
#endif

#ifdef DEBUG
extern void debugp(const char *format, ...);
#define DEBUGP(x, args ...) debugp(x, ## args)
#else
#define	DEBUGP(x, args ...) do {} while(0)
#endif

#define DEBUGPCR(x, args ...) DEBUGP(x "\r\n", ## args)
#define DEBUGPCRF(x, args ...) DEBUGPCR("%s(%d): " x, __FUNCTION__, __LINE__, ## args)

#endif /* dbgu_h */
