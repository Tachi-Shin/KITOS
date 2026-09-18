// include/kernel/timer.h
#ifndef KERNEL_TIME_TIMER_H
#define KERNEL_TIME_TIMER_H

#include <dos/type.h>

bool kernel_timer_tick(void);
uint64_t kernel_timer_get_ticks(void);

#endif /* KERNEL_TIME_TIMER_H */