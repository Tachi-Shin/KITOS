// include/usr/shell.h
#ifndef USR_SHELL_H
#define USR_SHELL_H

#include <dos/type.h>

void shell_task(void *argument);
void shell_execute(char *line);
void shell_color(uint32_t color);

void demo_task(void *argument);

#endif