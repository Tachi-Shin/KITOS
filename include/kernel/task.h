#ifndef KERNEL_TASK_H
#define KERNEL_TASK_H

#include <dos/type.h>
#include <dos/config.h>

#include <kernel/timer.h>

#include <arch/arm64/kernel/context.h>

#define TASK_NAME_SIZE   16U
#define TASK_LOG_SIZE    64U
#define TASK_COLOR_COUNT 8U

struct task {
    uint32_t id;
    char task_name[TASK_NAME_SIZE];

    uint32_t state;
    uint32_t color;

    bool protected;
    volatile uint64_t work;

    void (*entry)(void *);
    void *argument;

    /*
     * スタック領域は[stack_bottom, stack_top)。
     * stack_topは最後のバイトの直後を指す。
     */
    void *stack_bottom;
    void *stack_top;

    /* このタスクのsleepで使用するタイマー */
    struct timer_event sleep_timer;

    struct arch_context context;
};

struct task_info {
    uint32_t id;
    uint32_t state;
    uint32_t color;

    uint64_t work;

    char name[TASK_NAME_SIZE];

    void *stack_bottom;
    void *stack_top;
};

struct task_message {
    uint32_t id;
    uint32_t color;

    char text[TASK_LOG_SIZE];
};

enum task_operation {
    TASK_STOP,
    TASK_RESUME,
    TASK_KILL
};

int task_create(
    const char *name,
    void (*entry)(void *),
    void *argument
);

void task_start(void);
void task_yield(void);

/*
 * IRQが有効な通常のタスクから呼ぶ。
 * milliseconds == 0の場合はCPUを譲る。
 *
 * 成功時0、実行コンテキストが不正な場合は-1。
 */
int task_sleep_ms(uint32_t milliseconds);

/* 指定した別タスクを休止させる。millisecondsは1以上。 */
int task_sleep(uint32_t id, uint32_t milliseconds);

void task_exit(void);

int task_protect(uint32_t id);

int task_control(
    uint32_t id,
    enum task_operation operation
);

int task_set_color(
    uint32_t id,
    uint32_t color
);

unsigned int task_snapshot(
    struct task_info *out,
    unsigned int capacity
);

/* デモ用の処理回数カウンタ */
void task_work(void);

/* タスクから表示メッセージを登録する */
bool task_log(const char *text);

/* シェルから表示メッセージを取得する */
bool task_read_log(struct task_message *out);

#endif