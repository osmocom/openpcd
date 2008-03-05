/***************************************************************
 *
 * OpenPICC - clock switch driver
 * 
 * Copyright 2008 Henryk Plötz <henryk@ploetzli.ch>
 *
 ***************************************************************

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; version 2.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*/

#include <FreeRTOS.h>
#include <openpicc.h>

#include "clock_switch.h"

static int initialized = 0;

void clock_switch(enum clock_source clock)
{
	if(!OPENPICC->features.clock_switching) return;
	if(!initialized)
		AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, OPENPICC->CLOCK_SWITCH);
	
	switch(clock) {
	case CLOCK_SELECT_PLL:
		AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, OPENPICC->CLOCK_SWITCH);
		break;
	case CLOCK_SELECT_CARRIER:
		AT91F_PIO_SetOutput(AT91C_BASE_PIOA, OPENPICC->CLOCK_SWITCH);
		break;
	}
}

void clock_switch_init(void)
{
	if(!OPENPICC->features.clock_switching) return;
	AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, OPENPICC->CLOCK_SWITCH);
	clock_switch(OPENPICC->default_clock);
	initialized = 1;
}
