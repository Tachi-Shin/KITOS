#ifndef KERNEL_TASK_H
#define KERNEL_TASK_H

#include <dos/type.h>
#include <dos/config.h>
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

    void *stack_bottom;
    void *stack_top;

    struct arch_context context;
};

struct task_info {
    uint32_t id;
    uint32_t state;
    uint32_t color;

    uint64_t work;

    char name[TASK_NAME_SIZE];
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