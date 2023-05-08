OUTPUT_FORMAT ("elf32-littleriscv", "elf32-littleriscv", "elf32-littleriscv")

ENTRY(entry);

MEMORY {
	rom (rx)  : ORIGIN = 0x00000000, LENGTH = 64K
	ram (rwx) : ORIGIN = 0x20000000, LENGTH = 20K
}

SECTIONS {
	_stack_top = ORIGIN(ram) + LENGTH(ram);

	.text : AT(ORIGIN(rom)) {
		. = ALIGN(4);
		*(.init)
		*(.text)      /* normal code */
		*(.text*)     /* -ffunction-sections code */
		. = ALIGN(4);
		_text_end = .;
	} > rom

	.rodata : AT(_text_end) {
		. = ALIGN(4);
		*(.got)       /* global offset table, if any */
		*(.rodata)    /* constants */
		*(.rodata*)   /* -ffunction-sections constants */
		. = ALIGN(4);
		_rodata_end = .;
	} > rom

	/* .data in RAM during execution, initial values are in ROM
	 * At startup this need to be copied from ROM (FLASH) to RAM */
	.data : AT(_rodata_end) {
		. = ALIGN(4);
		_data_start = .;
		*(.data)
		*(SORT_BY_ALIGNMENT(.data*))
		. = ALIGN(4);
		_data_end = .;
	} > ram
	_data_size = SIZEOF(.data);

	.bss (NOLOAD) : { /* will be initialized (zero) by init code (ctr0) */
		. = ALIGN(4);
		_bss_start = .;
		*(.bss)
		*(SORT_BY_ALIGNMENT(.bss*) SORT_BY_ALIGNMENT(COMMON*))
		. = ALIGN(4);
		_bss_end = .;
	} > ram
	_bss_size = SIZEOF(.bss);
}
