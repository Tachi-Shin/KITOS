#include <usr/shell.h>

#include <kernel/task.h>
#include <kernel/printk.h>
#include <arch/arm64/kernel/irq.h>

#include <usr/apps.h>
#include <usr/fs_commands.h>
#include <usr/terminal.h>

#include <drivers/uart/uart.h>

#define LINE_SIZE    64U
#define HISTORY_SIZE 16U

static struct uart_device *console;

static char line[LINE_SIZE];
static unsigned int length;
static unsigned int escape_state;

static bool skip_lf;
static bool discard_line;

/* 履歴と、履歴をたどる前に入力していた行。 */
static char history[HISTORY_SIZE][LINE_SIZE];
static char saved_line[LINE_SIZE];

/* 次に履歴を書き込む位置と、有効な履歴の件数。 */
static unsigned int history_next;
static unsigned int history_count;

/* 0は入力途中の行、1は最新の履歴。 */
static unsigned int history_offset;

static unsigned int copy_line(char *dst, const char *src)
{
    unsigned int n = 0U;

    while (n + 1U < LINE_SIZE && src[n] != '\0') {
        dst[n] = src[n];
        n++;
    }

    dst[n] = '\0';
    return n;
}

static bool same_line(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }

    return *a == *b;
}

static void clear_input(void)
{
    length = 0U;
    line[0] = '\0';

    escape_state = 0U;
    history_offset = 0U;
    saved_line[0] = '\0';
}

/*
 * shell_execute()は引数の解析時にlineを書き換えるため、
 * 実行前に履歴へコピーする。
 */
static void history_add(void)
{
    unsigned int i = 0U;

    while (line[i] == ' ' || line[i] == '\t') {
        i++;
    }

    /* 空行・空白だけの行は保存しない。 */
    if (line[i] == '\0') {
        return;
    }

    /* 直前と完全に同じコマンドは重複保存しない。 */
    if (history_count != 0U) {
        unsigned int latest =
            (history_next + HISTORY_SIZE - 1U) % HISTORY_SIZE;

        if (same_line(history[latest], line)) {
            return;
        }
    }

    (void)copy_line(history[history_next], line);

    history_next =
        (history_next + 1U) % HISTORY_SIZE;

    if (history_count < HISTORY_SIZE) {
        history_count++;
    }
}

void shell_color(uint32_t color)
{
    terminal_set_color(console, color);
}

static void prompt(void)
{
    terminal_prompt(
        console,
        apps_fs_cwd(),
        discard_line
    );

    for (unsigned int i = 0U; i < length; i++) {
        uart_putc(console, line[i]);
    }
}

static void redraw_line(void)
{
    printk("\r\033[2K");
    prompt();
}

static void history_move(bool older)
{
    if (older) {
        /* 履歴が空、または最古の履歴に到達している。 */
        if (history_offset == history_count) {
            return;
        }

        /*
         * 初めて履歴へ移動するとき、
         * 入力途中だった行を退避する。
         */
        if (history_offset == 0U) {
            (void)copy_line(saved_line, line);
        }

        history_offset++;
    } else {
        if (history_offset == 0U) {
            return;
        }

        history_offset--;
    }

    if (history_offset == 0U) {
        /* 最新の履歴からさらに下へ進むと入力途中の行へ戻る。 */
        length = copy_line(line, saved_line);
    } else {
        unsigned int index =
            (history_next + HISTORY_SIZE - history_offset)
            % HISTORY_SIZE;

        /*
         * 編集用のlineへコピーする。
         * 呼び出した行を編集しても、保存済みの履歴は変わらない。
         */
        length = copy_line(line, history[index]);
    }

    redraw_line();
}

static void cancel_line(
    const char *message,
    bool discard
)
{
    clear_input();

    skip_lf = false;
    discard_line = discard;

    printk("\r\033[2K%s\n", message);
    prompt();
}

static void accept_char(unsigned char c)
{
    /* CRLFは1回のEnterとして扱う。 */
    if (c == '\n' && skip_lf) {
        skip_lf = false;
        return;
    }

    skip_lf = false;

    /* Ctrl+C：入力行を取り消す。 */
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
            history_add();
            shell_execute(line);
        }

        clear_input();
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

    /* Ctrl+U：入力行を消し、履歴の参照も終了する。 */
    if (c == 21U) {
        clear_input();
        redraw_line();
        return;
    }

    /* Ctrl+P / Ctrl+Nも、上下矢印と同じ操作。 */
    if (c == 16U || c == 14U) {
        escape_state = 0U;
        history_move(c == 16U);
        return;
    }

    if (c == 27U) {
        escape_state = 1U;
        return;
    }

    /*
     * 矢印キーの制御シーケンス：
     *   ESC [ A / ESC O A ：上
     *   ESC [ B / ESC O B ：下
     *
     * 受信が複数回に分かれてもescape_stateで状態を保持する。
     */
    if (escape_state == 1U) {
        escape_state =
            (c == '[' || c == 'O') ? 2U : 0U;
        return;
    }

    if (escape_state == 2U) {
        if (c >= 0x40U && c <= 0x7EU) {
            escape_state = 0U;

            if (c == 'A') {
                history_move(true);
            } else if (c == 'B') {
                history_move(false);
            }
        } else if (c < 0x20U || c > 0x3FU) {
            escape_state = 0U;
        }

        return;
    }

    /* Backspace / DEL */
    if (c == '\b' || c == 127U) {
        if (length != 0U) {
            length--;
            line[length] = '\0';

            printk("\b \b");
        }

        return;
    }

    /* タブは空白として扱う。 */
    if (c == '\t') {
        c = ' ';
    }

    /* ASCIIの表示可能文字を受け付ける。 */
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
    line[length] = '\0';

    uart_putc(console, (char)c);
}

void shell_task(void *argument)
{
    (void)argument;

    console = uart_get_device(0U);

    clear_input();

    history_next = 0U;
    history_count = 0U;

    skip_lf = false;
    discard_line = false;

    printk("\nKITOS shell. Type help.\n");
    prompt();

    for (;;) {
        bool active = false;

        /* UARTと、アプリ終了時に残った入力から文字を取得する。 */
        for (unsigned int i = 0U; i < 32U; i++) {
            char c;

            int result =
                apps_console_try_getc(console, &c);

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

        /* 他タスクのメッセージを表示する。 */
        for (unsigned int i = 0U; i < 4U; i++) {
            struct task_message message;

            if (!task_read_log(&message)) {
                break;
            }

            active = true;

            /*
             * 現在の入力行を消し、
             * メッセージ表示後に再描画する。
             * 履歴の参照位置と編集中の内容は維持する。
             */
            printk("\r\033[2K");

            terminal_task_message(console, &message);

            prompt();
        }

        /*
         * 前回のsleep実装で追加した割込み待機関数。
         * 他タスクへの切替はタイマー割込みで行われる。
         */
        if (!active) {
            arch_wait_for_irq();
        }
    }
}