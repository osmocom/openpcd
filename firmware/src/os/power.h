#ifndef _POWER_H

static inline void cpu_idle(void)
{
	AT91F_PMC_DisablePCK(AT91C_BASE_PMC, AT91C_PMC_PCK);
}

#endif
