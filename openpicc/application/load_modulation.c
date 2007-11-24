/* AT91SAM7 PWM routines for OpenPICC
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


#include <sys/types.h>
#include <lib_AT91SAM7.h>

#include "openpicc.h"

void load_mod_level(u_int8_t level)
{
	if (level > 3)
		level = 3;

	if (level & 0x1)
		AT91F_PIO_SetOutput(AT91C_BASE_PIOA, OPENPICC_PIO_LOAD1);
	else
		AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, OPENPICC_PIO_LOAD1);

	if (level & 0x2)
		AT91F_PIO_SetOutput(AT91C_BASE_PIOA, OPENPICC_PIO_LOAD2);
	else
		AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, OPENPICC_PIO_LOAD2);
}

void load_mod_init(void)
{
	AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, OPENPICC_PIO_LOAD1);
	AT91F_PIO_CfgOutput(AT91C_BASE_PIOA, OPENPICC_PIO_LOAD2);

	AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, OPENPICC_PIO_LOAD1);
	AT91F_PIO_ClearOutput(AT91C_BASE_PIOA, OPENPICC_PIO_LOAD2);
}
