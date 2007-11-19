#	FreeRTOS.org V4.2.1 - Copyright (C) 2003-2007 Richard Barry.
#
#	This file is part of the FreeRTOS.org distribution.
#
#	FreeRTOS.org is free software; you can redistribute it and/or modify
#	it under the terms of the GNU General Public License as published by
#	the Free Software Foundation; either version 2 of the License, or
#	(at your option) any later version.
#
#	FreeRTOS.org is distributed in the hope that it will be useful,
#	but WITHOUT ANY WARRANTY; without even the implied warranty of
#	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#	GNU General Public License for more details.
#
#	You should have received a copy of the GNU General Public License
#	along with FreeRTOS.org; if not, write to the Free Software
#	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
#	A special exception to the GPL can be applied should you wish to distribute
#	a combined work that includes FreeRTOS.org, without being obliged to provide
#	the source code for any proprietary components.  See the licensing section 
#	of http://www.FreeRTOS.org for full details of how and when the exception
#	can be applied.
#
#	***************************************************************************
#	See http://www.FreeRTOS.org for documentation, latest information, license 
#	and contact details.  Please ensure to read the configuration and relevant 
#	port sections of the online documentation.
#	***************************************************************************

CC=arm-elf-gcc
OBJCOPY=arm-elf-objcopy
OBJDUMP=arm-elf-objdump
ARCH=arm-elf-ar
CRT0=os/boot/boot.s
DEBUG=-g
OPTIM=-O2
LDSCRIPT=config/atmel-rom.ld
ARCH=AT91SAM7S256

#
# CFLAGS common to both the THUMB and ARM mode builds
#

CFLAGS= \
-D __$(ARCH)__ \
-Iconfig \
-Iapplication \
-Ios/core/include  \
-Ios/usb  \
-Ios/core/ARM7_AT91SAM7S  \
-Wall  \
-Werror \
-Wextra  \
-Wno-multichar \
-Wstrict-prototypes  \
-Wno-strict-aliasing  \
-D SAM7_GCC  \
-mcpu=arm7tdmi  \
-ffunction-sections \
-fdata-sections \
-T$(LDSCRIPT) \
$(DEBUG)  \
$(OPTIM) \
-fomit-frame-pointer

LINKER_FLAGS=-Xlinker -oopenpicc.elf -Xlinker -M -Xlinker -Map=openpicc.map

#
# Source files that must be built to ARM mode.
#
ARM_SRC= \
  application/main.c \
  application/led.c \
  application/cmd.c \
  application/env.c \
  application/da.c \
  application/adc.c \
  application/pll.c \
  application/pio_irq.c \
  application/ssc_picc.c \
  application/tc_cdiv_sync.c \
  application/tc_fdt.c \
  application/tc_cdiv.c \
  application/usb_print.c \
  os/boot/Cstartup_SAM7.c \
  os/core/list.c \
  os/core/queue.c \
  os/core/tasks.c \
  os/core/ARM7_AT91SAM7S/lib_AT91SAM7.c \
  os/core/ARM7_AT91SAM7S/port.c \
  os/core/ARM7_AT91SAM7S/portISR.c \
  os/core/MemMang/heap_2.c \
  os/usb/USB-CDC.c \
  os/usb/USBIsr.c
#  application/mesh/mesh.c \

#
# Define all object files.
#
ARM_OBJ = $(ARM_SRC:.c=.o)
FREERTOS_THUMB_OBJ = $(FREERTOS_THUMB_SRC:.c=.o)
DEMO_APP_THMUB_OBJ = $(DEMO_APP_THMUB_SRC:.c=.o)

openpicc.bin : openpicc.elf
	$(OBJCOPY) openpicc.elf -O binary openpicc.bin

openpicc.hex : openpicc.elf
	$(OBJCOPY) openpicc.elf -O ihex openpicc.hex

openpicc.elf : $(ARM_OBJ) $(DEMO_APP_THMUB_OBJ) $(FREERTOS_THUMB_OBJ) $(CRT0) Makefile config/FreeRTOSConfig.h
	$(CC) $(CFLAGS) $(ARM_OBJ) $(DEMO_APP_THMUB_OBJ) $(FREERTOS_THUMB_OBJ) -nostartfiles $(CRT0) $(LINKER_FLAGS)
	$(OBJDUMP) -d openpicc.elf > openpicc.asm

$(DEMO_APP_THMUB_OBJ)  : %.o : %.c $(LDSCRIPT) Makefile config/FreeRTOSConfig.h
	$(CC) -c $(THUMB_FLAGS) $(CFLAGS) $< -o $@

$(FREERTOS_THUMB_OBJ)  : %.o : %.c $(LDSCRIPT) Makefile config/FreeRTOSConfig.h
	$(CC) -c $(THUMB_FLAGS) $(CFLAGS) $< -o $@

$(ARM_OBJ) : %.o : %.c $(LDSCRIPT) Makefile config/FreeRTOSConfig.h config/board.h config/compile.h
	$(CC) -c $(CFLAGS) $< -o $@

clean :
	touch Makefile
	find -name '*.o' -exec rm \{\} \;
	rm -f openpicc.bin openpicc.elf openpicc.map openpicc.asm config/compile.h

.PHONY: config/compile.h
config/compile.h:
	scripts/mkcompile_h > config/compile.h
