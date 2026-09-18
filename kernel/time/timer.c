// kernel/time/timer.c

#include <dos/type.h>
#include <kernel/timer.h>

static volatile uint64_t timer_ticks;

bool kernel_timer_tick(void)
{
    timer_ticks++;

    /*
     * 切替を要求する。
     * 実際の切替は、irq_dispatch()で
     * GICへの割込み完了通知を済ませた後に行う。
     */
    return true;
}

uint64_t kernel_timer_get_ticks(void)
{
    return timer_ticks;
}