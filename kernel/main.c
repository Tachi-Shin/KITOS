#include <dos/type.h>
#include <dos/config.h>

#include <block/block.h>
#include <drivers/uart/uart.h>
#include <fs/fat32.h>

#include <arch/arm64/kernel/exception.h>
#include <arch/arm64/kernel/timer.h>
#include <arch/arm64/rpi4/gicctl.h>

#include <kernel/printk.h>
#include <kernel/task.h>

#include <usr/shell.h>
#include <usr/fs_commands.h>

extern void EnableInt(void);

static struct fat32_fs root_fs;

static const uint64_t data_partition_lba[BLOCK_DEVICE_COUNT] = {
    QEMU_DATA_PARTITION_LBA,
    RPI4_DATA_PARTITION_LBA,
};

static bool storage_smoke_test(void)
{
    struct block_device *device = NULL;
    struct fat32_file_info info;

    uint8_t sample[64];
    uint32_t bytes_read;

    block_register_devices();

    for (unsigned int index = 0U;
         index < BLOCK_DEVICE_COUNT;
         index++) {
        device = block_get_device(index);

        if (device == NULL) {
            continue;
        }

        printk(
            "[block device] initialize %s\n",
            device->name
        );

        if (block_init(device) == 0 &&
            fat32_mount_at(
                &root_fs,
                device,
                data_partition_lba[index]
            ) == 0) {
                (void)apps_fs_bind(&root_fs);
            printk(
                "[block device] block device ready: %s\n",
                device->name
            );

            break;
        }

        device = NULL;
    }

    if (device == NULL) {
        printk(
            "[block device] no SD block device initialized\n"
        );
        return false;
    }

    printk(
        "[file system] FAT32 mounted at LBA %llu\n",
        (unsigned long long)root_fs.volume_lba
    );

    if (fat32_stat(
            &root_fs,
            "/HELLO.TXT",
            &info
        ) == 0 &&
        fat32_read_file(
            &root_fs,
            "/HELLO.TXT",
            sample,
            sizeof(sample),
            &bytes_read
        ) == 0) {
        printk(
            "[file system] HELLO.TXT size=%u, first read=%u bytes\n",
            info.size,
            bytes_read
        );
    } else {
        printk(
            "[file system] HELLO.TXT is absent (mount test still passed)\n"
        );
    }

    return true;
}

static bool kernel_init(void)
{
    struct uart_device *console;

    uart_register_devices();

    console = uart_get_device(0U);

    if (console == NULL) {
        return false;
    }

    if (uart_init(console) != 0) {
        return false;
    }

    printk_set_console(console);

    printk("[init] UART initialize OK\n");
    printk("[init] kernel initialize start\n");

    if (!storage_smoke_test()) {
        printk("[storage] storage initialize failed\n");
        return false;
    }

    SetVectorTable();

    printk("[vector table] set vector table\n");

    SetupGIC();

    /* Mini UART受信割込み */
    ActivateInterrupt(
        console->irq_intid,
        8U,
        false
    );

    if (uart_enable_rx_irq(console) != 0) {
        printk("[uart] RX interrupt setup failed\n");
        return false;
    }

    printk("[uart] RX interrupt enabled\n");

    /* タイマー割込み */
    arch_timer_start(30U);

    EnableInt();

    return true;
}

static void clearbss(void)
{
    extern unsigned long long _bss_start[];
    extern unsigned long long _bss_end[];

    for (unsigned long long *p = _bss_start;
         p < _bss_end;
         p++) {
        *p = 0ULL;
    }
}

void main(void)
{
    clearbss();

    if (kernel_init()) {
        int shell_id = task_create(
            "shell",
            shell_task,
            NULL
        );

        if (shell_id < 0 ||
            task_protect((uint32_t)shell_id) != 0) {
            printk("[task] failed to create shell\n");
            goto halt;
        }

        printk(
            "[task] start shell: id=%u\n",
            (unsigned int)shell_id
        );

        task_start();
    } else {
        printk("[kernel] boot failed\n");
    }

halt:
    for (;;) {
        __asm__ volatile("wfi" ::: "memory");
    }
}