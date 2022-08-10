SECTIONS
{
	_start = .;
	. = 0;
	.head : {
		KEEP(*(.head))
 	}
	. = ALIGN(0x1000);
	.text : { *(.text) *(.text.*) *(.rodata) *(.rodata.*) }
	. = ALIGN(0x1000);
	.data : { *(.data) *(.data.*) *(.got) *(.toc) }
	. = ALIGN(0x80);
	.bss : {*(.dynsbss) *(.sbss) *(.scommon) *(.dynbss) *(.bss) *(.common) *(.bss.*)}
	. = . + 0x4000;
	__stack_top = .;
}
