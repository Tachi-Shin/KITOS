// kernel/time/timer.c
#include <dos/type.h>
#include <dos/config.h>

#include <kernel/timer.h>
#include <arch/arm64/kernel/irq.h>

static volatile uint64_t timer_ticks;
static struct timer_event *events;

/*
 * uint64_tの周回を考慮した順序比較。
 * 比較する期限の距離が2^63未満であることを前提とする。
 */
static bool before(uint64_t a, uint64_t b)
{
    return (a - b) > (1ULL << 63);
}

/* IRQ禁止状態で呼ぶ。 */
static bool cancel_locked(struct timer_event *event)
{
    struct timer_event **link = &events;

    if (event == NULL || !event->armed) {
        return false;
    }

    while (*link != NULL && *link != event) {
        link = &(*link)->next;
    }

    if (*link == NULL) {
        return false;
    }

    *link = event->next;

    event->next = NULL;
    event->armed = false;

    return true;
}

bool kernel_timer_cancel(struct timer_event *event)
{
    uint64_t flags = arch_irq_save();

    bool cancelled = cancel_locked(event);

    arch_irq_restore(flags);
    return cancelled;
}

int kernel_timer_arm(
    struct timer_event *event,
    uint32_t milliseconds,
    void (*callback)(void *),
    void *argument
)
{
    if (event == NULL || callback == NULL || milliseconds == 0U) {
        return -1;
    }

    uint64_t flags = arch_irq_save();

    /* 再登録なら、以前の期限を解除する。 */
    (void)cancel_locked(event);

    uint64_t wait_ticks =
        ((uint64_t)milliseconds + TIMER_INTERVAL_MS - 1U)
        / TIMER_INTERVAL_MS;

    /*
     * 登録直後に割込みが来ても早く復帰しないよう、
     * 切り上げた待機時間に1tick追加する。
     */
    event->deadline_ticks = timer_ticks + wait_ticks + 1ULL;
    event->callback = callback;
    event->argument = argument;

    /* 期限順に挿入する。同一期限は登録順。 */
    struct timer_event **link = &events;

    while (*link != NULL &&
           !before(event->deadline_ticks, (*link)->deadline_ticks)) {
        link = &(*link)->next;
    }

    event->next = *link;
    *link = event;
    event->armed = true;

    arch_irq_restore(flags);
    return 0;
}

bool kernel_timer_tick(void)
{
    uint64_t flags = arch_irq_save();

    timer_ticks++;
    uint64_t now = timer_ticks;

    while (events != NULL &&
           !before(now, events->deadline_ticks)) {
        struct timer_event *event = events;

        void (*callback)(void *) = event->callback;
        void *argument = event->argument;

        /*
         * コールバックより先にリストから外す。
         * コールバック内での再登録にも対応する。
         */
        events = event->next;
        event->next = NULL;
        event->armed = false;

        callback(argument);
    }

    arch_irq_restore(flags);

    /*
     * 切替を要求する。
     * 実際の切替は、既存のirq_dispatch()が
     * GICへの割込み完了通知を済ませた後に行う。
     */
    return true;
}

uint64_t kernel_timer_get_ticks(void)
{
    return timer_ticks;
}