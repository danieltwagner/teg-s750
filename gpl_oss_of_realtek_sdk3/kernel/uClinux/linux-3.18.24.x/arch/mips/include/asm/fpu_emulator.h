/*
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * Further private data for which no space exists in mips_fpu_struct.
 * This should be subsumed into the mips_fpu_struct structure as
 * defined in processor.h as soon as the absurd wired absolute assembler
 * offsets become dynamic at compile time.
 *
 * Kevin D. Kissell, kevink@mips.com and Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 2000 MIPS Technologies, Inc.  All rights reserved.
 */
#ifndef _ASM_FPU_EMULATOR_H
#define _ASM_FPU_EMULATOR_H

#include <linux/sched.h>
#include <asm/break.h>
#include <asm/thread_info.h>
#include <asm/inst.h>
#include <asm/local.h>
#include <asm/processor.h>

#ifdef CONFIG_CPU_HAS_EMU
#ifdef CONFIG_DEBUG_FS

struct mips_fpu_emulator_stats {
	unsigned long emulated;
	unsigned long loads;
	unsigned long stores;
	unsigned long cp1ops;
	unsigned long cp1xops;
	unsigned long errors;
	unsigned long ieee754_inexact;
	unsigned long ieee754_underflow;
	unsigned long ieee754_overflow;
	unsigned long ieee754_zerodiv;
	unsigned long ieee754_invalidop;
};

DECLARE_PER_CPU(struct mips_fpu_emulator_stats, fpuemustats);

#define MIPS_FPU_EMU_INC_STATS(M)					\
do {									\
	preempt_disable();						\
	__this_cpu_inc(fpuemustats.M);					\
	preempt_enable();						\
} while (0)

#else
#define MIPS_FPU_EMU_INC_STATS(M) do { } while (0)
#endif /* CONFIG_DEBUG_FS */

extern int mips_dsemul(struct pt_regs *regs, mips_instruction ir,
	unsigned long cpc);
extern int do_dsemulret(struct pt_regs *xcp);
extern int fpu_emulator_cop1Handler(struct pt_regs *xcp,
				    struct mips_fpu_struct *ctx, int has_fpu,
				    void *__user *fault_addr);
#else
static inline int do_dsemulret(struct pt_regs *xcp)
{
	return 0;	/* 0 means error, should never get here anyway */
}

static inline int fpu_emulator_cop1Handler(struct pt_regs *xcp,
					   struct mips_fpu_struct *ctx,
					   int has_fpu,
					   void *__user *fault_addr)
{
	*fault_addr = NULL;
	return SIGILL;	/* We don't speak MIPS FPU */
}
#endif /* CONFIG_CPU_HAS_EMU */

int process_fpemu_return(int sig, void __user *fault_addr);

/*
 * Instruction inserted following the badinst to further tag the sequence
 */
#define BD_COOKIE 0x0000bd36	/* tne $0, $0 with baggage */

/*
 * Break instruction with special math emu break code set
 */
#define BREAK_MATH (0x0000000d | (BRK_MEMU << 16))

#define SIGNALLING_NAN 0x7ff800007ff80000LL

static inline void fpu_emulator_init_fpu(void)
{
	struct task_struct *t = current;
	int i;

	t->thread.fpu.fcr31 = 0;

	for (i = 0; i < 32; i++)
		set_fpr64(&t->thread.fpu.fpr[i], 0, SIGNALLING_NAN);
}

#endif /* _ASM_FPU_EMULATOR_H */
