#ifndef _PLL_H
#define _PLL_H

extern int pll_is_locked(void);
extern int pll_is_inhibited(void);
extern void pll_inhibit(int inhibit);
extern void pll_init(void);

#endif
