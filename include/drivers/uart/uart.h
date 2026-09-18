#ifndef UART_H
#define UART_H

#include <dos/type.h>

#define UART_DEVICE_COUNT 1U
#define UART_RX_SIZE      256U

/* 受信データの欠落を検出した */
#define UART_RX_LOST      (-2)

struct uart_device;

struct uart_ops {
    int  (*init)(struct uart_device *dev);
    void (*putc)(struct uart_device *dev, char c);
    char (*getc)(struct uart_device *dev);

    void (*enable_rx_irq)(struct uart_device *dev);
    void (*handle_irq)(struct uart_device *dev);
};

struct uart_rx_stats {
    uint64_t interrupts;
    uint64_t bytes;
    uint64_t dropped;
    uint64_t overruns;
};

struct uart_device {
    const char *name;
    uintptr_t base_address;
    unsigned int irq_intid;
    const struct uart_ops *ops;

    char rx[UART_RX_SIZE];
    unsigned int rx_head;
    unsigned int rx_tail;

    bool rx_irq_enabled;
    bool rx_lost;

    struct uart_rx_stats rx_stats;
};

void uart_register_devices(void);
struct uart_device *uart_get_device(unsigned int index);

int uart_init(struct uart_device *dev);
void uart_putc(struct uart_device *dev, char c);
char uart_getc(struct uart_device *dev);

int uart_enable_rx_irq(struct uart_device *dev);
bool uart_handle_irq(unsigned int irq);

/*
 *  1 : 1文字取得
 *  0 : 受信バッファが空
 * -1 : 引数または初期化状態が不正
 * UART_RX_LOST : 受信データの欠落を検出
 */
int uart_try_getc(struct uart_device *dev, char *out);

void uart_get_rx_stats(
    struct uart_device *dev,
    struct uart_rx_stats *out
);

/* ドライバの割込み処理から、IRQ禁止状態で呼ぶ */
void uart_rx_push(struct uart_device *dev, char c);
void uart_rx_overrun(struct uart_device *dev);

#endif