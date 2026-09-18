#ifndef ARCH_ARM64_KERNEL_IRQ_H
#define ARCH_ARM64_KERNEL_IRQ_H

#include <dos/type.h>

/*
 * 現在のIRQマスクを保存して、IRQを禁止する。
 * 単一コア用。
 */
static inline uint64_t arch_irq_save(void)
{
    uint64_t flags;

    __asm__ volatile(
        "mrs %0, daif\n\t"
        "msr daifset, #2"
        : "=r"(flags)
        :
        : "memory"
    );

    return flags;
}

/* 呼出し前のIRQマスクへ戻す */
static inline void arch_irq_restore(uint64_t flags)
{
    if ((flags & (1ULL << 7)) != 0U) {
        __asm__ volatile("msr daifset, #2" ::: "memory");
    } else {
        __asm__ volatile("msr daifclr, #2" ::: "memory");
    }
}

#endif