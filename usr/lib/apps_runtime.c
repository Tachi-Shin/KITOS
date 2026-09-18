#include <usr/apps.h>
#include <usr/fs_commands.h>
#include <drivers/uart/uart.h>
#include <kernel/task.h>
#include <stdatomic.h>

#define TYPEAHEAD_CAP 1024u
#define RX_POLL_BUDGET 32u

/* All large state goes in .bss, never on a task's stack. */
static struct basic_context basic;
static struct editor_context editor;
static struct {
    struct uart_device *console;
    struct app_io io;
    char queue[TYPEAHEAD_CAP];
    size_t head, count;
    unsigned dropping, lost, interrupted, closed, active;
    atomic_uint done;
} terminal;

static void clear_queue(void) {
    terminal.head = 0;
    terminal.count = 0;
}

static int pop(char *out) {
    if (!terminal.count) return 0;
    *out = terminal.queue[terminal.head];
    terminal.head = (terminal.head + 1u) % TYPEAHEAD_CAP;
    --terminal.count;
    return 1;
}

static void lost_input(void) {
    clear_queue();
    terminal.dropping = 1;
    terminal.lost = 1;
}

/* Called in task context. Preserve normal input while scanning for Ctrl-C. */
static void pump(void) {
    unsigned i;
    if (terminal.closed) return;
    for (i = 0; i < RX_POLL_BUDGET; ++i) {
        char c;
        int rc = uart_try_getc(terminal.console, &c);
        if (!rc) break;
        if (rc == UART_RX_LOST) {
            lost_input();
            break;
        }
        if (rc < 0) {
            terminal.closed = 1;
            terminal.interrupted = 1;
            break;
        }
        if (c == 3) {
            clear_queue();
            terminal.dropping = 0;
            terminal.interrupted = 1;
            break;
        }
        if (terminal.dropping) {
            if (c == '\r' || c == '\n') terminal.dropping = 0;
            continue;
        }
        if (terminal.count == TYPEAHEAD_CAP) {
            lost_input();
            if (c == '\r' || c == '\n') terminal.dropping = 0;
            break;
        }
        terminal.queue[(terminal.head + terminal.count) % TYPEAHEAD_CAP] = c;
        ++terminal.count;
    }
}

static void output(void *unused, char c) {
    (void)unused;
    uart_putc(terminal.console, c);
}

static int take_interrupt(void) {
    if (terminal.lost) {
        terminal.lost = 0;
        terminal.interrupted = 0;
        app_puts(&terminal.io, "\nInput lost; press Enter to resync.\n");
        return 1;
    }
    if (terminal.interrupted) {
        terminal.interrupted = 0;
        return 1;
    }
    return 0;
}

static int cancelled(void *unused) {
    (void)unused;
    pump();
    return take_interrupt();
}

static int input(void *unused, char *out) {
    (void)unused;
    if (!terminal.count) pump();
    if (take_interrupt()) {
        *out = 3;
        return 1;
    }
    if (pop(out)) return 1;
    return terminal.closed ? -1 : 0;
}

static void relinquish(void *unused) {
    (void)unused;
    task_yield();
}

static void finished(void *unused) {
    (void)unused;
    atomic_store_explicit(&terminal.done, 1u, memory_order_release);
}

/* Shell side: drain typeahead left by the app before consulting UART RX. */
int apps_console_try_getc(struct uart_device *console, char *out) {
    unsigned i;
    if (!console || !out) return -1;
    if (console != terminal.console) return uart_try_getc(console, out);
    if (terminal.active) return 0; /* Only the foreground child consumes input. */
    if (terminal.lost) {
        terminal.lost = 0;
        return UART_RX_LOST;
    }
    if (terminal.interrupted) {
        terminal.interrupted = 0;
        *out = 3;
        return 1;
    }
    for (i = 0; i < RX_POLL_BUDGET; ++i) {
        int rc = pop(out);
        if (!rc) rc = uart_try_getc(console, out);
        if (rc == UART_RX_LOST) {
            clear_queue();
            terminal.dropping = 1;
            return rc;
        }
        if (rc <= 0) return rc;
        if (terminal.dropping) {
            if (*out == '\r' || *out == '\n') terminal.dropping = 0;
            continue;
        }
        if (terminal.io.skip_lf) {
            terminal.io.skip_lf = 0;
            if (*out == '\n') continue;
        }
        if (*out == '\r') terminal.io.skip_lf = 1;
        return 1;
    }
    return 0;
}

int apps_run_foreground(struct uart_device *console, enum app_program program) {
    int id;
    if (!console || terminal.active) return -1;
    if (program != APP_PROGRAM_BASIC && program != APP_PROGRAM_EDITOR) return -1;
    if (terminal.console != console) {
        clear_queue();
        terminal.dropping = 0;
        terminal.lost = 0;
        terminal.interrupted = 0;
    }
    terminal.console = console;
    terminal.closed = 0;
    terminal.io.user = NULL;
    terminal.io.try_getc = input;
    terminal.io.putc = output;
    terminal.io.yield = relinquish;
    terminal.io.cancelled = cancelled;
    terminal.io.finished = finished;
    terminal.io.fs = NULL;
    terminal.io.read_file = apps_fs_read;
    terminal.io.write_file = apps_fs_write;
    terminal.io.resolve_path = apps_fs_resolve;
    /* The shell has just consumed Enter. Ignore a possible trailing LF. */
    terminal.io.skip_lf = 1;
    terminal.io.escape_state = 0;
    atomic_store_explicit(&terminal.done, 0u, memory_order_relaxed);
    terminal.active = 1;
    if (program == APP_PROGRAM_BASIC) {
        basic.io = &terminal.io;
        id = task_create("basic", basic_task, &basic);
    } else {
        editor.io = &terminal.io;
        id = task_create("editor", editor_task, &editor);
    }
    if (id < 0) {
        terminal.active = 0;
        return -1;
    }
    while (!atomic_load_explicit(&terminal.done, memory_order_acquire)) task_yield();
    terminal.active = 0;
    return id;
}

/* Return 1 if handled, 0 if the existing shell should process this command. */
int apps_shell_command(struct uart_device *console, const char *line) {
    enum app_program program;
    const char *p = line;
    if (!console || !line) return 0;
    if (apps_fs_command(console, line)) return 1;
    (void)app_keyword(&p, "RUN");
    if (app_keyword(&p, "BASIC")) program = APP_PROGRAM_BASIC;
    else if (app_keyword(&p, "EDIT") || app_keyword(&p, "EDITOR")) program = APP_PROGRAM_EDITOR;
    else return 0;
    if (!app_end(p)) return 0;
    if (apps_run_foreground(console, program) < 0) {
        const char *message = "Cannot start application\r\n";
        while (*message) uart_putc(console, *message++);
    }
    return 1;
}
 