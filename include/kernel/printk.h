// include/kernel/printk.h

#ifndef PRINTK_H
#define PRINTK_H

struct uart_device;

void printk_set_console(struct uart_device *dev);
void printk(const char *format_string, ...);

#endif