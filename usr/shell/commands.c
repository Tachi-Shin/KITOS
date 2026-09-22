#include <usr/shell.h>

#include <kernel/task.h>
#include <kernel/printk.h>

#include <usr/apps.h>

#include <drivers/uart/uart.h>

#define ARG_COUNT 8U

static const char *const color_names[TASK_COLOR_COUNT] = {
    "default",
    "red",
    "green",
    "yellow",
    "blue",
    "magenta",
    "cyan",
    "white"
};

static bool equal(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }

    return *a == *b;
}

static bool parse_id(const char *text, uint32_t *out)
{
    uint32_t value = 0U;

    if (*text == '\0') {
        return false;
    }

    while (*text != '\0') {
        if (*text < '0' || *text > '9') {
            return false;
        }

        uint32_t digit = (uint32_t)(*text++ - '0');

        if (value > (0x7FFFFFFFU - digit) / 10U) {
            return false;
        }

        value = value * 10U + digit;
    }

    if (value == 0U) {
        return false;
    }

    *out = value;

    return true;
}

/*
 * sleep 3      : 3秒
 * sleep 3s     : 3秒
 * sleep 500ms  : 500ミリ秒
 * sleep 0      : yield
 */
static bool parse_sleep_ms(
    const char *text,
    uint32_t *milliseconds
)
{
    const char *p = text;
    uint32_t value;

    if (!app_uint(&p, &value, 0xFFFFFFFFU)) {
        return false;
    }

    if (equal(p, "ms")) {
        *milliseconds = value;
        return true;
    }

    if (
        (!equal(p, "") && !equal(p, "s")) ||
        value > 0xFFFFFFFFU / 1000U
    ) {
        return false;
    }

    *milliseconds = value * 1000U;

    return true;
}

static const char *state_name(uint32_t state)
{
    switch (state) {
    case TASK_READY:
        return "READY";

    case TASK_RUNNING:
        return "RUNNING";

    case TASK_BLOCKED:
        return "BLOCKED";

    case TASK_SLEEPING:
        return "SLEEPING";

    case TASK_STOPPED:
        return "STOPPED";

    case TASK_EXITED:
        return "EXITED";

    default:
        return "UNUSED";
    }
}

static void show_tasks(void)
{
    struct task_info tasks[MAX_TASKS];

    unsigned int count =
        task_snapshot(tasks, MAX_TASKS);

    printk("ID NAME STATE WORK COLOR\n");

    for (unsigned int i = 0U; i < count; i++) {
        struct task_info *t = &tasks[i];

        /* 終了済みのタスクは表示しない。 */
        if (t->state == TASK_EXITED) {
            continue;
        }

        shell_color(t->color);

        printk(
            "%u %s %s %llu %s\n",
            t->id,
            t->name,
            state_name(t->state),
            (unsigned long long)t->work,
            color_names[t->color]
        );

        /*
         * stack_topは領域の直後。
         * 表示するSIZEは確保容量であり、現在の使用量ではない。
         */
        printk(
            "  STACK [%p, %p) SIZE=%lu bytes\n",
            t->stack_bottom,
            t->stack_top,
            (unsigned long)(
                (uintptr_t)t->stack_top -
                (uintptr_t)t->stack_bottom
            )
        );
    }

    shell_color(0U);
}

static void show_uart(void)
{
    struct uart_rx_stats stats;

    uart_get_rx_stats(
        uart_get_device(0U),
        &stats
    );

    printk(
        "irq=%llu rx=%llu drop=%llu overrun=%llu\n",
        (unsigned long long)stats.interrupts,
        (unsigned long long)stats.bytes,
        (unsigned long long)stats.dropped,
        (unsigned long long)stats.overruns
    );
}

/*
 * psからSLEEPING状態を観察するためのデモ。
 */
static void sleep_demo_task(void *argument)
{
    const char *message = argument;

    for (;;) {
        if (task_sleep_ms(1000U) != 0) {
            return;
        }

        task_work();

        (void)task_log(message);
    }
}

static void run_app(const char *name)
{
    static const struct {
        const char *name;
        const char *message;
        void (*entry)(void *);
    } apps[] = {
        {
            "demo_a",
            "demo_a is running",
            demo_task
        },
        {
            "demo_b",
            "demo_b is running",
            demo_task
        },
        {
            "demo_sleep",
            "demo_sleep woke up",
            sleep_demo_task
        }
    };

    for (
        unsigned int i = 0U;
        i < sizeof(apps) / sizeof(apps[0]);
        i++
    ) {
        if (!equal(name, apps[i].name)) {
            continue;
        }

        int id = task_create(
            apps[i].name,
            apps[i].entry,
            (void *)apps[i].message
        );

        if (id < 0) {
            printk(
                "Cannot create task: no available task slot/ID.\n"
            );
        } else {
            printk(
                "Started %s: id=%u\n",
                apps[i].name,
                (unsigned int)id
            );
        }

        return;
    }

    printk(
        "Unknown application. Available: demo_a demo_b demo_sleep\n"
    );
}

void shell_execute(char *line)
{
    char *argv[ARG_COUNT];
    unsigned int argc = 0U;
    char *p = line;

    if (apps_shell_command(uart_get_device(0), line)) {
        return;
    }

    /* 空白・タブで引数を分割する。 */
    while (*p != '\0') {
        while (*p == ' ' || *p == '\t') {
            p++;
        }

        if (*p == '\0') {
            break;
        }

        if (argc == ARG_COUNT) {
            printk("Too many arguments (maximum 8).\n");
            return;
        }

        argv[argc++] = p;

        while (*p != '\0' && *p != ' ' && *p != '\t') {
            p++;
        }

        if (*p != '\0') {
            *p++ = '\0';
        }
    }

    if (argc == 0U) {
        return;
    }

    if (equal(argv[0], "help") && argc == 1U) {
        printk("help | echo TEXT | clear | ps | uart\n");
        printk("sleep ID N[s|ms] (positive integer; default: seconds)\n");
        printk("run demo_a|demo_b|demo_sleep\n");
        printk("stop ID | resume ID | kill ID | color ID COLOR\n");
        printk("Colors: default red green yellow blue magenta cyan white\n");

    } else if (equal(argv[0], "echo")) {
        for (unsigned int i = 1U; i < argc; i++) {
            if (i != 1U) {
                printk(" ");
            }

            printk("%s", argv[i]);
        }

        printk("\n");

    } else if (equal(argv[0], "clear") && argc == 1U) {
        printk("\033[2J\033[H");

    } else if (equal(argv[0], "ps") && argc == 1U) {
        show_tasks();

    } else if (equal(argv[0], "sleep")) {
        uint32_t id;
        uint32_t milliseconds;

        if (argc != 3U ||
            !parse_id(argv[1], &id) ||
            !parse_sleep_ms(argv[2], &milliseconds) ||
            milliseconds == 0U) {
            printk(
                "Usage: sleep ID N[s|ms] "
                "(positive integer; default: seconds)\n"
            );
            return;
        }

        if (task_sleep(id, milliseconds) == 0) {
            printk("OK: sleep %u %ums\n", id, milliseconds);
        } else {
            printk(
                "Cannot sleep: missing/protected task or invalid state.\n"
            );
        }

    } else if (equal(argv[0], "uart") && argc == 1U) {
        show_uart();

    } else if (equal(argv[0], "run") && argc == 2U) {
        run_app(argv[1]);

    } else if (equal(argv[0], "stop") ||
               equal(argv[0], "resume") ||
               equal(argv[0], "kill")) {
        uint32_t id;

        if (argc != 2U || !parse_id(argv[1], &id)) {
            printk("Usage: %s ID\n", argv[0]);
            return;
        }

        enum task_operation operation =
            equal(argv[0], "stop") ? TASK_STOP :
            equal(argv[0], "resume") ? TASK_RESUME :
            TASK_KILL;

        if (task_control(id, operation) == 0) {
            printk("OK: %s %u\n", argv[0], id);
        } else {
            printk(
                "Cannot %s: missing/protected task or invalid state.\n",
                argv[0]
            );
        }

    } else if (equal(argv[0], "color")) {
        uint32_t id;

        if (argc != 3U || !parse_id(argv[1], &id)) {
            printk("Usage: color ID COLOR\n");
            return;
        }

        for (uint32_t color = 0U;
             color < TASK_COLOR_COUNT;
             color++) {
            if (!equal(argv[2], color_names[color])) {
                continue;
            }

            if (task_set_color(id, color) == 0) {
                printk("OK: color %u %s\n", id, argv[2]);
            } else {
                printk("No live task with that ID.\n");
            }

            return;
        }

        printk("Unknown color. See help.\n");

    } else {
        printk("Unknown command or wrong arguments. See help.\n");
    }
}