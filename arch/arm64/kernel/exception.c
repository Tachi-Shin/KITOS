// arch/arm64/kernel/exception.c
#include <arch/arm64/kernel/exception.h>
#include <arch/arm64/armv8util.h>
#include <arch/arm64/rpi4/gicv2def.h>
#include <arch/arm64/rpi4/gicv2.h>
#include <arch/arm64/kernel/timer.h>
#include <arch/arm64/rpi4/gicctl.h>

#include <drivers/uart/uart.h>

#include <kernel/task.h>
#include <kernel/printk.h>

void SetVectorTable(void) {
    WriteSysReg(VBAR_EL1, (unsigned long)vector_table);
    __asm__ volatile("isb" ::: "memory");
    printk("[exception] vector table registered at 0x%lx\n", (uintptr_t)vector_table);
}

void irq_dispatch(void)
{
    bool need_resched = false;

    uint32_t iar = AckInterrupt();
    uint32_t irq = GetIRQId(iar);

    if (irq >= 1020U) {
        return;
    }

    switch (irq) {
    case IRQ_TIMER:
        need_resched = arch_timer_handler();
        break;

    case IRQ_EMMC2:
        // emmc2_irq_handler();
        break;

    default:
        if (!uart_handle_irq(irq)) {
            DisableInterrupt(irq);
        }
        break;
    }

    __asm__ volatile("dsb sy" ::: "memory");
    EndOfInterrupt(iar);
    __asm__ volatile("dsb sy" ::: "memory");

    if (need_resched) {
        task_yield();
    }
}

void sync_dispatch(uint64_t esr, uint64_t far) {
    /* uint32_t ec = (esr >> ESR_EC_SHIFT) & ESR_EC_MASK;

    switch (ec) {
    case ESR_EC_DATA_ABORT:
        // データアクセス例外 → ページフォルトハンドラへ
        page_fault_handler(far, esr);
        printk("exception happend!!\n");
        break;

    case ESR_EC_INST_ABORT:
        // 命令フェッチ例外 → ページフォルトハンドラへ
        page_fault_handler(far, esr);
        printk("exception happend!!\n");
        break;

    case ESR_EC_SVC64:
        // システムコール → 将来実装
        printk("[sync] SVC: not implemented\n");
        break;

    default:
        printk("[sync] unhandled exception EC=0x%x ESR=0x%lx FAR=0x%lx\n",
               ec, esr, far);
        while (1);
    }
    */
}