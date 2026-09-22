#include <usr/terminal.h>
#include <drivers/uart/uart.h>
#include <kernel/task.h>

static void write_text(
    struct uart_device *console,
    const char *text
)
{
    while (*text) {
        uart_putc(console, *text++);
    }
}

void terminal_output_init(
    struct terminal_output *out,
    struct uart_device *console,
    bool raw
)
{
    out->console = console;
    out->raw = raw;
    out->line_start = true;
    out->line_dirty = false;
}

void terminal_output_putc(
    struct terminal_output *out,
    char c
)
{
    if (!out->raw) {
        if (c == '\r' || c == '\n') {
            out->line_start = true;

            if (c == '\n') {
                out->line_dirty = false;
            }
        } else {
            if (out->line_start) {
                uart_putc(out->console, '\t');
            }

            out->line_start = false;
            out->line_dirty = true;
        }
    }

    uart_putc(out->console, c);
}

void terminal_output_finish(struct terminal_output *out)
{
    /*
     * アプリの表示色をシェルへ持ち越さない。
     * 通常出力の最終行に改行がなければ補う。
     *
     * rawアプリは自分で画面・カーソル位置を復元する。
     */
    write_text(out->console, TERMINAL_RESET);

    if (!out->raw && out->line_dirty) {
        write_text(out->console, "\r\n");
    }

    out->line_start = true;
    out->line_dirty = false;
}

void terminal_set_color(
    struct uart_device *console,
    uint32_t color
)
{
    static const char *const codes[TASK_COLOR_COUNT] = {
        TERMINAL_RESET,
        "\033[31m",
        "\033[32m",
        "\033[33m",
        "\033[34m",
        "\033[35m",
        "\033[36m",
        "\033[37m"
    };

    /*
     * 色だけでなく、太字・反転などの属性もリセットする。
     */
    write_text(console, TERMINAL_RESET);

    if (color > 0U && color < TASK_COLOR_COUNT) {
        write_text(console, codes[color]);
    }
}

void terminal_prompt(
    struct uart_device *console,
    const char *path,
    bool discard
)
{
    write_text(console, TERMINAL_RESET);

    if (discard) {
        write_text(console, "Press Enter> ");
        return;
    }

    /*
     * KITOSだけを黄緑色にする。
     * パス・記号・入力文字は端末の標準色。
     */
    write_text(
        console,
        TERMINAL_PROMPT_COLOR "KITOS" TERMINAL_RESET ":"
    );

    if (!path || !*path) {
        path = "/";
    }

    while (*path) {
        unsigned char c = (unsigned char)*path++;

        /*
         * パスに含まれる制御文字を端末命令として解釈させない。
         */
        uart_putc(
            console,
            c < 32U || c == 127U ? '?' : (char)c
        );
    }

    write_text(console, "> ");
}

static void output_text(
    struct terminal_output *out,
    const char *text
)
{
    while (*text) {
        terminal_output_putc(out, *text++);
    }
}

void terminal_task_message(
    struct uart_device *console,
    const struct task_message *message
)
{
    struct terminal_output out;
    char digits[10];
    unsigned int n = 0U;
    uint32_t id = message->id;

    terminal_output_init(&out, console, false);
    terminal_set_color(console, message->color);

    output_text(&out, "[task ");

    do {
        digits[n++] = (char)('0' + id % 10U);
        id /= 10U;
    } while (id);

    while (n) {
        terminal_output_putc(&out, digits[--n]);
    }

    output_text(&out, "] ");

    for (
        unsigned int i = 0U;
        i < TASK_LOG_SIZE && message->text[i];
        ++i
    ) {
        unsigned char c = (unsigned char)message->text[i];

        if (c == '\n') {
            output_text(&out, "\r\n");
        } else if (c != '\r') {
            terminal_output_putc(
                &out,
                (c < 32U && c != '\t') || c == 127U
                    ? '?'
                    : (char)c
            );
        }
    }

    terminal_output_finish(&out);
}