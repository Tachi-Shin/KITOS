// usr/bin/gpio.c
#include <usr/apps.h>
#include <drivers/gpio/gpio.h>

struct gpio_token {
    const char *text;
    size_t length;
};

static int is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static int next_token(const char **cursor, struct gpio_token *token)
{
    const char *p = *cursor;

    while (is_space(*p)) {
        ++p;
    }

    token->text = p;
    token->length = 0U;

    while (*p != '\0' && !is_space(*p)) {
        ++p;
        ++token->length;
    }

    *cursor = p;
    return token->length != 0U;
}

/* wordは小文字で指定する。入力は大文字・小文字を区別しない。 */
static int token_is(const struct gpio_token *token, const char *word)
{
    for (size_t i = 0U; i < token->length; ++i) {
        char c = token->text[i];

        if (word[i] == '\0') {
            return 0;
        }

        if (c >= 'A' && c <= 'Z') {
            c = (char)(c - 'A' + 'a');
        }

        if (c != word[i]) {
            return 0;
        }
    }

    return word[token->length] == '\0';
}

static int parse_pin(const struct gpio_token *token, unsigned int *pin)
{
    unsigned int value = 0U;

    for (size_t i = 0U; i < token->length; ++i) {
        char c = token->text[i];

        if (c < '0' || c > '9') {
            return 0;
        }

        unsigned int digit = (unsigned int)(c - '0');

        if (value > (~0U - digit) / 10U) {
            return 0;
        }

        value = value * 10U + digit;
    }

    *pin = value;
    return token->length != 0U;
}

static void usage(struct app_io *io)
{
    app_puts(io,
        "Usage:\n"
        "  gpio <pin>\n"
        "  gpio <pin> in|out|i|o|input|output\n"
        "  gpio <pin> 0|1|low|high|l|h\n"
        "Pin numbers are BCM GPIO numbers. Set out before writing.\n"
    );
}

static void report(struct app_io *io, const char *operation, int result)
{
    app_puts(io, operation);
    app_puts(io, " result: ");

    switch (result) {
    case GPIO_OK:
        app_puts(io, "ok\n");
        break;
    case GPIO_EINVAL:
        app_puts(io, "error (invalid pin or value)\n");
        break;
    case GPIO_ENODEV:
        app_puts(io, "error (GPIO is not initialized)\n");
        break;
    case GPIO_EMODE:
        app_puts(io, "error (pin mode mismatch)\n");
        break;
    default:
        app_puts(io, "error (");
        app_number(io, result);
        app_puts(io, ")\n");
        break;
    }
}

static void execute(struct gpio_context *ctx)
{
    struct app_io *io = ctx->io;
    const char *p = ctx->command;
    struct gpio_token pin_token, operation, extra;
    unsigned int pin;

    if (!next_token(&p, &pin_token) || !parse_pin(&pin_token, &pin)) {
        usage(io);
        return;
    }

    if (!next_token(&p, &operation)) {
        int result = gpio_read(pin);

        if (result < 0) {
            report(io, "input", result);
        } else {
            app_puts(io, result == HIGH
                ? "input result: true\n"
                : "input result: false\n");
        }

        return;
    }

    /* 余計な引数がある場合は、GPIOへ触れる前に拒否する。 */
    if (next_token(&p, &extra)) {
        usage(io);
        return;
    }

    if (token_is(&operation, "in") ||
        token_is(&operation, "i") ||
        token_is(&operation, "input")) {
        report(io, "mode", gpio_pinMode(pin, INPUT));
    } else if (token_is(&operation, "out") ||
               token_is(&operation, "o") ||
               token_is(&operation, "output")) {
        report(io, "mode", gpio_pinMode(pin, OUTPUT));
    } else if (token_is(&operation, "1") ||
               token_is(&operation, "h") ||
               token_is(&operation, "high")) {
        report(io, "output", gpio_write(pin, HIGH));
    } else if (token_is(&operation, "0") ||
               token_is(&operation, "l") ||
               token_is(&operation, "low")) {
        report(io, "output", gpio_write(pin, LOW));
    } else {
        usage(io);
    }
}

void gpio_task(void *argument)
{
    struct gpio_context *ctx = argument;

    if (ctx == NULL || ctx->io == NULL) {
        return;
    }

    struct app_io *io = ctx->io;

    execute(ctx);

    /* 成功・入力エラー・ドライバエラーのいずれでもシェルへ終了通知する。 */
    if (io->finished != NULL) {
        io->finished(io->user);
    }
}
