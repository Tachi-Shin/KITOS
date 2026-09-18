#include <dos/type.h>

#include <arch/arm64/kernel/irq.h>

#include <drivers/uart/uart.h>
#include <drivers/uart/mini_uart/mini_uart.h>

#include <kernel/task.h>

static struct uart_device *uart_devices[UART_DEVICE_COUNT];

void uart_register_devices(void)
{
    uart_devices[0] = mini_uart_get_device();
}

struct uart_device *uart_get_device(unsigned int index)
{
    return index < UART_DEVICE_COUNT ? uart_devices[index] : NULL;
}

/* 起動時、受信割込みを有効にする前に呼ぶ */
int uart_init(struct uart_device *dev)
{
    if (dev == NULL || dev->ops == NULL || dev->ops->init == NULL) {
        return -1;
    }

    dev->rx_head = 0U;
    dev->rx_tail = 0U;
    dev->rx_irq_enabled = false;
    dev->rx_lost = false;
    dev->rx_stats = (struct uart_rx_stats){0};

    return dev->ops->init(dev);
}

void uart_putc(struct uart_device *dev, char c)
{
    if (dev != NULL && dev->ops != NULL && dev->ops->putc != NULL) {
        dev->ops->putc(dev, c);
    }
}

/*
 * 従来の、文字を待つAPI。
 * 割込み方式では、IRQが有効な通常のタスクから呼ぶこと。
 * 割込みハンドラ内からは呼ばない。
 *
 * 新しい受信処理には、状態を区別できるuart_try_getc()を使う。
 */
char uart_getc(struct uart_device *dev)
{
    char c;
    int result;

    if (dev == NULL || dev->ops == NULL || dev->ops->getc == NULL) {
        return '\0';
    }

    if (!dev->rx_irq_enabled) {
        return dev->ops->getc(dev);
    }

    for (;;) {
        result = uart_try_getc(dev, &c);

        if (result == 1) {
            return c;
        }

        if (result < 0) {
            return '\0';
        }

        task_yield();
    }
}

int uart_enable_rx_irq(struct uart_device *dev)
{
    uint64_t flags;

    if (dev == NULL || dev->ops == NULL ||
        dev->ops->enable_rx_irq == NULL ||
        dev->ops->handle_irq == NULL) {
        return -1;
    }

    flags = arch_irq_save();

    dev->rx_irq_enabled = true;
    dev->ops->enable_rx_irq(dev);

    arch_irq_restore(flags);

    return 0;
}

/* irq_dispatch()から呼ぶ */
bool uart_handle_irq(unsigned int irq)
{
    for (unsigned int i = 0U; i < UART_DEVICE_COUNT; i++) {
        struct uart_device *dev = uart_devices[i];

        if (dev != NULL &&
            dev->irq_intid == irq &&
            dev->rx_irq_enabled) {
            dev->rx_stats.interrupts++;
            dev->ops->handle_irq(dev);
            return true;
        }
    }

    return false;
}

/* IRQ禁止状態で呼ぶ */
void uart_rx_push(struct uart_device *dev, char c)
{
    unsigned int next = (dev->rx_head + 1U) % UART_RX_SIZE;

    dev->rx_stats.bytes++;

    if (next == dev->rx_tail) {
        dev->rx_stats.dropped++;
        dev->rx_lost = true;
        return;
    }

    dev->rx[dev->rx_head] = c;
    dev->rx_head = next;
}

/* IRQ禁止状態で呼ぶ */
void uart_rx_overrun(struct uart_device *dev)
{
    dev->rx_stats.overruns++;
    dev->rx_lost = true;
}

int uart_try_getc(struct uart_device *dev, char *out)
{
    uint64_t flags;
    int result = 0;

    if (dev == NULL || out == NULL || !dev->rx_irq_enabled) {
        return -1;
    }

    flags = arch_irq_save();

    if (dev->rx_lost) {
        /*
         * 欠落した文字列をそのまま使わないように、
         * バッファを破棄して呼出し側に通知する。
         */
        dev->rx_tail = dev->rx_head;
        dev->rx_lost = false;
        result = UART_RX_LOST;
    } else if (dev->rx_tail != dev->rx_head) {
        *out = dev->rx[dev->rx_tail];
        dev->rx_tail = (dev->rx_tail + 1U) % UART_RX_SIZE;
        result = 1;
    }

    arch_irq_restore(flags);

    return result;
}

void uart_get_rx_stats(
    struct uart_device *dev,
    struct uart_rx_stats *out
)
{
    uint64_t flags;

    if (dev == NULL || out == NULL) {
        return;
    }

    flags = arch_irq_save();
    *out = dev->rx_stats;
    arch_irq_restore(flags);
}