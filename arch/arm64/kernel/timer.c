// arch/arm64/kernel/timer.c
#include <dos/type.h>
#include <dos/config.h>

#include <kernel/printk.h>
#include <kernel/timer.h>

#include <arch/arm64/kernel/timer.h>
#include <arch/arm64/armv8util.h>
#include <arch/arm64/rpi4/gicv2def.h>
#include <arch/arm64/rpi4/gicctl.h>

static uint64_t timer_interval_cycles;
static uint64_t next_deadline;

static uint64_t milliseconds_to_cycles(uint32_t milliseconds)
{
    uint64_t frequency = ReadSysReg(CNTFRQ_EL0);

    return frequency * (uint64_t)milliseconds / 1000U;
}

void arch_timer_start(uint32_t irq)
{
    timer_interval_cycles =
        milliseconds_to_cycles(TIMER_INTERVAL_MS);

    if (timer_interval_cycles == 0U) {
        timer_interval_cycles = 1U;
    }

    ActivateInterrupt(
        irq,
        TIMER_GIC_PRIORITY,
        false
    );

    next_deadline =
        ReadSysReg(CNTPCT_EL0) + timer_interval_cycles;

    WriteSysReg(CNTP_CVAL_EL0, next_deadline);
    WriteSysReg(CNTP_CTL_EL0, CNTP_CTL_ENABLE);

    __asm__ volatile("isb" ::: "memory");

    printk(
        "[timer] frequency=%llu Hz interval=%llu cycles\n",
        (unsigned long long)ReadSysReg(CNTFRQ_EL0),
        (unsigned long long)timer_interval_cycles
    );
}

bool arch_timer_handler(void)
{
    uint64_t now = ReadSysReg(CNTPCT_EL0);

    next_deadline += timer_interval_cycles;

    /*
     * 割込み禁止などで遅れた場合は、
     * 過去の時刻を設定して割込みを連発させない。
     */
    if (next_deadline <= now) {
        next_deadline = now + timer_interval_cycles;
    }

    WriteSysReg(CNTP_CVAL_EL0, next_deadline);

    __asm__ volatile("isb" ::: "memory");

    return kernel_timer_tick();
}