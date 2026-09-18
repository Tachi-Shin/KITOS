// include/arch/arm64/rpi4/mmio.h

#ifndef MMIO_H
#define MMIO_H

#include <dos/type.h>

uint8_t  mmio_read8(uintptr_t address);
uint16_t mmio_read16(uintptr_t address);
uint32_t mmio_read32(uintptr_t address);
uint64_t mmio_read64(uintptr_t address);

void mmio_write8(uintptr_t address, uint8_t value);
void mmio_write16(uintptr_t address, uint16_t value);
void mmio_write32(uintptr_t address, uint32_t value);
void mmio_write64(uintptr_t address, uint64_t value);

void mmio_dmb(void);
void mmio_dsb(void);
void mmio_isb(void);

#endif