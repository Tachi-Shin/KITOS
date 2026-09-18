// drivers/uart/mini_uart.c

#include <dos/type.h>

#include <arch/arm64/rpi4/mmio.h>

#include <drivers/uart/uart.h>
#include <drivers/uart/mini_uart/mini_uart.h>

#define GPIO_BASE          0xFE200000UL
#define GPIO_GPFSEL1       0x04U
#define GPIO_PUP_PDN0      0xE4U

#define AUX_BASE           0xFE215000UL
#define AUX_ENABLES        (AUX_BASE + 0x04U)

#define AUX_MU_IO_REG      (AUX_BASE + 0x40U)
#define AUX_MU_IER_REG     (AUX_BASE + 0x44U)
#define AUX_MU_IIR_REG     (AUX_BASE + 0x48U)
#define AUX_MU_LCR_REG     (AUX_BASE + 0x4CU)
#define AUX_MU_MCR_REG     (AUX_BASE + 0x50U)
#define AUX_MU_LSR_REG     (AUX_BASE + 0x54U)
#define AUX_MU_CNTL_REG    (AUX_BASE + 0x60U)
#define AUX_MU_BAUD_REG    (AUX_BASE + 0x68U)
#define AUX_MU_STAT_REG     (AUX_BASE + 0x64U)

#define AUX_ENABLE_MU      (1U << 0)

#define AUX_MU_LSR_DATA_READY (1U << 0)
#define AUX_MU_LSR_OVERRUN    (1U << 1)

#define AUX_MU_STAT_TX_SPACE  (1U << 1)

#define AUX_MU_IER_RX_IRQ     (1U << 0)
#define AUX_MU_IIR_NO_IRQ     (1U << 0)

/* 1回の割込み処理で読む文字数の上限 */
#define MINI_UART_RX_BUDGET   64U

#define AUX_MU_CNTL_RX_ENABLE (1U << 0)
#define AUX_MU_CNTL_TX_ENABLE (1U << 1)

static int mini_uart_init(struct uart_device *dev);
static void mini_uart_putc(struct uart_device *dev, char c);
static char mini_uart_getc(struct uart_device *dev);
static void mini_uart_enable_rx_irq(struct uart_device *dev);
static void mini_uart_handle_irq(struct uart_device *dev);

static const struct uart_ops mini_uart_ops = {
    .init = mini_uart_init,
    .putc = mini_uart_putc,
    .getc = mini_uart_getc,
    .enable_rx_irq = mini_uart_enable_rx_irq,
    .handle_irq = mini_uart_handle_irq,
};

static struct uart_device mini_uart_device = {
    .name         = "BCM2711 Mini UART",
    .base_address = AUX_MU_IO_REG,
    .irq_intid    = 125U,
    .ops          = &mini_uart_ops,
};

struct uart_device *mini_uart_get_device(void)
{
    return &mini_uart_device;
}

static void mini_uart_configure_gpio(void)
{
    uint32_t value;

    /*
     * GPIO14とGPIO15をALT5に設定する。
     *
     * GPIO14:
     *   bits 12-14
     *
     * GPIO15:
     *   bits 15-17
     *
     * ALT5 = 0b010
     */
    value = mmio_read32(GPIO_BASE + GPIO_GPFSEL1);

    value &= ~((7U << 12) | (7U << 15));
    value |=  ((2U << 12) | (2U << 15));

    mmio_write32(GPIO_BASE + GPIO_GPFSEL1, value);

    /*
     * GPIO14とGPIO15のpull-up/downを無効化する。
     */
    value = mmio_read32(GPIO_BASE + GPIO_PUP_PDN0);

    value &= ~((3U << 28) | (3U << 30));

    mmio_write32(GPIO_BASE + GPIO_PUP_PDN0, value);

    mmio_dsb();
}

static int mini_uart_init(struct uart_device *dev)
{
    uint32_t value;

    if (dev == NULL) {
        return -1;
    }

    /*
     * AUX Mini UARTを有効化する。
     */
    value = mmio_read32(AUX_ENABLES);
    value |= AUX_ENABLE_MU;
    mmio_write32(AUX_ENABLES, value);

    /*
     * 初期化中は送受信を無効化する。
     */
    mmio_write32(AUX_MU_CNTL_REG, 0U);

    /*
     * 割り込みを無効化する。
     */
    mmio_write32(AUX_MU_IER_REG, 0U);

    /*
     * 8ビットモード。
     */
    mmio_write32(AUX_MU_LCR_REG, 3U);

    /*
     * RTSを使用しない。
     */
    mmio_write32(AUX_MU_MCR_REG, 0U);

    /*
     * FIFOをクリアする。
     */
    mmio_write32(AUX_MU_IIR_REG, 0xC6U);

    /*
     * GPIO14/15をMini UARTへ接続する。
     */
    mini_uart_configure_gpio();

    /*
     * core_freq = 250 MHz、baud = 115200の場合:
     *
     * baud_reg =
     *   core_freq / (8 × baud) - 1
     *
     *   250,000,000 / (8 × 115,200) - 1
     *   ≒ 270
     */
    mmio_write32(AUX_MU_BAUD_REG, 270U);

    /*
     * 送受信を有効化する。
     */
    mmio_write32(
        AUX_MU_CNTL_REG,
        AUX_MU_CNTL_RX_ENABLE |
        AUX_MU_CNTL_TX_ENABLE
    );

    mmio_dsb();

    return 0;
}

static void mini_uart_putc(struct uart_device *dev, char c)
{
    if (dev == NULL) {
        return;
    }

    while ((mmio_read32(AUX_MU_STAT_REG) &
            AUX_MU_STAT_TX_SPACE) == 0U) {
    }

    mmio_write32(
        dev->base_address,
        (uint32_t)(uint8_t)c
    );
}

static char mini_uart_getc(struct uart_device *dev)
{
    uint32_t value;

    if (dev == NULL) {
        return '\0';
    }

    while ((mmio_read32(AUX_MU_LSR_REG) &
            AUX_MU_LSR_DATA_READY) == 0U) {
    }

    value = mmio_read32(dev->base_address);

    return (char)(value & 0xFFU);
}

static void mini_uart_enable_rx_irq(struct uart_device *dev)
{
    (void)dev;

    /* 受信割込みを有効化。送信割込みは無効のまま */
    mmio_write32(AUX_MU_IER_REG, AUX_MU_IER_RX_IRQ);
    mmio_dsb();
}

static void mini_uart_handle_irq(struct uart_device *dev)
{
    if ((mmio_read32(AUX_MU_IIR_REG) & AUX_MU_IIR_NO_IRQ) != 0U) {
        return;
    }

    for (unsigned int i = 0U; i < MINI_UART_RX_BUDGET; i++) {
        uint32_t status = mmio_read32(AUX_MU_LSR_REG);

        if ((status & AUX_MU_LSR_OVERRUN) != 0U) {
            uart_rx_overrun(dev);
        }

        if ((status & AUX_MU_LSR_DATA_READY) == 0U) {
            break;
        }

        uart_rx_push(
            dev,
            (char)(mmio_read32(dev->base_address) & 0xFFU)
        );
    }

    mmio_dsb();
}