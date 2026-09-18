// kernel/printk/printk.c

#include <kernel/printk.h>

#include <dos/type.h>
#include <drivers/uart/uart.h>

#include <stdarg.h>

#define PRINTK_BUFFER_SIZE 4096U

static char printk_buffer[PRINTK_BUFFER_SIZE];
static struct uart_device *printk_console = NULL;

struct format_buffer {
    char *data;
    size_t size;
    size_t position;
};

void printk_set_console(struct uart_device *dev)
{
    printk_console = dev;
}

static void buffer_putc(struct format_buffer *buffer, char c)
{
    /*
     * 最後の1バイトは'\0'のために残す。
     */
    if (buffer->position + 1U >= buffer->size) {
        return;
    }

    buffer->data[buffer->position] = c;
    buffer->position++;
}

static void buffer_puts(struct format_buffer *buffer, const char *str)
{
    if (str == NULL) {
        str = "(null)";
    }

    while (*str != '\0') {
        buffer_putc(buffer, *str);
        str++;
    }
}

static void buffer_put_unsigned(
    struct format_buffer *buffer,
    unsigned long long value,
    unsigned int base,
    bool uppercase)
{
    char temporary[32];
    unsigned int length = 0;
    const char *digits;

    if (uppercase) {
        digits = "0123456789ABCDEF";
    } else {
        digits = "0123456789abcdef";
    }

    if (value == 0ULL) {
        buffer_putc(buffer, '0');
        return;
    }

    while (value != 0ULL) {
        temporary[length] = digits[value % base];
        length++;
        value /= base;
    }

    while (length > 0U) {
        length--;
        buffer_putc(buffer, temporary[length]);
    }
}

static void buffer_put_signed(
    struct format_buffer *buffer,
    long long value)
{
    unsigned long long magnitude;

    if (value < 0) {
        buffer_putc(buffer, '-');

        /*
         * LLONG_MINでも符号付きオーバーフローを起こさない変換。
         */
        magnitude = 0ULL - (unsigned long long)value;
    } else {
        magnitude = (unsigned long long)value;
    }

    buffer_put_unsigned(buffer, magnitude, 10U, false);
}

static void format(
    char *destination,
    size_t destination_size,
    const char *format_string,
    va_list arguments)
{
    struct format_buffer buffer = {
        .data = destination,
        .size = destination_size,
        .position = 0U,
    };

    const char *current = format_string;

    if (destination == NULL || destination_size == 0U) {
        return;
    }

    if (format_string == NULL) {
        destination[0] = '\0';
        return;
    }

    while (*current != '\0') {
        bool is_long = false;
        bool is_long_long = false;

        if (*current != '%') {
            buffer_putc(&buffer, *current);
            current++;
            continue;
        }

        /*
         * '%'を読み飛ばす。
         */
        current++;

        /*
         * 文字列末尾が単独の'%'だった場合。
         */
        if (*current == '\0') {
            buffer_putc(&buffer, '%');
            break;
        }

        /*
         * lまたはll修飾子を判定する。
         */
        if (*current == 'l') {
            current++;

            if (*current == 'l') {
                is_long_long = true;
                current++;
            } else {
                is_long = true;
            }
        }

        switch (*current) {
        case 'd':
        case 'i': {
            long long value;

            if (is_long_long) {
                value = va_arg(arguments, long long);
            } else if (is_long) {
                value = (long long)va_arg(arguments, long);
            } else {
                value = (long long)va_arg(arguments, int);
            }

            buffer_put_signed(&buffer, value);
            current++;
            break;
        }

        case 'u': {
            unsigned long long value;

            if (is_long_long) {
                value = va_arg(arguments, unsigned long long);
            } else if (is_long) {
                value = (unsigned long long)
                    va_arg(arguments, unsigned long);
            } else {
                value = (unsigned long long)
                    va_arg(arguments, unsigned int);
            }

            buffer_put_unsigned(&buffer, value, 10U, false);
            current++;
            break;
        }

        case 'x':
        case 'X': {
            unsigned long long value;
            bool uppercase = (*current == 'X');

            if (is_long_long) {
                value = va_arg(arguments, unsigned long long);
            } else if (is_long) {
                value = (unsigned long long)
                    va_arg(arguments, unsigned long);
            } else {
                value = (unsigned long long)
                    va_arg(arguments, unsigned int);
            }

            buffer_put_unsigned(&buffer, value, 16U, uppercase);
            current++;
            break;
        }

        case 'p': {
            void *pointer = va_arg(arguments, void *);
            uintptr_t address = (uintptr_t)pointer;

            buffer_puts(&buffer, "0x");
            buffer_put_unsigned(
                &buffer,
                (unsigned long long)address,
                16U,
                false);

            current++;
            break;
        }

        case 's': {
            const char *string = va_arg(arguments, const char *);

            buffer_puts(&buffer, string);
            current++;
            break;
        }

        case 'c': {
            char character = (char)va_arg(arguments, int);

            buffer_putc(&buffer, character);
            current++;
            break;
        }

        case '%':
            buffer_putc(&buffer, '%');
            current++;
            break;

        default:
            /*
             * 未対応の指定子は、そのまま出力する。
             */
            buffer_putc(&buffer, '%');

            if (is_long_long) {
                buffer_putc(&buffer, 'l');
                buffer_putc(&buffer, 'l');
            } else if (is_long) {
                buffer_putc(&buffer, 'l');
            }

            if (*current != '\0') {
                buffer_putc(&buffer, *current);
                current++;
            }
            break;
        }
    }

    buffer.data[buffer.position] = '\0';
}

static void printk_write(const char *string)
{
    if (printk_console == NULL || string == NULL) {
        return;
    }

    while (*string != '\0') {
        /*
         * UART端末の改行コードをCR+LFへ変換する。
         */
        if (*string == '\n') {
            uart_putc(printk_console, '\r');
        }

        uart_putc(printk_console, *string);
        string++;
    }
}

void printk(const char *format_string, ...)
{
    va_list arguments;

    if (printk_console == NULL || format_string == NULL) {
        return;
    }

    va_start(arguments, format_string);

    format(
        printk_buffer,
        sizeof(printk_buffer),
        format_string,
        arguments);

    va_end(arguments);

    printk_write(printk_buffer);
}