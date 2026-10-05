#ifndef KITOS_DRIVERS_GPIO_BCM2711_GPIO_H
#define KITOS_DRIVERS_GPIO_BCM2711_GPIO_H

#include <dos/type.h>
#include <drivers/gpio/gpio.h>

/* カーネル/ボード初期化処理専用。単一コアAArch64のEL1で使用する。 */
struct gpio_device {
    /*
     * CPUからアクセスできるGPIOレジスタの先頭アドレス。
     * Pi 4Bの通常の物理アドレス配置では0xFE200000。
     * MMU有効時はDeviceメモリとしてマップした仮想アドレスを渡す。
     */
    uintptr_t base;

    /*
     * 有効なGPIO番号は0..pin_count-1。BCM2711では最大58。
     * GPIO0..27に限定する場合は28を指定する。
     */
    unsigned int pin_count;
};

extern const struct gpio_device bcm2711_gpio_device;

/*
 * 設定を内部へコピーする。デバイス構造体を保持し続ける必要はない。
 * ピンの方向、出力、プル設定は変更しない。
 * 成功時GPIO_OK、引数不正時GPIO_EINVAL。
 * スケジューラ/利用タスクの起動前に呼ぶ。
 */
int gpio_init(const struct gpio_device *gpio_device);

/* ドライバ内部の接続用。利用側からはgpio_*()のみを呼ぶ。 */
struct gpio_backend_ops {
    int (*init)(const struct gpio_device *gpio_device);
    int (*read)(unsigned int pin_num);
    int (*write)(unsigned int pin_num, enum gpio_level level);
    int (*pin_mode)(unsigned int pin_num, enum gpio_mode mode);
};

extern const struct gpio_backend_ops bcm2711_gpio_ops;

#endif
