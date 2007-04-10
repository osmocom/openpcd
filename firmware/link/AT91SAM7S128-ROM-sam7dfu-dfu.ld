/* Memory Definitions */

MEMORY
{
  FLASH  (rx) : ORIGIN = 0x00000000, LENGTH = 0x00004000
  DATA   (rw) : ORIGIN = 0x00200000, LENGTH = 0x00008000
  STACK  (rw) : ORIGIN = 0x00208000, LENGTH = 0x00000000
}

/* Section Definitions */

SECTIONS
{
	. = 0x00000000;
	/* first section is .text which is used for code */
	.text 0x00000000: AT ( 0x00000000 ) { 
		* (.reset)
		* (.text)
		* (.rodata*)
		/* * (.dfu.functab) */ /* for debugging */
  		. = ALIGN(4);
  	} >FLASH

  	_etext = . ;
	PROVIDE (etext = .);

	.data.shared (0x00200020): AT (LOADADDR(.text) + SIZEOF(.text)) {
		_data_shared = . ;
		* (.data.shared)
		. = ALIGN(4);
	} >DATA

	_edata_shared = . ;
	_data_flash = LOADADDR(.data.shared) + SIZEOF(.data.shared);

	/* 0x00200000 ... */
	.data (0x00200000 + SIZEOF(.text)): AT 
		( LOADADDR(.text) + SIZEOF(.text) + SIZEOF(.data.shared)) {
  		_data = . ;
		* (.vectram)
  		* (.data)
  		. = ALIGN(4);
  	} >DATA

  	_edata = . ;
	PROVIDE (edata = .);
	
	/* dfu.functab addresses are valid as long as DFU is in flash, i.e.
	 * only while application calls those functions.  Once executing in
	 * DFU mode, we are re-located to ram */
	.dfu.functab 0x00003fd0: AT ( ADDR(.dfu.functab) - ADDR(.text) ) {
  		*(.dfu.functab)
  		. = ALIGN(4096);
  	} >FLASH


	/* .bss section which is used for uninitialized data */
	.bss : {
		__bss_start = . ;
		__bss_start__ = . ;
		*(.bss)
		*(COMMON)
	} >DATA

	. = ALIGN(4);
	__bss_end__ = . ;
	__bss_end__ = . ;

	PROVIDE (main = .);

	_end = . ;

	. = ALIGN(4);
	.int_data : { 
		*(.internal_ram_top) 
   	} >STACK 

	PROVIDE (end = .);

  /* Stabs debugging sections.  */
  .stab          0 : { *(.stab) }
  .stabstr       0 : { *(.stabstr) }
  .stab.excl     0 : { *(.stab.excl) }
  .stab.exclstr  0 : { *(.stab.exclstr) }
  .stab.index    0 : { *(.stab.index) }
  .stab.indexstr 0 : { *(.stab.indexstr) }
  .comment       0 : { *(.comment) }
  /* DWARF debug sections.
     Symbols in the DWARF debugging sections are relative to the beginning
     of the section so we begin them at 0.  */
  /* DWARF 1 */
  .debug          0 : { *(.debug) }
  .line           0 : { *(.line) }
  /* GNU DWARF 1 extensions */
  .debug_srcinfo  0 : { *(.debug_srcinfo) }
  .debug_sfnames  0 : { *(.debug_sfnames) }
  /* DWARF 1.1 and DWARF 2 */
  .debug_aranges  0 : { *(.debug_aranges) }
  .debug_pubnames 0 : { *(.debug_pubnames) }
  /* DWARF 2 */
  .debug_info     0 : { *(.debug_info .gnu.linkonce.wi.*) }
  .debug_abbrev   0 : { *(.debug_abbrev) }
  .debug_line     0 : { *(.debug_line) }
  .debug_frame    0 : { *(.debug_frame) }
  .debug_str      0 : { *(.debug_str) }
  .debug_loc      0 : { *(.debug_loc) }
  .debug_macinfo  0 : { *(.debug_macinfo) }
  /* SGI/MIPS DWARF 2 extensions */
  .debug_weaknames 0 : { *(.debug_weaknames) }
  .debug_funcnames 0 : { *(.debug_funcnames) }
  .debug_typenames 0 : { *(.debug_typenames) }
  .debug_varnames  0 : { *(.debug_varnames) }

}
