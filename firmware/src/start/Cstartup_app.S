/* Cstartup header for the application to be started by at91dfu 
 * (C) 2006 by Harald Welte <hwelte@hmw-consulting.de> */

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

        .end

