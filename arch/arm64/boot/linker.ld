/* arch/arm64/boot/linker.ld */

OUTPUT_ARCH(aarch64)
ENTRY(_start)

SECTIONS
{
    /*
     * Raspberry Pi firmware / QEMUがkernel8.imgを
     * 0x80000へロードし、先頭から実行する。
     */
    . = 0x00080000;

    __kernel_start = .;

    /*
     * kernel8.imgの先頭には必ず_startを置く。
     */
    .reset : {
        KEEP(*(.reset))
    }

    /*
     * VBAR_EL1の下位11ビットは0でなければならないため、
     * ベクタテーブルを0x800境界へ配置する。
     */
    . = ALIGN(0x800);

    .vector_table : {
        KEEP(*(.vector_table))
    }

    .text : {
        *(.text*)
    }

    .rodata : {
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    . = ALIGN(16);

    _bss_start = .;
    .bss : {
        *(.bss*)
        *(COMMON)
    }
    _bss_end = .;

    . = ALIGN(16);

    _stack_start = .;
    .stack : {
        . += 64K;
    }
    _stack_end = .;

    . = ALIGN(16);

    _heap_start = .;
    .heap : {
        . += 64K;
    }
    _heap_end = .;

    __kernel_end = .;

    . = 0x00800000;
    __user_area_start = .;
}