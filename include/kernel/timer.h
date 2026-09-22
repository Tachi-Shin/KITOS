// include/kernel/timer.h
#ifndef KERNEL_TIME_TIMER_H
#define KERNEL_TIME_TIMER_H

#include <dos/type.h>

/*
 * 初回使用前にゼロ初期化する。
 * 登録中は、この構造体のメモリを解放しないこと。
 *
 * callbackはIRQ禁止状態で呼ばれる。
 * 待機やコンテキスト切替は行わないこと。
 */
struct timer_event {
    uint64_t deadline_ticks;

    void (*callback)(void *);
    void *argument;

    struct timer_event *next;
    bool armed;
};

/*
 * 同じイベントを再登録すると期限を更新する。
 * milliseconds == 0 はエラー。
 */
int kernel_timer_arm(
    struct timer_event *event,
    uint32_t milliseconds,
    void (*callback)(void *),
    void *argument
);

bool kernel_timer_cancel(struct timer_event *event);

bool kernel_timer_tick(void);
uint64_t kernel_timer_get_ticks(void);

#endif