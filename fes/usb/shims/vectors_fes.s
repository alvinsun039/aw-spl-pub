/*
 * Copyright (c) 2026, edk2-a733 contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */
/* vectors_fes.s - FES mode exception vectors + IRQ stub + abort diagnostics
 *
 * The table must be 32-byte aligned for VBAR (ARMv7: VBAR[4:0] are RES0);
 * fes_fes_vectors_init() programs VBAR with its address.
 *
 * Each abort path prints one letter before parking the core, so a crash
 * log tells which exception fired:
 *   R reset, U undefined, S supervisor call, P prefetch abort,
 *   D data abort, F FIQ, ? reserved.
 */
    .arm
    .section .fes_vectors, "ax"
    .balign 32
    .global fes_vectors
fes_vectors:
    b   abort_reset
    b   abort_undef
    b   abort_svc
    b   abort_pabt
    b   abort_dabt
    b   abort_resv
    b   fes_irq_stub
    b   abort_fiq

abort_reset:    mov r0, #'R'
                b   abort_common
abort_undef:    mov r0, #'U'
                b   abort_common
abort_svc:      mov r0, #'S'
                b   abort_common
abort_pabt:     mov r0, #'P'
                b   abort_common
abort_dabt:     mov r0, #'D'
                b   abort_common
abort_resv:     mov r0, #'?'
                b   abort_common
abort_fiq:      mov r0, #'F'
                b   abort_common

    .global fes_irq_stub
fes_irq_stub:
    sub  lr, lr, #4
    push {r0-r3, r12, lr}
    bl   do_irq
    pop  {r0-r3, r12, lr}
    movs pc, lr

/* Polled UART0 write of the letter in r0, then a newline, then park. */
abort_common:
    ldr  r1, =0x02500000       @ UART0 THR
    ldr  r2, =0x02500014       @ UART0 LSR
wait1:
    ldr  r3, [r2]
    tst  r3, #0x20
    beq  wait1
    str  r0, [r1]
wait2:
    ldr  r3, [r2]
    tst  r3, #0x20
    beq  wait2
    mov  r3, #'\n'
    str  r3, [r1]
hang:
    b    hang

    .global fes_fes_vectors_init
fes_fes_vectors_init:
    mrs  r1, cpsr
    ldr  r0, =fes_vectors
    mcr  p15, 0, r0, c12, c0, 0	@ VBAR
    mrs  r2, cpsr
    and  r2, r2, #0x1f
    cmp  r2, #0x16
    mcrne p15, 0, r0, c12, c0, 1	@ MVBAR (monitor mode only)
    cps  #0x12			@ IRQ mode
    ldr  sp, =0xDF000		@ IRQ stack above the image
    cps  #0x16			@ monitor mode
    ldr  sp, =0xDF400
    cps  #0x17			@ abort mode
    ldr  sp, =0xDF800
    msr  cpsr_c, r1		@ restore mode and flags
    bx   lr