#include <usr/shell.h>

#include <kernel/task.h>
#include <kernel/printk.h>

#include <usr/apps.h>

#include <drivers/uart/uart.h>

#define LINE_SIZE 64U

static struct uart_device *console;

static char line[LINE_SIZE];
static unsigned int length;
static unsigned int escape_state;

static bool skip_lf;
static bool discard_line;

void shell_color(uint32_t color)
{
    static const char *const codes[TASK_COLOR_COUNT] = {
        "\033[0m",
        "\033[31m",
        "\033[32m",
        "\033[33m",
        "\033[34m",
        "\033[35m",
        "\033[36m",
        "\033[37m"
    };

    printk(
        "%s",
        codes[color < TASK_COLOR_COUNT ? color : 0U]
    );
}

static void prompt(void)
{
    shell_color(0U);

    printk(
        "%s",
        discard_line ? "Press Enter> " : "DOS> "
    );

    for (unsigned int i = 0U; i < length; i++) {
        uart_putc(console, line[i]);
    }
}

static void cancel_line(
    const char *message,
    bool discard
)
{
    length = 0U;
    escape_state = 0U;
    skip_lf = false;
    discard_line = discard;

    printk("\r\033[2K%s\n", message);
    prompt();
}

static void accept_char(unsigned char c)
{
    /* CRLFは1回のEnterとして扱う */
    if (c == '\n' && skip_lf) {
        skip_lf = false;
        return;
    }

    skip_lf = false;

    /* Ctrl+C：入力行を取り消す */
    if (c == 3U) {
        cancel_line("^C", false);
        return;
    }

    /* Enter */
    if (c == '\r' || c == '\n') {
        skip_lf = c == '\r';

        printk("\n");

        line[length] = '\0';

        if (!discard_line) {
            shell_execute(line);
        }

        length = 0U;
        escape_state = 0U;
        discard_line = false;

        prompt();
        return;
    }

    /*
     * 受信欠落・行の長さ超過があった場合は、
     * Enterまで残りの文字を読み捨てる。
     */
    if (discard_line) {
        return;
    }

    /* 一般的な矢印キーの制御シーケンスを読み飛ばす */
    if (escape_state == 1U) {
        escape_state =
            (c == '[' || c == 'O') ? 2U : 0U;
        return;
    }

    if (escape_state == 2U) {
        if (c >= 0x40U && c <= 0x7EU) {
            escape_state = 0U;
        }

        return;
    }

    if (c == 27U) {
        escape_state = 1U;
        return;
    }

    /* Ctrl+U：入力行を消す */
    if (c == 21U) {
        length = 0U;

        printk("\r\033[2K");
        prompt();

        return;
    }

    /* Backspace / DEL */
    if (c == '\b' || c == 127U) {
        if (length != 0U) {
            length--;
            printk("\b \b");
        }

        return;
    }

    /* タブは空白として扱う */
    if (c == '\t') {
        c = ' ';
    }

    /* この版ではASCIIの表示可能文字を受け付ける */
    if (c < 0x20U || c > 0x7EU) {
        return;
    }

    if (length + 1U >= LINE_SIZE) {
        cancel_line(
            "Line too long. Input cancelled.",
            true
        );
        return;
    }

    line[length++] = (char)c;

    uart_putc(console, (char)c);
}

void shell_task(void *argument)
{
    (void)argument;

    console = uart_get_device(0U);

    printk("\nDOS shell. Type help.\n");
    prompt();

    for (;;) {
        bool active = false;

        /* UARTの受信バッファから文字を取得する */
        for (unsigned int i = 0U; i < 32U; i++) {
            char c;

            int result = uart_try_getc(console, &c);

            if (result == 0) {
                break;
            }

            active = true;

            if (result == UART_RX_LOST) {
                cancel_line(
                    "Input lost. Input cancelled.",
                    true
                );
                break;
            }

            if (result < 0) {
                printk("\nUART receive is unavailable.\n");
                return;
            }

            accept_char((unsigned char)c);
        }

        /* 他タスクのメッセージを表示する */
        for (unsigned int i = 0U; i < 4U; i++) {
            struct task_message message;

            if (!task_read_log(&message)) {
                break;
            }

            active = true;

            /*
             * 現在の入力行を消し、
             * メッセージ表示後に再描画する。
             */
            printk("\r\033[2K");

            shell_color(message.color);

            printk(
                "[task %u] %s\n",
                message.id,
                message.text
            );

            prompt();
        }

        /*
         * UARTまたはタイマー割込みを待つ。
         * 他タスクへの切替はタイマー割込みで行われる。
         */
        if (!active) {
            __asm__ volatile("wfi" ::: "memory");
        }
    }
}