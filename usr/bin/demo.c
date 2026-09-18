#include <dos/config.h>

#include <kernel/task.h>
#include <kernel/timer.h>

#include <usr/shell.h>

void demo_task(void *argument)
{
    const char *message = argument;

    uint64_t interval =
        (1000U + TIMER_INTERVAL_MS - 1U) / TIMER_INTERVAL_MS;

    uint64_t next = kernel_timer_get_ticks() + interval;

    for (;;) {
        task_work();

        uint64_t now = kernel_timer_get_ticks();

        if (now >= next) {
            (void)task_log(message);
            next = now + interval;
        }

        /* タスク切替はタイマー割込みが行う */
    }
}