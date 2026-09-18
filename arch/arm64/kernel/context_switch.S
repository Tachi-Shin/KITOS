// arch/arm64/kernel/context_switch.S

.globl arch_context_switch
.type arch_context_switch, %function
.balign 4

// void arch_context_switch(
//     struct arch_context *previous,  // x0
//     struct arch_context *next);    // x1

arch_context_switch:
    // previousへ現在のコンテキストを保存
    stp     x19, x20, [x0, #0x00]
    stp     x21, x22, [x0, #0x10]
    stp     x23, x24, [x0, #0x20]
    stp     x25, x26, [x0, #0x30]
    stp     x27, x28, [x0, #0x40]
    stp     x29, x30, [x0, #0x50]

    mov     x2, sp
    str     x2, [x0, #0x60]

    // nextからコンテキストを復元
    ldp     x19, x20, [x1, #0x00]
    ldp     x21, x22, [x1, #0x10]
    ldp     x23, x24, [x1, #0x20]
    ldp     x25, x26, [x1, #0x30]
    ldp     x27, x28, [x1, #0x40]
    ldp     x29, x30, [x1, #0x50]

    ldr     x2, [x1, #0x60]
    mov     sp, x2

    ret

.size arch_context_switch, . - arch_context_switch