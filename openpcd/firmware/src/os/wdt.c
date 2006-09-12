/* AT91SAM7 Watch Dog Timer code for OpenPCD / OpenPICC
 * (C) 2006 by Harald Welte <hwelte@hmw-consulting.de>
 */

#define WDT_DEBUG
 
void wdt_irq(void)
{
	DEBUGPCRF("================> WATCHDOG EXPIRED !!!!!");
}

void wdt_init(void)
{
#ifdef WDT_DEBUG
	AT91F_WDTSetMode(AT91C_BASE_WDT, (0xfff << 16) |
			 AT91C_WDTC_WDDBGHLT | AT91C_WDTC_WDIDLEHLT |
			 AT91C_WDTC_WDFIEN);
#else
	AT91F_WDTSetMode(AT91C_BASE_WDT, (0xfff << 16) |
			 AT91C_WDTC_WDDBGHLT | AT91C_WDTC_WDIDLEHLT |
			 AT91C_WDTC_WDRSTEN);
#endif
}
