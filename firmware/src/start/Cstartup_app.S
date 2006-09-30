/* Cstartup header for the application to be started by at91dfu 
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


//#define DEBUG_LL

	.equ AIC_FVR,         (260) 
	.equ AIC_EOICR,       (304)
	.equ AT91C_BASE_AIC,  (0xFFFFF000)
	.equ ARM_MODE_FIQ,       0x11
	.equ ARM_MODE_IRQ,       0x12
	.equ ARM_MODE_SVC,       0x13

	.equ I_BIT,              0x80
	.equ F_BIT,              0x40


#define AT91C_BASE_PIOA	0xFFFFF400
#define AT91C_BASE_TC0	0xFFFA0000
#define AT91C_TC_SWTRG	(1 << 2)
#define PIOA_SODR	0x30
#define PIOA_CODR	0x34
#define PIOA_PDSR	0x3c
#define PIOA_ISR	0x4c
#define PIOA_IDR	0x44
#define PIO_DATA	(1 << 27)
#define TC_CCR		0x00


#define PIO_LED1	(1 << 25) /* this only works on OpenPICC, not Olimex */

#ifdef DEBUG_LL
/* Debugging macros for switching on/off LED1 (green) */
#define PIOA_PER	0xFFFFF400
#define PIOA_OER	0xFFFFF410
	.macro	led1on
		ldr	r2, =AT91C_BASE_PIOA
		mov	r1, #PIO_LED1
		str	r1, [r2, #PIOA_CODR]
	.endm
	.macro	led1off
		ldr	r2, =AT91C_BASE_PIOA
		mov	r1, #PIO_LED1
		str	r1, [r2, #PIOA_SODR]
	.endm
	.macro	ledinit
		ldr	r2, =PIOA_PER
		mov	r1, #PIO_LED1
		str	r1, [r2]
		ldr	r2, =PIOA_OER
		str	r1, [r2]
		led1off
	.endm
#else
	.macro ledinit
	.endm
	.macro led1on
	.endm
	.macro led1off
	.endm
#endif

	.global	_startup
	.func _startup
_startup:
	/* Relocate .data section (copy from Flash to RAM) */
	ldr	r1, =_etext
	ldr	r2, =_data
	ldr	r3, =_edata
loop_r:	cmp	r2, r3
	ldrlo	r0, [r1], #4
	strlo	r0, [r2], #4
	blo	loop_r

	/* Clear .bss section (Zero init) */
	mov	r0, #0
	ldr	r1, =__bss_start__
	ldr	r2, =__bss_end__
loop_z:	cmp	r1, r2
	strlo	r0, [r1], #4
	blo	loop_z

	/* initialize FIQ mode registers */ 
	msr	CPSR_c, #ARM_MODE_FIQ | I_BIT | F_BIT
	ldr	r10, =AT91C_BASE_PIOA
	ldr	r12, =AT91C_BASE_TC0
	mov	r9, #AT91C_TC_SWTRG
	msr	CPSR_c, #ARM_MODE_SVC

	led1on

	/* prepare C function call to main */
	mov	r0, #0	/* argc = 0 */
	ldr	lr, =exit
	ldr	r10, =main

	bx	r10

        .size   _startup, . - _startup
        .endfunc
		
/* "exit" dummy to avoid sbrk write read etc. needed by the newlib default "exit" */
        .global exit
        .func   exit
exit:
        b    .
	.size   exit, . - exit
        .endfunc


	.text
	.arm
	.section .fastrun, "ax"

	.global fiq_handler
	.func fiq_handler
fiq_handler:
		/* code that uses pre-initialized FIQ reg */
		/* r8	AT91C_BASE_AIC (dfu init)
		   r9	AT91C_TC_SWTRG
		   r10	AT91C_BASE_PIOA
		   r11	tmp
		   r12	AT91C_BASE_TC0
		   r13	stack
		   r14	lr
		 */
#if 0
		mov 	r11, #PIO_LED1
		str	r11, [r10, #PIOA_CODR] /* enable LED */
		str	r11, [r10, #PIOA_SODR] /* disable LED */
#endif

		ldr	r8, [r10, #PIOA_ISR]
		tst	r8, #PIO_DATA		/* check for PIO_DATA change */
		ldrne	r11, [r10, #PIOA_PDSR]
		tstne	r11, #PIO_DATA		/* check for PIO_DATA == 1 */
		strne	r9, [r12, #TC_CCR]	/* software trigger */

		movne	r11, #PIO_DATA
		strne	r11, [r10, #PIOA_IDR]	/* disable further PIO_DATA FIQ */

		/*- Mark the End of Interrupt on the AIC */
		ldr	r11, =AT91C_BASE_AIC
		str	r11, [r11, #AIC_EOICR]

		/* push r0, r1-r3, r12, r14 onto FIQ stack */
		stmfd	sp!, { r0-r3, r12, lr}
		mov	r0, r8

		/* enable interrupts while handling demux */
		/* msr	CPSR_c, #F_BIT | ARM_MODE_SVC */

		/* Call C function, give PIOA_ISR as argument */
		ldr	r11, =__pio_irq_demux
		mov	r14, pc
		bx	r11 

		/* msr	CPSR_c, #I_BIT | F_BIT | ARM_MODE_FIQ */

		ldmia	sp!, { r0-r3, r12, lr }

#if 0
/*- Save and r0 in FIQ_Register */
		mov	r9, r0
		ldr	r0, [r11, #AIC_FVR]
		str	r8, [r11, #AIC_FVR]
		/* msr	CPSR_c, #I_BIT | F_BIT | ARM_MODE_SVC */

		/*- Save scratch/used registers and LR in User Stack */
		stmfd	sp!, { r1-r3, r12, lr}

		led1on

		/*- Branch to the routine pointed by the AIC_FVR */
		mov	r14, pc
		bx	r0

		/*- Restore scratch/used registers and LR from User Stack */
		ldmia	sp!, { r1-r3, r12, lr}

		/*- Leave Interrupts disabled and switch back in FIQ mode */
		/* msr	CPSR_c, #I_BIT | F_BIT | ARM_MODE_FIQ */

		/*- Mark the End of Interrupt on the AIC */
		ldr	r0, =AT91C_BASE_AIC
		str	r0, [r0, #AIC_EOICR]

		/*- Restore the R0 ARM_MODE_SVC register */
		mov	r0,r9

		ldr	r10, =PIOA_SODR
		mov	r11, #(1 << LED1)
		str	r11, [r10]

#endif
		/*- Restore the Program Counter using the LR_fiq directly in the PC */
		subs        pc, lr, #4

        .size   fiq_handler, . - fiq_handler
        .endfunc
        .end

