#ifndef EXCEPTION_H
#define EXCEPTION_H

#include <dos/type.h>

// ---------------------------------------------------------------------------
// IRQ番号定義
// ---------------------------------------------------------------------------
// 30: Generic Timer (EL1物理タイマ)
// 153: UART PL011 (RPi4)
// 1023: spurious interrupt (無視する)
#define IRQ_TIMER       30
#define IRQ_UART_PL011  153
#define IRQ_SPURIOUS    1023
#define IRQ_EMMC2       158

// ESR_EL1 EC フィールド (bit[31:26])
#define ESR_EC_SHIFT        26
#define ESR_EC_MASK         0x3FUL
#define ESR_EC_DATA_ABORT   0x25    // Data Abort (EL1)
#define ESR_EC_INST_ABORT   0x21    // Instruction Abort (EL1)
#define ESR_EC_SVC64        0x15    // SVC命令 (AArch64)
#define ESR_EC_UNKNOWN      0x00    // 未知

extern char vector_table[];

void SetVectorTable(void);
void irq_dispatch(void);
void sync_dispatch(uint64_t esr, uint64_t far);

void DisableInt(void);
void EnableInt(void);

#endif