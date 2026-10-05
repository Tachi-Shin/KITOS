#include <drivers/gpio/bcm2711/bcm2711_gpio.h>

#if !defined(__aarch64__)
#error "This GPIO driver requires AArch64."
#endif

#define BCM2711_GPIO_MAX_PINS 58U
#define GPIO_GPFSEL0         0x00U
#define GPIO_GPSET0          0x1CU
#define GPIO_GPCLR0          0x28U
#define GPIO_GPLEV0          0x34U
#define GPIO_REGISTER_END    0xF3U

const struct gpio_device bcm2711_gpio_device = {
    .base = (uintptr_t)0xFE200000UL,
    .pin_count = 28U
};

/* BSSは起動処理でゼロクリアされること。base == 0は未初期化を表す。 */
static struct gpio_device active_gpio;

/* 単一コア内のタスク/IRQ/FIQ間の競合を防ぐ。SMP用ロックではない。 */
static uint64_t gpio_lock(void)
{
    uint64_t saved_daif;

    __asm__ volatile (
        "mrs %0, daif\n\t"
        "msr daifset, #3"
        : "=r" (saved_daif)
        :
        : "memory"
    );

    return saved_daif;
}

static void gpio_unlock(uint64_t saved_daif)
{
    __asm__ volatile (
        "msr daif, %0"
        :
        : "r" (saved_daif)
        : "memory"
    );
}

static void gpio_barrier(void)
{
    __asm__ volatile ("dsb sy" ::: "memory");
}

static uint32_t gpio_mmio_read(uintptr_t offset)
{
    volatile uint32_t *reg =
        (volatile uint32_t *)(active_gpio.base + offset);

    gpio_barrier();
    uint32_t value = *reg;
    gpio_barrier();

    return value;
}

static void gpio_mmio_write(uintptr_t offset, uint32_t value)
{
    volatile uint32_t *reg =
        (volatile uint32_t *)(active_gpio.base + offset);

    gpio_barrier();
    *reg = value;
    gpio_barrier();
}

/* 以下の内部関数はgpio_lock()取得中に呼ぶ。 */
static int gpio_check_pin(unsigned int pin_num)
{
    if (active_gpio.base == 0U) {
        return GPIO_ENODEV;
    }

    if (pin_num >= active_gpio.pin_count) {
        return GPIO_EINVAL;
    }

    return GPIO_OK;
}

static uint32_t gpio_function(unsigned int pin_num)
{
    uintptr_t offset = GPIO_GPFSEL0 + (pin_num / 10U) * 4U;
    unsigned int shift = (pin_num % 10U) * 3U;

    return (gpio_mmio_read(offset) >> shift) & 7U;
}

static int bcm2711_init(const struct gpio_device *gpio_device)
{
    if (gpio_device == NULL ||
        gpio_device->base == 0U ||
        (gpio_device->base & (uintptr_t)3U) != 0U ||
        gpio_device->base > (~(uintptr_t)0) - GPIO_REGISTER_END ||
        gpio_device->pin_count == 0U ||
        gpio_device->pin_count > BCM2711_GPIO_MAX_PINS) {
        return GPIO_EINVAL;
    }

    uint64_t saved_daif = gpio_lock();

    active_gpio.base = gpio_device->base;
    active_gpio.pin_count = gpio_device->pin_count;

    gpio_unlock(saved_daif);
    return GPIO_OK;
}

static int bcm2711_read(unsigned int pin_num)
{
    uint64_t saved_daif = gpio_lock();
    int result = gpio_check_pin(pin_num);

    if (result == GPIO_OK) {
        uint32_t function = gpio_function(pin_num);

        if (function != (uint32_t)INPUT &&
            function != (uint32_t)OUTPUT) {
            result = GPIO_EMODE;
        } else {
            uintptr_t offset = GPIO_GPLEV0 + (pin_num / 32U) * 4U;
            uint32_t mask = 1U << (pin_num % 32U);

            result = (gpio_mmio_read(offset) & mask) != 0U
                ? HIGH : LOW;
        }
    }

    gpio_unlock(saved_daif);
    return result;
}

static int bcm2711_write(unsigned int pin_num, enum gpio_level level)
{
    if (level != LOW && level != HIGH) {
        return GPIO_EINVAL;
    }

    uint64_t saved_daif = gpio_lock();
    int result = gpio_check_pin(pin_num);

    if (result == GPIO_OK) {
        if (gpio_function(pin_num) != (uint32_t)OUTPUT) {
            result = GPIO_EMODE;
        } else {
            uintptr_t offset = level == HIGH ? GPIO_GPSET0 : GPIO_GPCLR0;
            offset += (pin_num / 32U) * 4U;

            /* SET/CLRは書き込み専用。対象の1ビットだけを直接書く。 */
            gpio_mmio_write(offset, 1U << (pin_num % 32U));
        }
    }

    gpio_unlock(saved_daif);
    return result;
}

static int bcm2711_pin_mode(unsigned int pin_num, enum gpio_mode mode)
{
    if (mode != INPUT && mode != OUTPUT) {
        return GPIO_EINVAL;
    }

    uint64_t saved_daif = gpio_lock();
    int result = gpio_check_pin(pin_num);

    if (result == GPIO_OK) {
        uintptr_t offset = GPIO_GPFSEL0 + (pin_num / 10U) * 4U;
        unsigned int shift = (pin_num % 10U) * 3U;
        uint32_t value = gpio_mmio_read(offset);
        uint32_t function = (value >> shift) & 7U;

        if (function != (uint32_t)mode) {
            /* 代替機能からの切り替えでは、一度入力状態にする。 */
            if (function > (uint32_t)OUTPUT) {
                value &= ~(7U << shift);
                gpio_mmio_write(offset, value);
            }

            if (mode == OUTPUT) {
                /* 出力へ切り替える前にラッチをLOWにする。 */
                uintptr_t clear_offset = GPIO_GPCLR0 + (pin_num / 32U) * 4U;
                gpio_mmio_write(
                    clear_offset,
                    1U << (pin_num % 32U)
                );
            }

            value &= ~(7U << shift);
            value |= (uint32_t)mode << shift;
            gpio_mmio_write(offset, value);
        }
    }

    gpio_unlock(saved_daif);
    return result;
}

const struct gpio_backend_ops bcm2711_gpio_ops = {
    .init = bcm2711_init,
    .read = bcm2711_read,
    .write = bcm2711_write,
    .pin_mode = bcm2711_pin_mode
};
