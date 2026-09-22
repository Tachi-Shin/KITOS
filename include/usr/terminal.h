#ifndef USR_TERMINAL_H
#define USR_TERMINAL_H

#include <dos/type.h>

struct uart_device;
struct task_message;

#define TERMINAL_RESET "\033[0m"

/* ANSI 256色の154番：黄緑色 */
#define TERMINAL_PROMPT_COLOR "\033[38;5;154m"

/*
 * 通常出力では行頭にTABを追加する。
 * カーソル移動や全画面表示を行うアプリはrawを使用する。
 */
struct terminal_output {
    struct uart_device *console;
    bool raw;
    bool line_start;
    bool line_dirty;
};

void terminal_output_init(
    struct terminal_output *out,
    struct uart_device *console,
    bool raw
);

void terminal_output_putc(
    struct terminal_output *out,
    char c
);

void terminal_output_finish(struct terminal_output *out);

void terminal_set_color(
    struct uart_device *console,
    uint32_t color
);

void terminal_prompt(
    struct uart_device *console,
    const char *path,
    bool discard
);

void terminal_task_message(
    struct uart_device *console,
    const struct task_message *message
);

#endif