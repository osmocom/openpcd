	/* Sample initialization file */

	.extern main
	.extern exit
	.extern AT91F_LowLevelInit
	.extern pio_irq_isr_value
	.extern ssc_tx_pending
	.extern ssc_tx_fiq_fdt_cdiv
	.extern ssc_tx_fiq_fdt_ssc

	.text
	.code 32


	.align  0

	.extern __stack_end__
	.extern __bss_beg__
	.extern __bss_end__
	.extern __data_beg__
	.extern __data_end__
	.extern __data+beg_src__

	.global start
	.global endless_loop

	/* Stack Sizes */
    .set  UND_STACK_SIZE, 0x00000004
    .set  ABT_STACK_SIZE, 0x00000004
    .set  FIQ_STACK_SIZE, 0x00000400
    .set  IRQ_STACK_SIZE, 0X00000400
    .set  SVC_STACK_SIZE, 0x00000400

	/* Standard definitions of Mode bits and Interrupt (I & F) flags in PSRs */
    .set  MODE_USR, 0x10            /* User Mode */
    .set  MODE_FIQ, 0x11            /* FIQ Mode */
    .set  MODE_IRQ, 0x12            /* IRQ Mode */
    .set  MODE_SVC, 0x13            /* Supervisor Mode */
    .set  MODE_ABT, 0x17            /* Abort Mode */
    .set  MODE_UND, 0x1B            /* Undefined Mode */
    .set  MODE_SYS, 0x1F            /* System Mode */

    .equ  I_BIT, 0x80               /* when I bit is set, IRQ is disabled */
    .equ  F_BIT, 0x40               /* when F bit is set, FIQ is disabled */

.equ AT91C_BASE_AIC,  (0xFFFFF000)
.equ AT91C_BASE_MC,   (0xFFFFFF00)
.equ AT91C_BASE_PIOA, 0xFFFFF400
.equ AT91C_BASE_TC0,  0xFFFA0000
.equ AT91C_BASE_SSC,  0xFFFD4000
.equ SSC_CR,          0x0
.equ SSC_RCMR,        0x10
.equ SSC_CR_TXEN,     0x100
.equ AT91C_TC_SWTRG,  ((1 << 2)|1)
.equ AT91C_TC_CLKEN,  (1 << 0)
.equ PIO_DATA,        (1 << 27)
.equ PIO_FRAME,       (1 << 20)
.equ PIO_SSC_TF,      (1 << 15)
.equ PIOA_SODR,       0x30
.equ PIOA_CODR,       0x34
.equ PIOA_PDSR,       0x3c
.equ PIOA_IDR,        0x44
.equ PIOA_ISR,        0x4c
.equ TC_CCR,          0x00
.equ TC2_CV,          (0x80+0x10)
.equ AIC_EOICR,       (304)
.equ PIO_LED1,        (1 << 25)
.equ PIO_LED2,        (1 << 12)
.equ MC_RCR,          0xFFFFFF00
.equ AIC_ISCR,        (0x12C)
.equ PIO_SECONDARY_IRQ, 31
.equ PIO_SECONDARY_IRQ_BIT, (1 << PIO_SECONDARY_IRQ)

start:
_start:
_mainCRTStartup:

	/* Setup a stack for each mode - note that this only sets up a usable stack
	for system/user, SWI and IRQ modes.   Also each mode is setup with
	interrupts initially disabled. */
    ldr   r0, .LC6
    msr   CPSR_c, #MODE_UND|I_BIT|F_BIT /* Undefined Instruction Mode */
    mov   sp, r0
    sub   r0, r0, #UND_STACK_SIZE
    msr   CPSR_c, #MODE_ABT|I_BIT|F_BIT /* Abort Mode */
    mov   sp, r0
    sub   r0, r0, #ABT_STACK_SIZE
    msr   CPSR_c, #MODE_FIQ|I_BIT|F_BIT /* FIQ Mode */
    /* Preload registers for FIQ handler */
    ldr     r10, =AT91C_BASE_PIOA
    ldr     r12, =AT91C_BASE_TC0
    ldr     r8, =AT91C_BASE_AIC
    mov     r9, #AT91C_TC_SWTRG
    /*ldr     r9, =AT91C_BASE_SSC*/
    mov   sp, r0
    sub   r0, r0, #FIQ_STACK_SIZE
    msr   CPSR_c, #MODE_IRQ|I_BIT|F_BIT /* IRQ Mode */
    mov   sp, r0
    sub   r0, r0, #IRQ_STACK_SIZE
    msr   CPSR_c, #MODE_SVC|I_BIT|F_BIT /* Supervisor Mode */
    mov   sp, r0
    sub   r0, r0, #SVC_STACK_SIZE
    msr   CPSR_c, #MODE_SYS|I_BIT|F_BIT /* System Mode */
    mov   sp, r0

	/* We want to start in supervisor mode.  Operation will switch to system
	mode when the first task starts. */
	msr   CPSR_c, #MODE_SVC|I_BIT|F_BIT

    bl		AT91F_LowLevelInit

	/* Clear BSS. */

	mov     a2, #0			/* Fill value */
	mov		fp, a2			/* Null frame pointer */
	mov		r7, a2			/* Null frame pointer for Thumb */

	ldr		r1, .LC1		/* Start of memory block */
	ldr		r3, .LC2		/* End of memory block */
	subs	r3, r3, r1      /* Length of block */
	beq		.end_clear_loop
	mov		r2, #0

.clear_loop:
	strb	r2, [r1], #1
	subs	r3, r3, #1
	bgt		.clear_loop

.end_clear_loop:

	/* Initialise data. */

	ldr		r1, .LC3		/* Start of memory block */
	ldr		r2, .LC4		/* End of memory block */
	ldr		r3, .LC5
	subs	r3, r3, r1		/* Length of block */
	beq		.end_set_loop

.set_loop:
	ldrb	r4, [r2], #1
	strb	r4, [r1], #1
	subs	r3, r3, #1
	bgt		.set_loop

.end_set_loop:

	/* call main */
	mov		r0, #0          /* no arguments  */
	mov		r1, #0          /* no argv either */

    ldr lr, =main	
	bx	lr

endless_loop:
	b               endless_loop


	.align 0

	.LC1:
	.word   __bss_beg__
	.LC2:
	.word   __bss_end__
	.LC3:
	.word   __data_beg__
	.LC4:
	.word   __data_beg_src__
	.LC5:
	.word   __data_end__
	.LC6:
	.word	__stack_end__


	/* Setup vector table.  Note that undf, pabt, dabt, fiq just execute
	a null loop. */

.section .startup,"ax"
         .code 32
         .align 0

	b     _start						/* reset - _start			*/
	ldr   pc, _undf						/* undefined - _undf		*/
	ldr   pc, _swi						/* SWI - _swi				*/
	ldr   pc, _pabt						/* program abort - _pabt	*/
	ldr   pc, _dabt						/* data abort - _dabt		*/
	nop									/* reserved					*/
	ldr   pc, [pc,#-0xF20]				/* IRQ - read the AIC		*/
/*	ldr   pc, [pc,#-0xF20]				/* FIQ - fall through to fiq_handler		*/


/* Following is modified from openpcd/firmware/src/start/Cstartup_app.S */
#define LED_TRIGGER

        .global fiq_handler
        .func fiq_handler
my_fiq_handler:
                /* code that uses pre-initialized FIQ reg */
                /* r8   tmp
                   r9   AT91C_TC_SWTRG
                   r10  AT91C_BASE_PIOA
                   r11  tmp
                   r12  AT91C_BASE_TC0
                   r13  stack
                   r14  lr
                 */

#ifdef LED_TRIGGER
                mov   r11, #PIO_LED1
                str   r11, [r10, #PIOA_CODR] /* enable LED */
#endif
                ldr     r8, [r10, #PIOA_ISR]
                
                /* Store the retrieved PIO ISR value into pio_irq_isr_value */
                ldr   r11, =pio_irq_isr_value
                str   r8, [r11]
                
                tst     r8, #PIO_DATA           /* check for PIO_DATA change */
                ldrne   r11, [r10, #PIOA_PDSR]
                tstne   r11, #PIO_DATA          /* check for PIO_DATA == 1 */
                /*strne   r9, [r12, #TC_CCR]      /* software trigger */

                movne   r11, #PIO_DATA
                strne   r11, [r10, #PIOA_IDR]   /* disable further PIO_DATA FIQ */
                beq .no_pio_data
/* .loop_high:
				ldr     r11, [r10, #PIOA_PDSR]
				tst     r11, #PIO_DATA
				bne     .loop_high
                str   r9, [r12, #TC_CCR]      /* software trigger */

.no_pio_data:
                tst     r8, #PIO_SSC_TF         /* check for SSC Transmit Frame signal */
                ldrne   r11, [r10, #PIOA_PDSR]
                tstne   r11, #PIO_SSC_TF        /* check for SSC_TF == 1 */
                
                movne   r11, #PIO_SSC_TF
                strne   r11, [r10, #PIOA_IDR]   /* disable further SSC_TF FIQ */
                
                ldrne   r11, =ssc_tx_pending
                ldrne   r8, [r11]
                tstne   r8, #0x01              /* Check whether a TX is pending */
                beq     .no_ssc
                
                mov   r8, #PIO_LED1
                str   r8, [r10, #PIOA_SODR] /* disable LED */
                
                mov   r8, #0x00
                str   r8, [r11]                /* Set ssc_tx_pending to 0 */
                
                ldr   r11, =ssc_tx_fiq_fdt_cdiv
                ldr   r11, [r11]               /* r11 == ssc_tx_fiq_fdt_cdiv */

/* Problem: LDR from the timer and loop still take too long and cause us to miss the exact time.
 * (One load-from-timer,compare,jump-back-if-less cycle are 7 CPU cycles, which are 2 carrier cycles.)
 * Strategy: Spin on TC2 till 3 or 4 carrier cycles before the actual time. Then go into an unrolled
 * 'loop' for the remaining time. (load-from-timer,compare,jump-forward-if-greater-or-equal are 5 CPU 
 * cycles if the condition is not reached.)
 * 
 * At 47.923200 MHz 7 processor cycles are 2 carrier cycles of the 13.56MHz carrier
 */
 .equ SUB_TIME, 4 /* subtract 4 carrier cycles == 14 processor cycles */
 
 				mov r8, #SUB_TIME
 				sub r11, r11, r8              /* r11 == fdt_cdiv-SUB_TIME */

.wait_for_fdt_cdiv:
				ldr   r8, [r12, #TC2_CV]
				cmp   r8, r11
				blt   .wait_for_fdt_cdiv       /* spin while TC2.CV is less fdt_cdiv-SUB_TIME */
				
				mov r8, #SUB_TIME
				add r11, r11, r8               /* r11 == fdt_cdiv */

/* Seven copies of the loop contents, covering for 35 CPU cycles, or 10 carrier cycles */
				ldr   r8, [r12, #TC2_CV]
				cmp   r8, r11
				bge   .fdt_expired             /* jump forward if TC2.CV is greater than or equal fdt_cdiv */

				ldr   r8, [r12, #TC2_CV]
				cmp   r8, r11
				bge   .fdt_expired             

				ldr   r8, [r12, #TC2_CV]
				cmp   r8, r11
				bge   .fdt_expired             

				ldr   r8, [r12, #TC2_CV]
				cmp   r8, r11
				bge   .fdt_expired             

				ldr   r8, [r12, #TC2_CV]
				cmp   r8, r11
				bge   .fdt_expired             

				ldr   r8, [r12, #TC2_CV]
				cmp   r8, r11
				bge   .fdt_expired             

				ldr   r8, [r12, #TC2_CV]
				cmp   r8, r11
				bge   .fdt_expired             

.fdt_expired:				
				str   r9, [r12, #TC_CCR]       /* SWTRG on TC0 */
				
                ldr   r11, =ssc_tx_fiq_fdt_ssc
                ldr   r11, [r11]               /* r11 == ssc_tx_fiq_fdt_ssc */

.wait_for_fdt_ssc:
				ldr   r8, [r12, #TC2_CV]
				cmp   r8, r11
				bmi   .wait_for_fdt_ssc        /* spin while TC2.CV is less fdt_ssc */
                
                mov   r11, #PIO_LED1
                str   r11, [r10, #PIOA_CODR] /* enable LED */
                
                ldr   r11, =AT91C_BASE_SSC
                mov   r8, #SSC_CR_TXEN
                str   r8, [r11, #SSC_CR]       /* Write TXEN to SSC_CR, enables tx */ 

.no_ssc:               
                /* Trigger PIO_SECONDARY_IRQ */
                mov r11, #PIO_SECONDARY_IRQ_BIT
                ldr r8, =AT91C_BASE_AIC
                str r11, [r8, #AIC_ISCR]

#ifdef LED_TRIGGER
                mov     r11, #PIO_LED1
                str     r11, [r10, #PIOA_SODR] /* disable LED */
#endif
            
                /*- Mark the End of Interrupt on the AIC */
                ldr     r11, =AT91C_BASE_AIC
                str     r11, [r11, #AIC_EOICR]

                /*- Restore the Program Counter using the LR_fiq directly in the PC */
                subs        pc, lr, #4

        .size   my_fiq_handler, . - my_fiq_handler
        .endfunc

_undf:  .word __undf                    /* undefined				*/
_swi:   .word swi_handler				/* SWI						*/
_pabt:  .word __pabt                    /* program abort			*/
_dabt:  .word __dabt                    /* data abort				*/
_fiq:   .word __fiq                     /* FIQ						*/

__undf: b     .                         /* undefined				*/
__pabt: b     .                         /* program abort			*/
__dabt: b     .                         /* data abort				*/
__fiq:  b     .                         /* FIQ						*/

        .end
        
