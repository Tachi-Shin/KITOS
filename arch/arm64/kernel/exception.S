// arch/arm64/kernel/exception.S
.section ".vector_table", "ax"

.global vector_table
.type vector_table, %object

.balign 0x800
vector_table:

curr_el_sp0_sync:       // Synchronous
    b undefined_handler

    .balign 0x80
curr_el_sp0_irq:        // IRQ/vIRQ
    b undefined_handler

    .balign 0x80
curr_el_sp0_fiq:        // FIQ/vFIQ
    b undefined_handler

    .balign 0x80
curr_el_sp0_serr:       // SError/vSError
    b undefined_handler

// Current EL with SPx
    .balign 0x80
curr_el_spx_sync:       // Synchronous
    b sync_entry

    .balign 0x80
curr_el_spx_irq:        // IRQ/vIRQ
    b irq_entry

    .balign 0x80
curr_el_spx_fiq:        // FIQ/vFIQ
    b undefined_handler

    .balign 0x80
curr_el_spx_serr:       // SError/vSError
    b undefined_handler

// Lower EL using AArch64
    .balign 0x80
lower_el_a64_sync:       // Synchronous
    b undefined_handler

    .balign 0x80
lower_el_a64_irq:        // IRQ/vIRQ
    b undefined_handler

    .balign 0x80
lower_el_a64_fiq:        // FIQ/vFIQ
    b undefined_handler

    .balign 0x80
lower_el_a64_serr:       // SError/vSError
    b undefined_handler

// Lower EL using AArch32
    .balign 0x80
lower_el_a32_sync:       // Synchronous
    b undefined_handler

    .balign 0x80
lower_el_a32_irq:        // IRQ/vIRQ
    b undefined_handler

    .balign 0x80
lower_el_a32_fiq:        // FIQ/vFIQ
    b undefined_handler

    .balign 0x80
lower_el_a32_serr:       // SError/vSError
    b undefined_handler

    .balign 0x800
.size vector_table, . - vector_table

.section ".text.exception", "ax"

.balign 4
undefined_handler:
1:
    wfe
    b 1b


.macro save_context
    stp x18, x30, [sp, #-16]!
    stp x0,  x1,  [sp, #-16]!
    stp x2,  x3,  [sp, #-16]!
    stp x4,  x5,  [sp, #-16]!
    stp x6,  x7,  [sp, #-16]!
    stp x8,  x9,  [sp, #-16]!
    stp x10, x11, [sp, #-16]!
    stp x12, x13, [sp, #-16]!
    stp x14, x15, [sp, #-16]!
    stp x16, x17, [sp, #-16]!
    mrs x0, ELR_EL1
    mrs x1, SPSR_EL1
    stp x0,  x1,  [sp, #-16]!
.endm

.macro restore_context
    ldp x0,  x1,  [sp], #16
    msr ELR_EL1, x0
    msr SPSR_EL1, x1
    ldp x16, x17, [sp], #16
    ldp x14, x15, [sp], #16
    ldp x12, x13, [sp], #16
    ldp x10, x11, [sp], #16
    ldp x8,  x9,  [sp], #16
    ldp x6,  x7,  [sp], #16
    ldp x4,  x5,  [sp], #16
    ldp x2,  x3,  [sp], #16
    ldp x0,  x1,  [sp], #16
    ldp x18, x30, [sp], #16
.endm

.global irq_entry
.global sync_entry

    .balign 0x80
irq_entry:
    save_context
    bl irq_dispatch         // exception.c: GIC IAR読み取り → 各ハンドラへ
    restore_context
    eret

    .balign 0x80
sync_entry:
    save_context
    mrs x0, ESR_EL1         // 第1引数: 例外の原因
    mrs x1, FAR_EL1         // 第2引数: フォルトしたアドレス
    bl sync_dispatch         // exception.c: EC判定 → 各ハンドラへ
    restore_context
    eret



#include <arch/arm64/armv8def.h>
    .global EnableInt
    .type EnableInt,@function
EnableInt:
    MSR   DAIFClr,  #(DAIF_A | DAIF_F | DAIF_I)
    ret
    .size EnableInt,.-EnableInt

    .global DisableInt
    .type DisableInt,@function
DisableInt:
    MSR   DAIFSet,  #(DAIF_A | DAIF_F | DAIF_I)
    ret
    .size DisableInt,.-DisableInt