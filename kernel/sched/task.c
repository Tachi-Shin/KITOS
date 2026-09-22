// kernel/sched/task.c
#include <kernel/task.h>
#include <arch/arm64/kernel/irq.h>

static struct task task_table[MAX_TASKS];

static uint8_t task_stacks[MAX_TASKS][TASK_STACK_SIZE]
    __attribute__((aligned(16)));

static struct task *current_task;
static struct arch_context boot_context;

/*
 * 実行可能な通常タスクがない場合の待機用タスク。
 * 通常タスクの枠やIDは消費しない。
 */
static struct task idle_task;

static uint8_t idle_stack[TASK_STACK_SIZE]
    __attribute__((aligned(16)));

/* タスク枠を再利用しても、IDは新しく割り当てる */
static uint32_t next_id = 1U;

#define LOG_COUNT 16U

static struct task_message messages[LOG_COUNT];
static unsigned int log_head;
static unsigned int log_tail;

static void copy_text(
    char *dst,
    const char *src,
    unsigned int size
)
{
    unsigned int i = 0U;

    while (i + 1U < size && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }

    dst[i] = '\0';
}

/* IRQ禁止状態で呼ぶ */
static struct task *find_task(uint32_t id)
{
    for (unsigned int i = 0U; i < MAX_TASKS; i++) {
        if (
            task_table[i].state != TASK_UNUSED &&
            task_table[i].id == id
        ) {
            return &task_table[i];
        }
    }

    return NULL;
}

/* IRQ禁止状態で呼ぶ */
static struct task *select_next_task(void)
{
    unsigned int index = MAX_TASKS - 1U;

    if (
        current_task != NULL &&
        current_task != &idle_task
    ) {
        index = (unsigned int)(current_task - task_table);
    }

    for (unsigned int i = 0U; i < MAX_TASKS; i++) {
        index = (index + 1U) % MAX_TASKS;

        if (task_table[index].state == TASK_READY) {
            return &task_table[index];
        }
    }

    /*
     * 他にREADYタスクがなくても、
     * 現在のタスクが実行可能なら継続する。
     */
    if (
        current_task != NULL &&
        current_task->state == TASK_RUNNING
    ) {
        return current_task;
    }

    /* 全タスクが待機・停止・終了している */
    return &idle_task;
}

/* IRQ禁止状態で呼ぶ */
static void switch_to_task(struct task *next)
{
    struct task *previous = current_task;

    if (next == NULL || next == previous) {
        return;
    }

    /*
     * SLEEPING、STOPPED、EXITEDなどの状態は保持する。
     */
    if (previous->state == TASK_RUNNING) {
        previous->state = TASK_READY;
    }

    next->state = TASK_RUNNING;
    current_task = next;

    arch_context_switch(
        &previous->context,
        &next->context
    );
}

static void task_bootstrap(void)
{
    struct task *task = current_task;

    /* 新しいタスクのスタックに切り替わってからIRQを許可 */
    arch_irq_restore(0U);

    task->entry(task->argument);

    /* タスク関数から戻った場合は終了処理へ */
    task_exit();
}

static void idle_entry(void *argument)
{
    (void)argument;

    for (;;) {
        arch_wait_for_irq();
    }
}

/*
 * タイマー割込み中に呼ばれる。
 * 実行可能状態に戻すだけで、ここでは切り替えない。
 */
static void wake_sleeping_task(void *argument)
{
    struct task *task = argument;

    if (task->state == TASK_SLEEPING) {
        task->state = TASK_READY;
    }

    /*
     * STOPPEDなら、期限が到来しても停止状態を維持する。
     * resume時にタイマーの登録状態を確認する。
     */
}

int task_sleep(uint32_t id, uint32_t milliseconds)
{
    uint64_t flags = arch_irq_save();

    struct task *task = find_task(id);
    int result = -1;

    /*
     * シェル自身や保護タスクは休止させない。
     * 停止中・終了済み・別の理由で待機中のタスクも対象外。
     */
    if (milliseconds == 0U ||
        task == NULL ||
        task == current_task ||
        task->protected ||
        (task->state != TASK_READY &&
         task->state != TASK_SLEEPING)) {
        goto done;
    }

    if (kernel_timer_arm(
            &task->sleep_timer,
            milliseconds,
            wake_sleeping_task,
            task
        ) == 0) {
        task->state = TASK_SLEEPING;
        result = 0;
    }

done:
    arch_irq_restore(flags);
    return result;
}

int task_create(
    const char *name,
    void (*entry)(void *),
    void *argument
)
{
    if (name == NULL || entry == NULL) {
        return -1;
    }

    uint64_t flags = arch_irq_save();
    int result = -1;

    if (next_id > 0x7FFFFFFFU) {
        goto done;
    }

    for (unsigned int i = 0U; i < MAX_TASKS; i++) {
        struct task *task = &task_table[i];

        /*
         * 現在実行中のタスクのスタックは再利用しない。
         * 未使用か終了済みの枠を探す。
         */
        if (
            task == current_task ||
            (
                task->state != TASK_UNUSED &&
                task->state != TASK_EXITED
            )
        ) {
            continue;
        }

        task->id = next_id++;

        copy_text(
            task->task_name,
            name,
            TASK_NAME_SIZE
        );

        task->entry = entry;
        task->argument = argument;

        task->color = 0U;
        task->protected = false;
        task->work = 0U;

        task->stack_bottom = &task_stacks[i][0];
        task->stack_top = &task_stacks[i][TASK_STACK_SIZE];

        /*
         * 再利用する枠に古いタイマーを残さない。
         */
        (void)kernel_timer_cancel(&task->sleep_timer);
        task->sleep_timer = (struct timer_event){0};

        arch_context_init(
            &task->context,
            task->stack_top,
            task_bootstrap
        );

        /* 初期化完了後に実行可能にする */
        task->state = TASK_READY;

        result = (int)task->id;
        break;
    }

done:
    arch_irq_restore(flags);
    return result;
}

void task_yield(void)
{
    uint64_t flags = arch_irq_save();

    if (current_task != NULL) {
        switch_to_task(select_next_task());
    }

    arch_irq_restore(flags);
}

int task_sleep_ms(uint32_t milliseconds)
{
    uint64_t flags = arch_irq_save();

    /*
     * IRQ禁止中や割込みハンドラ内からのsleepを拒否する。
     */
    if (
        (flags & (1ULL << 7)) != 0U ||
        current_task == NULL ||
        current_task == &idle_task ||
        current_task->state != TASK_RUNNING
    ) {
        arch_irq_restore(flags);
        return -1;
    }

    if (milliseconds != 0U) {
        if (
            kernel_timer_arm(
                &current_task->sleep_timer,
                milliseconds,
                wake_sleeping_task,
                current_task
            ) != 0
        ) {
            arch_irq_restore(flags);
            return -1;
        }

        current_task->state = TASK_SLEEPING;
    }

    /*
     * milliseconds == 0の場合は、
     * 状態をRUNNINGのまま切替処理へ渡してyieldする。
     *
     * 正の待機時間ならSLEEPING状態なので、
     * 次回以降の実行対象から外れる。
     */
    switch_to_task(select_next_task());

    /*
     * タイマーによる復帰後、再びこのタスクが選ばれると
     * ここから処理が再開する。
     */
    arch_irq_restore(flags);

    return 0;
}

void task_exit(void)
{
    (void)arch_irq_save();

    if (current_task != NULL) {
        (void)kernel_timer_cancel(&current_task->sleep_timer);

        current_task->state = TASK_EXITED;

        switch_to_task(select_next_task());
    }

    arch_irq_restore(0U);

    for (;;) {
        arch_wait_for_irq();
    }
}

void task_start(void)
{
    uint64_t flags = arch_irq_save();

    if (current_task == NULL) {
        idle_task.entry = idle_entry;
        idle_task.protected = true;

        idle_task.stack_bottom = idle_stack;
        idle_task.stack_top = idle_stack + sizeof(idle_stack);

        arch_context_init(
            &idle_task.context,
            idle_task.stack_top,
            task_bootstrap
        );

        current_task = select_next_task();
        current_task->state = TASK_RUNNING;

        arch_context_switch(
            &boot_context,
            &current_task->context
        );
    }

    arch_irq_restore(flags);
}

int task_protect(uint32_t id)
{
    uint64_t flags = arch_irq_save();

    struct task *task = find_task(id);
    int result = -1;

    if (
        task != NULL &&
        task->state != TASK_EXITED
    ) {
        task->protected = true;
        result = 0;
    }

    arch_irq_restore(flags);

    return result;
}

int task_control(
    uint32_t id,
    enum task_operation operation
)
{
    uint64_t flags = arch_irq_save();

    struct task *task = find_task(id);
    int result = -1;

    if (task == NULL ||
        task == current_task ||
        task->protected) {
        goto done;
    }

    if (operation == TASK_STOP &&
        (task->state == TASK_READY ||
         task->state == TASK_SLEEPING)) {
        /* 自動復帰を解除して、無期限停止へ変更する。 */
        (void)kernel_timer_cancel(&task->sleep_timer);

        task->state = TASK_STOPPED;
        result = 0;

    } else if (operation == TASK_RESUME &&
               (task->state == TASK_STOPPED ||
                task->state == TASK_SLEEPING)) {
        /* sleepの期限を待たず、実行可能にする。 */
        (void)kernel_timer_cancel(&task->sleep_timer);

        task->state = TASK_READY;
        result = 0;

    } else if (operation == TASK_KILL &&
               (task->state == TASK_READY ||
                task->state == TASK_STOPPED ||
                task->state == TASK_SLEEPING)) {
        /* 終了後に古いタイマーで復活しないよう解除する。 */
        (void)kernel_timer_cancel(&task->sleep_timer);

        task->state = TASK_EXITED;
        result = 0;
    }

done:
    arch_irq_restore(flags);
    return result;
}

int task_set_color(
    uint32_t id,
    uint32_t color
)
{
    uint64_t flags = arch_irq_save();

    struct task *task = find_task(id);
    int result = -1;

    if (
        task != NULL &&
        task->state != TASK_EXITED &&
        color < TASK_COLOR_COUNT
    ) {
        task->color = color;
        result = 0;
    }

    arch_irq_restore(flags);

    return result;
}

unsigned int task_snapshot(
    struct task_info *out,
    unsigned int capacity
)
{
    if (out == NULL) {
        return 0U;
    }

    uint64_t flags = arch_irq_save();
    unsigned int n = 0U;

    for (
        unsigned int i = 0U;
        i < MAX_TASKS && n < capacity;
        i++
    ) {
        struct task *task = &task_table[i];

        if (task->state == TASK_UNUSED) {
            continue;
        }

        out[n].id = task->id;
        out[n].state = task->state;
        out[n].color = task->color;
        out[n].work = task->work;

        out[n].stack_bottom = task->stack_bottom;
        out[n].stack_top = task->stack_top;

        copy_text(
            out[n].name,
            task->task_name,
            TASK_NAME_SIZE
        );

        n++;
    }

    arch_irq_restore(flags);

    return n;
}

void task_work(void)
{
    if (current_task != NULL) {
        current_task->work++;
    }
}

/*
 * 通常のタスクから呼ぶ。
 * 最大63文字のメッセージをキューへ保存する。
 * キューが満杯なら待たずにfalseを返す。
 */
bool task_log(const char *text)
{
    if (text == NULL) {
        return false;
    }

    uint64_t flags = arch_irq_save();

    unsigned int next = (log_head + 1U) % LOG_COUNT;
    bool accepted = false;

    if (
        current_task != NULL &&
        next != log_tail
    ) {
        messages[log_head].id = current_task->id;
        messages[log_head].color = current_task->color;

        copy_text(
            messages[log_head].text,
            text,
            TASK_LOG_SIZE
        );

        log_head = next;
        accepted = true;
    }

    arch_irq_restore(flags);

    return accepted;
}

bool task_read_log(struct task_message *out)
{
    if (out == NULL) {
        return false;
    }

    uint64_t flags = arch_irq_save();

    bool available = log_tail != log_head;

    if (available) {
        *out = messages[log_tail];
        log_tail = (log_tail + 1U) % LOG_COUNT;
    }

    arch_irq_restore(flags);

    return available;
}