#include <drivers/gpio/gpio.h>
#include <drivers/gpio/bcm2711/bcm2711_gpio.h>

/* KITOSのRaspberry Pi 4B構成ではBCM2711ドライバを使用する。 */
int gpio_init(const struct gpio_device *gpio_device)
{
    return bcm2711_gpio_ops.init(gpio_device);
}

int gpio_read(unsigned int pin_num)
{
    return bcm2711_gpio_ops.read(pin_num);
}

int gpio_write(unsigned int pin_num, enum gpio_level level)
{
    return bcm2711_gpio_ops.write(pin_num, level);
}

int gpio_pinMode(unsigned int pin_num, enum gpio_mode mode)
{
    return bcm2711_gpio_ops.pin_mode(pin_num, mode);
}