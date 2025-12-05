/*
 * Copyright (C) 2019-2023, STMicroelectronics - All Rights Reserved
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <platform_def.h>

#include <arch.h>
#include <asm_macros.S>
#include <common/bl_common.h>
#include <drivers/st/stm32_gpio.h>
#include <smccc_helpers.h>

#define STM32_HALF_STACK_SIZE	((PLATFORM_STACK_SIZE) >> 1)

	.global	stm32_pwr_down_wfi_load
	.global	stm32_pwr_cstop_critic_exit
	.global	stm32_pwr_down_wfi_wrapper
#if STM32MP13
	.global	stm32_pwr_back_from_stop2
	.global	stm32_pwr_call_optee_ep
#endif
	.global	bl2_vector_table
	.global	disable_mmu_secure
	.global	__STACKS_END__

.section .data
	.balign 4
saved_sp:		.word 0x00000000, 0x00000000
saved_vbar:		.word 0x00000000, 0x00000000
saved_mvbar:		.word 0x00000000, 0x00000000

.section .text
	.align	5

func stm32_pwr_down_wfi_wrapper
	push	{r4,r5,lr}

	# Save r0 and r1 in r4 and r5
	mov	r4, r0
	mov	r5, r1

	# Load CPU_ID in r2 and WORD_SIZE in r3
	bl	plat_my_core_pos
	mov	r2, r0
	mov	r3, #4

	# Save current sp in sp1
	ldr	r1, =saved_sp
	mla	r1, r2, r3, r1
	str	sp, [r1]

	# Save current VBAR in vbar1
	ldr	r1, =saved_vbar
	mla	r1, r2, r3, r1
	ldcopr	r0, VBAR
	str	r0, [r1]

	# Save current MVBAR in mvbar1
	ldr	r1, =saved_mvbar
	mla	r1, r2, r3, r1
	ldcopr	r0, MVBAR
	str	r0, [r1]

	# Reuse BL2 vector table for VBAR and MVBAR
	ldr	r1, =bl2_vector_table
	stcopr	r1, VBAR
	stcopr	r1, MVBAR

	# Set sp to BL2 STACK (as BL2 is not using it anymore)
	ldr	r1, =__STACKS_END__
	mov	r3, #STM32_HALF_STACK_SIZE
	mls	sp, r2, r3, r1

	mov	r0, r4
	mov	r1, r5

	bl	stm32_pwr_down_wfi_load

	mov	r4, r0

	# Load CPU_ID in r2 and WORD_SIZE in r3
	bl	plat_my_core_pos
	mov	r2, r0
	mov	r3, #4

	# Restore stack
	ldr	r1, =saved_sp
	mla	r1, r2, r3, r1
	ldr	sp, [r1]

	# Restore VBAR
	ldr	r1, =saved_vbar
	mla	r1, r2, r3, r1
	ldr	r0, [r1]
	stcopr	r0, VBAR

	# Restore MVBAR
	ldr	r1, =saved_mvbar
	mla	r1, r2, r3, r1
	ldr	r0, [r1]
	stcopr	r0, MVBAR

	mov	r0, r4

	# Synchronize on memory access and instruction
	# after resetting stack/IT handler
	dsb
	isb

	pop	{r4,r5,pc}
endfunc stm32_pwr_down_wfi_wrapper

#if STM32MP13
func stm32_pwr_back_from_stop2
	# switch to monitor mode.
	cps	#MODE32_mon

	ldr	sp, =__STACKS_END__

	ldr	r1, =bl2_vector_table
	stcopr	r1, VBAR
	stcopr	r1, MVBAR

	# exit ddr from self refresh
	bl	stm32_pwr_cstop_critic_exit

	# Restore stack
	ldr	r1, =saved_sp
	ldr	sp, [r1]

	# Restore VBAR
	ldr	r1, =saved_vbar
	ldr	r0, [r1]
	stcopr	r0, VBAR

	# Restore MVBAR
	ldr	r1, =saved_mvbar
	ldr	r0, [r1]
	stcopr	r0, MVBAR

	# Synchronize on memory access and instruction
	# after resetting stack/IT handler
	dsb
	isb

	bl stm32_pwr_call_optee_ep

endfunc stm32_pwr_back_from_stop2
#endif
