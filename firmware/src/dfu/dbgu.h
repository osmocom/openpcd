#ifndef dbgu_h
#define dbgu_h

#define AT91C_DBGU_BAUD 115200

#define DEBUGP(x, args ...) debugp(x, ## args)
#define DEBUGPCR(x, args ...) DEBUGP(x "\r\n", ## args)
#define DEBUGPCRF(x, args ...) DEBUGPCR("%s(%d): " x, __FUNCTION__, __LINE__, ## args)

extern void AT91F_DBGU_Init(void);
extern void AT91F_DBGU_Printk(char *buffer);
extern int AT91F_DBGU_Get(char *val);
extern void AT91F_DBGU_Ready(void);

#ifdef DEBUG
extern void debugp(const char *format, ...);
extern const char *hexdump(const void *data, unsigned int len);
extern void AT91F_DBGU_Frame(char *buffer);
#else
#define debugp(x, args ...)
#define hexdump(x, args ...)
#define AT91F_DBGU_Frame(x)
#endif

#endif /* dbgu_h */
