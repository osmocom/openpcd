/* OpenPICC pin assignment array for dynamic run-time configuration for different board layouts
 * (C) 2008 Henryk Plötz <henryk@ploetzli.ch>
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

#include "board.h"
#include "lib_AT91SAM7.h"

const struct openpicc_hardware OPENPICC_HARDWARE[] = {
		[OPENPICC_v0_4]		   = {OPENPICC_v0_4, 
				"OpenPICC v0.4", // release name
				{0, 0, 0,},       // features: data_gating, clock_gating, clock_switching
				CLOCK_SELECT_PLL, // default_clock
				AT91C_PIO_PA4, // PLL_LOCK
				-1,            // CLOCK_GATE
				-1,            // DATA_GATE
				-1,            // CLOCK_SWITCH
			},
		[OPENPICC_v0_4_p1]	   = {OPENPICC_v0_4_p1, 
				"OpenPICC v0.4 patchlevel 1",
				{1, 1, 0,},
				CLOCK_SELECT_PLL,
				AT91C_PIO_PA5,
				AT91C_PIO_PA4,
				AT91C_PIO_PA31,
				-1,
			},
		[OPENPICC_v0_4_p2]	   = {OPENPICC_v0_4_p2, 
				"OpenPICC v0.4 patchlevel 2",
				{1, 1, 1,},
				CLOCK_SELECT_CARRIER,
				AT91C_PIO_PA5,
				AT91C_PIO_PA4,
				AT91C_PIO_PA31,
				AT91C_PIO_PA30,
			},
};

const struct openpicc_hardware *OPENPICC;
