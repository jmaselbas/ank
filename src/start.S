	.section    .init,"ax",@progbits
	.global entry
entry:
vector_base:
	.balign 4
	j   _start
	.balign 4
	j   _start
	.balign 4
	nop /* Hard Fault Handler */
	.balign 4
	nop
	.balign 4
	nop
	.balign 4
	nop
	.balign 4
	nop
	.balign 4
	nop
	.balign 4
	nop
	.balign 4
	nop
	.balign 4
	nop
	.balign 4
	nop /* SysTick Handler */
	.balign 4
	nop
	.balign 4
	nop /* SW Handler */
	.balign 4
	ebreak
_start:
	/* disable interrupts */
	li t0, 0x1800
	csrs mstatus, t0
	/* set the machine trap vector base-address */
	la t0, vector_base
	or t0, t0, 1
	csrw mtvec, t0
	/* init c runtime */
	jal crt0
	la sp, _stack_top
	la t0, main
	csrw mepc, t0
	/* return to main */
	mret
_sys_reset:
	li t0, 0xe000e000 /* PFIC base */
	li t1, 0xbeef0080 /* trigger a sysreset */
	sw t1, 0x48(t0)   /* PFIC_CFGR */
	wfi
	j _sys_reset
crt0:
	/* copy .data */
	la a0, _rodata_end
	la a1, _data_start
	la a2, _data_end
	bgeu a1, a2, 2f
1:
	lw t0, (a0)
	sw t0, (a1)
	add a0, a0, 4
	add a1, a1, 4
	bltu a1, a2, 1b
2:
	/* clear .bss */
	la a1, _bss_start
	la a2, _bss_end
	bgeu a1, a2, 2f
1:
	sw zero, (a1)
	add a1, a1, 4
	bltu a1, a2, 1b
2:
	ret