// include/arch/arm64/kernel/context.h

#ifndef ARCH_ARM64_KERNEL_CONTEXT_H
#define ARCH_ARM64_KERNEL_CONTEXT_H

#include <dos/type.h>

struct arch_context {
    uint64_t x19;
    uint64_t x20;
    uint64_t x21;
    uint64_t x22;
    uint64_t x23;
    uint64_t x24;
    uint64_t x25;
    uint64_t x26;
    uint64_t x27;
    uint64_t x28;
    uint64_t x29;
    uint64_t x30;   // Link Registar
    uint64_t sp;    // Stack Pointer
};

void arch_context_init(
    struct arch_context *context,
    void *stack_top,
    void (*entry)(void));

void arch_context_switch(
    struct arch_context *previous,
    struct arch_context *next);

#endif