// include/arch/arm64/kernel/timer.h
#ifndef ARCH_ARM64_KERNEL_TIMER_H
#define ARCH_ARM64_KERNEL_TIMER_H

#include <dos/type.h>

#define CNTP_CTL_ENABLE       (1U << 0)
#define TIMER_GIC_PRIORITY    10U
#define ARM_GENERIC_TIMER_IRQ 30U

bool arch_timer_handler(void);

void arch_timer_start(uint32_t irq);

/* ハードウェアカウンタからミリ秒単位の時刻を取得する */
uint64_t arch_timer_get_ms(void);

#endif