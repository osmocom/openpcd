MEMORY 
{
	flash	: ORIGIN = 0x00100000, LENGTH = 256K - 1K /* save 1K for environment on top of flash */
	ram		: ORIGIN = 0x00200028, LENGTH = 64K-28
}

__stack_end__ = 0x00200000 + 64K - 4;

SECTIONS 
{
	. = 0;
	startup : { *(.startup)} >flash

	prog : 
	{
		*(.text)
		*(.text.*)
		*(.rodata)
		*(.rodata*)
		*(.glue_7)
		*(.glue_7t)
	} >flash

	__end_of_text__ = .;

	.data : 
	{
		*(.fastrun)
		*(.ramfunc)

		__data_beg__ = .; __data_beg_src__ = __end_of_text__;
		*(.data)
		*(.data.*)
		__data_end__ = .;

		__bss_beg__ = .;
		*(.bss)
		*(.bss.*)
		. = ALIGN(32 / 8);
		_end = . ; _bss_end__ = . ; __bss_end__ = . ; __end__ = . ;
	} >ram
}


