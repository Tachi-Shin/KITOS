#ifndef KITOS_DRIVERS_GPIO_GPIO_H
#define KITOS_DRIVERS_GPIO_GPIO_H

enum gpio_level {
    LOW = 0,
    HIGH = 1
};

enum gpio_mode {
    INPUT = 0,
    OUTPUT = 1
};

enum gpio_result {
    GPIO_OK = 0,
    GPIO_EINVAL = -1,
    GPIO_ENODEV = -2,
    GPIO_EMODE = -3
};

/* pin_numはBCM GPIO番号。コネクタの物理端子番号ではない。 */

/* INPUT/OUTPUTの現在の端子レベルを返す。失敗時は負数。 */
int gpio_read(unsigned int pin_num);

/* OUTPUT設定済みのピンへ出力する。成功時GPIO_OK、失敗時は負数。 */
int gpio_write(unsigned int pin_num, enum gpio_level level);

/*
 * 入出力方向を設定する。代替機能のピンもGPIOへ切り替える。
 * OUTPUTへの切り替え時はLOWで開始する。
 * 既にOUTPUTの場合は現在の出力を維持する。
 * プルアップ/プルダウン設定は変更しない。
 */
int gpio_pinMode(unsigned int pin_num, enum gpio_mode mode);

#endif