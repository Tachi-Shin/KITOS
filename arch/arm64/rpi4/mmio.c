// arch/arm64/rpi4/mmio.c

#include <arch/arm64/rpi4/mmio.h>

uint8_t mmio_read8(uintptr_t address)
{
    return *(volatile uint8_t *)address;
}

uint16_t mmio_read16(uintptr_t address)
{
    return *(volatile uint16_t *)address;
}

uint32_t mmio_read32(uintptr_t address)
{
    return *(volatile uint32_t *)address;
}

uint64_t mmio_read64(uintptr_t address)
{
    return *(volatile uint64_t *)address;
}

void mmio_write8(uintptr_t address, uint8_t value)
{
    *(volatile uint8_t *)address = value;
}

void mmio_write16(uintptr_t address, uint16_t value)
{
    *(volatile uint16_t *)address = value;
}

void mmio_write32(uintptr_t address, uint32_t value)
{
    *(volatile uint32_t *)address = value;
}

void mmio_write64(uintptr_t address, uint64_t value)
{
    *(volatile uint64_t *)address = value;
}

void mmio_dmb(void)
{
    /*
     * この命令より前のメモリアクセスが完了してから、
     * 後続のメモリアクセスを開始させる。
     */
    __asm__ __volatile__(
        "dmb sy"
        :
        :
        : "memory"
    );
}

void mmio_dsb(void)
{
    /*
     * この命令より前のメモリアクセスが
     * 完全に終了するまで待つ。
     */
    __asm__ __volatile__(
        "dsb sy"
        :
        :
        : "memory"
    );
}

void mmio_isb(void)
{
    /*
     * 命令パイプラインを同期する。
     */
    __asm__ __volatile__(
        "isb"
        :
        :
        : "memory"
    );
}