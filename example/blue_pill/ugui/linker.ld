ENTRY(start_)

MEMORY
{
	ram : ORIGIN = 0x20000000, LENGTH = 20k
}

SECTIONS
{
	/*
	 * Program
	 */
	.text : {
		*(.text.startup_) /* start code */
		*(.text)          /* .text sections */
		*(.text*)         /* .text* sections */
		*(.glue_7)        /* Glue ARM to thumb */
		*(.glue_7t)       /* Glue thumb to ARM */
		*(.eh_frame)      /* Exceptions, stack unwinding */

		KEEP (*(.init))
		KEEP (*(.fini))
	} > ram

	/* ARM section unwinding */
	.ARM.extab : {
		*(.ARM.extab* .gnu.linkonce.armextab.*)
	} > ram

	.ARM.exidx : {
		*(.ARM.exidx* .gnu.linkonce.armexidx.*)
	} > ram

	/* Stack unwinding for C++ */
	.eh_frame_hdr : {
		*(.eh_frame_hdr)
	} > ram
	.eh_frame : ONLY_IF_RO {
		*(.eh_frame)
	} > ram

	/* Constants */
	.rodata : ALIGN(4) {
		. = ALIGN(4);
		__rodata_start__ = .;
		*(.rodata)
		*(.rodata.*)
		. = ALIGN(4);
		__rodata_end__ = .;
	} > ram

	_etext = .;

	.data : ALIGN(4)
	{
		. = ALIGN(4);
		*(.data)
		*(.data.*)
		*(.ramtext)
		. = ALIGN(4);
	} > ram

	.bss (NOLOAD) : ALIGN(4)
	{
		. = ALIGN(4);
		_bss_start = .;
		*(.bss)
		*(.bss.*)
		*(COMMON)
		. = ALIGN(4);
		_bss_end = .;
	} > ram

	.noinit (NOLOAD) : ALIGN(4)
	{
		__noinit_start__ = .;
		*(.noinit .noinit.*)
		. = ALIGN(4) ;
		__noinit_end__ = .;
	} > ram

	end = .;


	/*
	.heap (NOLOAD) :
	{
		. = ALIGN(4);
		__heap_start__ = .;
		__heap_base__ = .;
		. = ORIGIN(HEAP_RAM) + LENGTH(HEAP_RAM);
		__heap_end__ = .;
	} > ram
	*/

	
	/*
	.rodata : {
		*(.rodata*);
	} > ram
	.bss : {
		*(.bss*);
	} > ram
	*/
}

