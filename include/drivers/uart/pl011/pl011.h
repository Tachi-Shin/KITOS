// include/drivers/uart/pl011/pl011.h

#ifndef PL011_H
#define PL011_H

struct uart_device;

struct uart_device *pl011_get_device(void);

#endif