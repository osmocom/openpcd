/* AT91SAM7 low-level startup outines for OpenPCD / OpenPICC DFU loader
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

/*------------------------------------------------------------------------------
//*-         ATMEL Microcontroller Software Support  -  ROUSSET  -
//*------------------------------------------------------------------------------
//* The software is delivered "AS IS" without warranty or condition of any
//* kind, either express, implied or statutory. This includes without
//* limitation any warranty or condition with respect to merchantability or
//* fitness for any particular purpose, or against the infringements of
//* intellectual property rights of others.
//*-----------------------------------------------------------------------------
//*- File source          : Cstartup.s
//*- Object               : Generic CStartup for KEIL and GCC No Use REMAP
//*- Compilation flag     : None
//*-
//*- 1.0 18/Oct/04 JPP    : Creation
//*- 1.1 21/Feb/05 JPP    : Set Interrupt
//*- 1.1 01/Apr/05 JPP    : save SPSR 
//*-----------------------------------------------------------------------------*/

#define CONFIG_DFU_SWITCH

//#define DEBUG_LL

#ifdef DEBUG_LL
/* Debugging macros for switching on/off LED1 (green) */
#define PIOA_PER	0xFFFFF400
#define PIOA_OER	0xFFFFF410
#define PIOA_SODR	0xFFFFF430
#define PIOA_CODR	0xFFFFF434
#define LED1		25		/* this only works on OpenPICC, not Olimex */
	.macro	led1on
		ldr	r2, =PIOA_CODR
		mov	r1, #(1 << LED1)
		str	r1, [r2]
	.endm
	.macro	led1off
		ldr	r2, =PIOA_SODR
		mov	r1, #(1 << LED1)
		str	r1, [r2]
	.endm
	.macro	ledinit
		ldr	r2, =PIOA_PER
		mov	r1, #(1 << LED1)
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

     .equ   IRQ_Stack_Size,     0x00000400

	.equ AIC_IVR,         (256) 
	.equ AIC_FVR,         (260) 
	.equ AIC_EOICR,       (304)
	.equ AIC_MCR_RCR,     (0xf00)
	.equ AT91C_BASE_AIC,  (0xFFFFF000)
	.equ AT91C_PMC_PCER,  (0xFFFFFC10)
	.equ AT91C_BASE_PIOA, (0xFFFFF400)
	.equ AT91C_ID_PIOA,   (2)
	.equ PIOA_PDSR,	      (0x3c)
	.equ PIO_BOOTLDR,     (1 << 27)
	#.equ PIO_BOOTLDR,     (1 << 6)


/* #include "AT91SAM7S64_inc.h"	  */

/* Exception Vectors in RAM */

	.text
	.arm
	.section	.vectram, "ax"

	.global _remap_call_dfu
	.func	_remap_call_dfu
_remap_call_dfu:
		led1on
		/* Remap RAM to 0x00000000 for DFU */
		ldr	r1, =AT91C_BASE_AIC
		mov	r2, #0x01
		str	r2, [r1, #AIC_MCR_RCR]
		
		ldr	r4, =dfu_main
		bx	r4

	.size	_remap_call_dfu, . - _remap_call_dfu
	.endfunc


#;------------------------------------------------------------------------------
#;- Section Definition
#;-----------------
#;-  Section    
#;- .internal_ram_top 	Top_Stack: used by the cstartup for vector initalisation 
#;-			management defined by ld and affect from ldscript 
#;------------------------------------------------------------------------------
	.section 	.internal_ram_top
	.code 32
	.align 	0
	.global	Top_Stack
Top_Stack:
	
/*------------------------------------------------------------------------------
*- Area Definition
*------------------------------------------------------------------------------
* .text is used instead of .section .text so it works with arm-aout too.  */
        .section 	.reset
        .text
reset: 
/*------------------------------------------------------------------------------
//*- Exception vectors 
//*--------------------
//*- These vectors can be read at address 0 or at RAM address
//*- They ABSOLUTELY requires to be in relative addresssing mode in order to
//*- guarantee a valid jump. For the moment, all are just looping.
//*- If an exception occurs before remap, this would result in an infinite loop.
//*- To ensure if a exeption occurs before start application to infinite loop.
//*------------------------------------------------------------------------------*/

                B           InitReset           /* 0x00 Reset handler */
undefvec:
                B           undefvec            /* 0x04 Undefined Instruction */
swivec:
                B           swivec              /* 0x08 Software Interrupt */
pabtvec:
                B           pabtvec             /* 0x0C Prefetch Abort */
dabtvec:
		b	dabtvec             /* 0x10 Data Abort */
rsvdvec:
		b	rsvdvec             /* 0x14 reserved  */
irqvec:
		b	IRQ_Handler_Entry	/* 0x18 IRQ */
fiqvec:
		b	FIQ_Handler_Entry
						/* 0x1c FIQ */
dfu_state_dummy:
		.word	0
		
FIQ_Handler_Entry:

/*- Switch in SVC/User Mode to allow User Stack access for C code 	*/
/* because the FIQ is not yet acknowledged*/

/*- Save and r0 in FIQ_Register */
		mov	r9, r0
		ldr	r0, [r8, #AIC_FVR]
		msr	CPSR_c, #I_BIT | F_BIT | ARM_MODE_SVC

		/*- Save scratch/used registers and LR in User Stack */
		stmfd	sp!, { r1-r3, r12, lr}

		/*- Branch to the routine pointed by the AIC_FVR */
		mov	r14, pc
		bx	r0

		/*- Restore scratch/used registers and LR from User Stack */
		ldmia	sp!, { r1-r3, r12, lr}

		/*- Leave Interrupts disabled and switch back in FIQ mode */
		msr	CPSR_c, #I_BIT | F_BIT | ARM_MODE_FIQ

		/*- Restore the R0 ARM_MODE_SVC register */
		mov	r0,r9

		/*- Restore the Program Counter using the LR_fiq directly in the PC */
		subs        pc, lr, #4

	.global IRQ_Handler_Entry
	.func   IRQ_Handler_Entry

IRQ_Handler_Entry:

		/*- Manage Exception Entry  */
		/*- Adjust and save LR_irq in IRQ stack  */
		sub         lr, lr, #4
		stmfd       sp!, {lr}

		/*- Save SPSR need to be saved for nested interrupt */
		mrs	r14, SPSR
		stmfd	sp!, {r14}

		/*- Save and r0 in IRQ stack  */
		stmfd	sp!, {r0}

		/*- Write in the IVR to support Protect Mode  */
		/*- No effect in Normal Mode  */
		/*- De-assert the NIRQ and clear the source in Protect Mode */
		ldr	r14, =AT91C_BASE_AIC
		ldr	r0 , [r14, #AIC_IVR]
		str	r14, [r14, #AIC_IVR]

		/*- Enable Interrupt and Switch in Supervisor Mode */
		msr	CPSR_c, #ARM_MODE_SVC

		/*- Save scratch/used registers and LR in User Stack */
		stmfd	sp!, { r1-r3, r12, r14}

		/*- Branch to the routine pointed by the AIC_IVR  */
		mov	r14, pc
		bx	r0

		/*- Restore scratch/used registers and LR from User Stack*/
		ldmia	sp!, { r1-r3, r12, r14}

		/*- Disable Interrupt and switch back in IRQ mode */
		msr	CPSR_c, #I_BIT | ARM_MODE_IRQ

		/*- Mark the End of Interrupt on the AIC */
		ldr	r14, =AT91C_BASE_AIC
		str	r14, [r14, #AIC_EOICR]

		/*- Restore SPSR_irq and r0 from IRQ stack */
		ldmia	sp!, {r0}

		/*- Restore SPSR_irq and r0 from IRQ stack */
		ldmia	sp!, {r14}
		msr	SPSR_cxsf, r14

		/*- Restore adjusted  LR_irq from IRQ stack directly in the PC */
		ldmia	sp!, {pc}^

	.size   IRQ_Handler_Entry, . - IRQ_Handler_Entry
	.endfunc
	.align 0
.RAM_TOP:
	.word	Top_Stack

	.global _startup
	.func _startup
InitReset:
/*------------------------------------------------------------------------------
/*- Low level Init (PMC, AIC, ? ....) by C function AT91F_LowLevelInit
/*------------------------------------------------------------------------------*/
            	.extern   AT91F_LowLevelInit
/*- minumum C initialization */
/*- call  AT91F_LowLevelInit( void) */

	ldr	r13,.RAM_TOP            /* temporary stack in internal RAM */
/*--Call Low level init function in ABSOLUTE through the Interworking	*/
        ldr	r0,=AT91F_LowLevelInit
	mov	lr, pc
	bx	r0
	ledinit
/*------------------------------------------------------------------------------
//*- Stack Sizes Definition
//*------------------------
//*- Interrupt Stack requires 2 words x 8 priority level x 4 bytes when using
//*- the vectoring. This assume that the IRQ management.
//*- The Interrupt Stack must be adjusted depending on the interrupt handlers.
//*- Fast Interrupt not requires stack If in your application it required you must
//*- be definehere.
//*- The System stack size is not defined and is limited by the free internal
//*- SRAM.
//*------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
//*- Top of Stack Definition
//*-------------------------
//*- Interrupt and Supervisor Stack are located at the top of internal memory in 
//*- order to speed the exception handling context saving and restoring.
//*- ARM_MODE_SVC (Application, C) Stack is located at the top of the external memory.
//*------------------------------------------------------------------------------*/

	  .EQU		IRQ_STACK_SIZE,    (3*8*4)
          .EQU		ARM_MODE_FIQ,       0x11
          .EQU		ARM_MODE_IRQ,       0x12
          .EQU		ARM_MODE_SVC,       0x13

          .EQU		I_BIT,              0x80
          .EQU		F_BIT,              0x40

/*------------------------------------------------------------------------------
//*- Setup the stack for each mode
//*-------------------------------*/
                mov     r0,r13

/*- Set up Fast Interrupt Mode and set FIQ Mode Stack*/
                msr     CPSR_c, #ARM_MODE_FIQ | I_BIT | F_BIT
/*- Init the FIQ register*/
            	ldr     r8, =AT91C_BASE_AIC

/*- Set up Interrupt Mode and set IRQ Mode Stack*/
                msr     CPSR_c, #ARM_MODE_IRQ | I_BIT | F_BIT
                mov     r13, r0                     /* Init stack IRQ */
                sub     r0, r0, #IRQ_Stack_Size
/*- Set up Supervisor Mode and set Supervisor Mode Stack*/
                msr     CPSR_c, #ARM_MODE_SVC
                mov     r13, r0                     /* Init stack Sup */

# Relocate DFU .data section (Copy from ROM to RAM)
                LDR     R1, =_etext
                LDR     R2, =_data
                LDR     R3, =_edata
LoopRel:        CMP     R2, R3
                LDRLO   R0, [R1], #4
                STRLO   R0, [R2], #4
                BLO     LoopRel

# Clear DFU .bss section (Zero init)
                MOV     R0, #0
                LDR     R1, =__bss_start__
                LDR     R2, =__bss_end__
LoopZI:         CMP     R1, R2
                STRLO   R0, [R1], #4
                BLO     LoopZI

		/* prepare c function call to main */
		mov	r0, #0	/* argc = 0 */
		ldr	lr, =exit
		ldr	r10, =0x00101000

#ifdef CONFIG_DFU_SWITCH
		/* check whether bootloader button is pressed */
		ldr	r1, =AT91C_PMC_PCER
		mov	r2, #(1 << AT91C_ID_PIOA)
		str	r2, [r1] 

		ldr 	r1, =AT91C_BASE_PIOA
		ldr	r2, [r1, #PIOA_PDSR]
		tst	r2, #PIO_BOOTLDR
		bne	_reloc_dfu_text
#endif

		bx	r10

_reloc_dfu_text:

		ldr	r1, =0x00100000
		ldr	r2, =0x00200000
		ldr	r3, =_etext
		add	r3, r3, r2
loop_rel_t:	cmp	r2, r3
		ldrlo	r4, [r1], #4
		strlo	r4, [r2], #4
		blo	loop_rel_t
		ldr	r4, =_remap_call_dfu
		bx	r4
		
        .size   _startup, . - _startup
        .endfunc

/* "exit" dummy to avoid sbrk write read etc. needed by the newlib default "exit" */
        .global exit
        .func   exit
exit:
        b    .
		.size   exit, . - exit
        .endfunc

/*---------------------------------------------------------------
//* ?EXEPTION_VECTOR
//* This module is only linked if needed for closing files.
//*---------------------------------------------------------------*/
        .global AT91F_Default_FIQ_handler
        .func   AT91F_Default_FIQ_handler
AT91F_Default_FIQ_handler:
            b     AT91F_Default_FIQ_handler
        .size   AT91F_Default_FIQ_handler, . - AT91F_Default_FIQ_handler
        .endfunc

        .global AT91F_Default_IRQ_handler
        .func   AT91F_Default_IRQ_handler
AT91F_Default_IRQ_handler:
            b     AT91F_Default_IRQ_handler
        .size   AT91F_Default_IRQ_handler, . - AT91F_Default_IRQ_handler
        .endfunc

        .global AT91F_Spurious_handler
        .func   AT91F_Spurious_handler
AT91F_Spurious_handler:
            b     AT91F_Spurious_handler
        .size   AT91F_Spurious_handler, . - AT91F_Spurious_handler
        .endfunc



        .end

