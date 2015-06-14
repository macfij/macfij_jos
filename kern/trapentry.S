/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

#include <kern/picirq.h>


###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */
TRAPHANDLER_NOEC(idt_divzero, T_DIVIDE)
TRAPHANDLER_NOEC(idt_debug, T_DEBUG)
TRAPHANDLER_NOEC(idt_nmi, T_NMI)
TRAPHANDLER_NOEC(idt_break, T_BRKPT)
TRAPHANDLER_NOEC(idt_overflow, T_OFLOW)
TRAPHANDLER_NOEC(idt_bound, T_BOUND)
TRAPHANDLER_NOEC(idt_illop, T_ILLOP)
TRAPHANDLER_NOEC(idt_device, T_DEVICE)
TRAPHANDLER(idt_doublefault, T_DBLFLT)
TRAPHANDLER(idt_tss, T_TSS)
TRAPHANDLER(idt_segnp, T_SEGNP)
TRAPHANDLER(idt_stack, T_STACK)
TRAPHANDLER(idt_gpf, T_GPFLT)
TRAPHANDLER(idt_pagefault, T_PGFLT)
TRAPHANDLER_NOEC(idt_floatingpoint, T_FPERR)
TRAPHANDLER(idt_align, T_ALIGN)
TRAPHANDLER_NOEC(idt_machinechk, T_MCHK)
TRAPHANDLER_NOEC(idt_simd, T_SIMDERR)
TRAPHANDLER_NOEC(idt_syscall, T_SYSCALL)

TRAPHANDLER_NOEC(irq_timer, IRQ_OFFSET + IRQ_TIMER)
TRAPHANDLER_NOEC(irq_kbd, IRQ_OFFSET + IRQ_KBD)
TRAPHANDLER_NOEC(irq_serial, IRQ_OFFSET + IRQ_SERIAL)
TRAPHANDLER_NOEC(irq_spurious, IRQ_OFFSET + IRQ_SPURIOUS)
TRAPHANDLER_NOEC(irq_ide, IRQ_OFFSET + IRQ_IDE)
TRAPHANDLER_NOEC(irq_error, IRQ_OFFSET + IRQ_ERROR)

/*
 * Lab 3: Your code here for _alltraps
 * should:
 * - push values to make stack look like struct Trapframe:
		struct PushRegs tf_regs;
		uint16_t tf_es;
		uint16_t tf_padding1;
		uint16_t tf_ds;
		uint16_t tf_padding2;
		uint32_t tf_trapno;
		* below here defined by x86 hardware *
		uint32_t tf_err;
		uintptr_t tf_eip;
		uint16_t tf_cs;
		uint16_t tf_padding3;
		uint32_t tf_eflags;
		* below here only when crossing rings, such as from user to kernel *
		uintptr_t tf_esp;
		uint16_t tf_ss;
		uint16_t tf_padding4;
 * - load GD_KD into %ds and %es
 * - pushl %esp to pass a pointer to the Trapframe as an arg to trap()
 * - call trap
 */
_alltraps:
	pushl %ds;
	pushl %es;
	pushal;
	movw $(GD_KD), %ax;
	movw %ax, %ds;
	movw %ax, %es;
	pushl %esp;
	call trap;
