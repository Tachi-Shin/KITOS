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
        EXCLUDE_FILE (
            build/arm64/usr/bin/demo_a.c.o
            build/arm64/usr/bin/demo_b.c.o
            build/arm64/usr/bin/demo_c.c.o
            build/arm64/usr/bin/demo_d.c.o
            build/arm64/usr/bin/basic.c.o
            build/arm64/usr/bin/editor.c.o
            build/arm64/usr/bin/gpio.c.o
            build/arm64/usr/shell/shell.c.o
        ) *(.text*)
    }

    .rodata : {
        EXCLUDE_FILE (
            build/arm64/usr/bin/demo_a.c.o
            build/arm64/usr/bin/demo_b.c.o
            build/arm64/usr/bin/demo_c.c.o
            build/arm64/usr/bin/demo_d.c.o
            build/arm64/usr/bin/basic.c.o
            build/arm64/usr/bin/editor.c.o
            build/arm64/usr/bin/gpio.c.o
            build/arm64/usr/shell/shell.c.o
        ) *(.rodata*)
    }

    .data : {
        EXCLUDE_FILE (
            build/arm64/usr/bin/demo_a.c.o
            build/arm64/usr/bin/demo_b.c.o
            build/arm64/usr/bin/demo_c.c.o
            build/arm64/usr/bin/demo_d.c.o
            build/arm64/usr/bin/basic.c.o
            build/arm64/usr/bin/editor.c.o
            build/arm64/usr/bin/gpio.c.o
            build/arm64/usr/shell/shell.c.o
        ) *(.data*)
    }

    . = ALIGN(16);
    _bss_start = .;

    .bss (NOLOAD) : {
        EXCLUDE_FILE (
            build/arm64/usr/bin/demo_a.c.o
            build/arm64/usr/bin/demo_b.c.o
            build/arm64/usr/bin/demo_c.c.o
            build/arm64/usr/bin/demo_d.c.o
            build/arm64/usr/bin/basic.c.o
            build/arm64/usr/bin/editor.c.o
            build/arm64/usr/bin/gpio.c.o
            build/arm64/usr/shell/shell.c.o
        ) *(.bss* COMMON)

        . = ALIGN(16);
    }

    _bss_end = .;

    . = ALIGN(16);

    .stack (NOLOAD) : ALIGN(16) {
        _stack_start = .;

        EXCLUDE_FILE (
            build/arm64/usr/bin/demo_a.c.o
            build/arm64/usr/bin/demo_b.c.o
            build/arm64/usr/bin/demo_c.c.o
            build/arm64/usr/bin/demo_d.c.o
            build/arm64/usr/bin/basic.c.o
            build/arm64/usr/bin/editor.c.o
            build/arm64/usr/bin/gpio.c.o
            build/arm64/usr/shell/shell.c.o
        ) *(.stack*)

        . += 64K;

        _stack_end = .;
    }

    . = ALIGN(16);

    __kernel_end = .;

    ASSERT(
        __kernel_end <= 0x00800000,
        "kernel overlaps user area"
    )

    . = 0x00800000;
    __user_area_start = .;

    /* DEMO_A専用領域 */
    __demo_a_start = .;

    .demo_a_text : {
        build/arm64/usr/bin/demo_a.c.o(.text*)
    }

    .demo_a_rodata : {
        build/arm64/usr/bin/demo_a.c.o(.rodata*)
    }

    .demo_a_data : {
        build/arm64/usr/bin/demo_a.c.o(.data*)
    }

    .demo_a_bss (NOLOAD) : {
        . = ALIGN(16);
        __demo_a_bss_start = .;

        build/arm64/usr/bin/demo_a.c.o(.bss*)
        build/arm64/usr/bin/demo_a.c.o(COMMON)

        . = ALIGN(16);
        __demo_a_bss_end = .;
    }

    .demo_a_stack (NOLOAD) : ALIGN(16) {
        __demo_a_stack_start = .;

        /* DEMO_A用スタックを64 KiB確保 */
        . += 64K;

        __demo_a_stack_end = .;
    }

    __demo_a_end = .;

    /* DEMO_B専用領域 */
    __demo_b_start = .;

    .demo_b_text : {
        build/arm64/usr/bin/demo_b.c.o(.text*)
    }

    .demo_b_rodata : {
        build/arm64/usr/bin/demo_b.c.o(.rodata*)
    }

    .demo_b_data : {
        build/arm64/usr/bin/demo_b.c.o(.data*)
    }

    .demo_b_bss (NOLOAD) : {
        . = ALIGN(16);
        __demo_b_bss_start = .;

        build/arm64/usr/bin/demo_b.c.o(.bss*)
        build/arm64/usr/bin/demo_b.c.o(COMMON)

        . = ALIGN(16);
        __demo_b_bss_end = .;
    }

    .demo_b_stack (NOLOAD) : ALIGN(16) {
        __demo_b_stack_start = .;

        /* DEMO_B用スタックを64 KiB確保 */
        . += 64K;

        __demo_b_stack_end = .;
    }

    __demo_b_end = .;

    /* DEMO_C専用領域 */
    __demo_c_start = .;

    .demo_c_text : {
        build/arm64/usr/bin/demo_c.c.o(.text*)
    }

    .demo_c_rodata : {
        build/arm64/usr/bin/demo_c.c.o(.rodata*)
    }

    .demo_c_data : {
        build/arm64/usr/bin/demo_c.c.o(.data*)
    }

    .demo_c_bss (NOLOAD) : {
        . = ALIGN(16);
        __demo_c_bss_start = .;

        build/arm64/usr/bin/demo_c.c.o(.bss*)
        build/arm64/usr/bin/demo_c.c.o(COMMON)

        . = ALIGN(16);
        __demo_c_bss_end = .;
    }

    .demo_c_stack (NOLOAD) : ALIGN(16) {
        __demo_c_stack_start = .;

        /* DEMO_C用スタックを64 KiB確保 */
        . += 64K;

        __demo_c_stack_end = .;
    }

    __demo_c_end = .;

    /* DEMO_D専用領域 */
    __demo_d_start = .;

    .demo_d_text : {
        build/arm64/usr/bin/demo_d.c.o(.text*)
    }

    .demo_d_rodata : {
        build/arm64/usr/bin/demo_d.c.o(.rodata*)
    }

    .demo_d_data : {
        build/arm64/usr/bin/demo_d.c.o(.data*)
    }

    .demo_d_bss (NOLOAD) : {
        . = ALIGN(16);
        __demo_d_bss_start = .;

        build/arm64/usr/bin/demo_d.c.o(.bss*)
        build/arm64/usr/bin/demo_d.c.o(COMMON)

        . = ALIGN(16);
        __demo_d_bss_end = .;
    }

    .demo_d_stack (NOLOAD) : ALIGN(16) {
        __demo_d_stack_start = .;

        /* DEMO_D用スタックを64 KiB確保 */
        . += 64K;

        __demo_d_stack_end = .;
    }

    __demo_d_end = .;

    /* BASIC専用領域 */
    __basic_start = .;

    .basic_text : {
        build/arm64/usr/bin/basic.c.o(.text*)
    }

    .basic_rodata : {
        build/arm64/usr/bin/basic.c.o(.rodata*)
    }

    .basic_data : {
        build/arm64/usr/bin/basic.c.o(.data*)
    }

    .basic_bss (NOLOAD) : {
        . = ALIGN(16);
        __basic_bss_start = .;

        build/arm64/usr/bin/basic.c.o(.bss*)
        build/arm64/usr/bin/basic.c.o(COMMON)

        . = ALIGN(16);
        __basic_bss_end = .;
    }

    .basic_stack (NOLOAD) : ALIGN(16) {
        __basic_stack_start = .;

        /* BASIC用スタックを64 KiB確保 */
        . += 64K;

        __basic_stack_end = .;
    }

    __basic_end = .;

    /* EDITOR専用領域 */
    __editor_start = .;

    .editor_text : {
        build/arm64/usr/bin/editor.c.o(.text*)
    }

    .editor_rodata : {
        build/arm64/usr/bin/editor.c.o(.rodata*)
    }

    .editor_data : {
        build/arm64/usr/bin/editor.c.o(.data*)
    }

    .editor_bss (NOLOAD) : {
        . = ALIGN(16);
        __editor_bss_start = .;

        build/arm64/usr/bin/editor.c.o(.bss*)
        build/arm64/usr/bin/editor.c.o(COMMON)

        . = ALIGN(16);
        __editor_bss_end = .;
    }

    .editor_stack (NOLOAD) : ALIGN(16) {
        __editor_stack_start = .;

        /* EDITOR用スタックを64 KiB確保 */
        . += 64K;

        __editor_stack_end = .;
    }

    __editor_end = .;

    /* GPIO専用領域 */
    __gpio_start = .;

    .gpio_text : {
        build/arm64/usr/bin/gpio.c.o(.text*)
    }

    .gpio_rodata : {
        build/arm64/usr/bin/gpio.c.o(.rodata*)
    }

    .gpio_data : {
        build/arm64/usr/bin/gpio.c.o(.data*)
    }

    .gpio_bss (NOLOAD) : {
        . = ALIGN(16);
        __gpio_bss_start = .;

        build/arm64/usr/bin/gpio.c.o(.bss*)
        build/arm64/usr/bin/gpio.c.o(COMMON)

        . = ALIGN(16);
        __gpio_bss_end = .;
    }

    .gpio_stack (NOLOAD) : ALIGN(16) {
        __gpio_stack_start = .;

        /* GPIO用スタックを64 KiB確保 */
        . += 64K;

        __gpio_stack_end = .;
    }

    __gpio_end = .;

    /* SHELL専用領域 */
    __shell_start = .;

    .shell_text : {
        build/arm64/usr/shell/shell.c.o(.text*)
    }

    .shell_rodata : {
        build/arm64/usr/shell/shell.c.o(.rodata*)
    }

    .shell_data : {
        build/arm64/usr/shell/shell.c.o(.data*)
    }

    .shell_bss (NOLOAD) : {
        . = ALIGN(16);
        __shell_bss_start = .;

        build/arm64/usr/shell/shell.c.o(.bss*)
        build/arm64/usr/shell/shell.c.o(COMMON)

        . = ALIGN(16);
        __shell_bss_end = .;
    }

    .shell_stack (NOLOAD) : ALIGN(16) {
        __shell_stack_start = .;

        /* SHELL用スタックを64 KiB確保 */
        . += 64K;

        __shell_stack_end = .;
    }

    __shell_end = .;
    __user_area_end = .;
}