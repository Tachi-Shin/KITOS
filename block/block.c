#include <block/block.h>
#include <drivers/mmc/sd.h>

static struct block_device *block_devices[BLOCK_DEVICE_COUNT];

void block_register_devices(void)
{
    /* QEMU raspi4bのSDカードは先頭、実機eMMC2は次に置く。 */
    block_devices[0] = sd_get_qemu_block_device();
    block_devices[1] = sd_get_emmc2_block_device();
}

struct block_device *block_get_device(unsigned int index)
{
    if (index >= BLOCK_DEVICE_COUNT) {
        return NULL;
    }
    return block_devices[index];
}

static int block_request_valid(struct block_device *device, uint64_t lba,
                               uint32_t count, const void *buffer)
{
    uint64_t end;

    if (device == NULL || device->ops == NULL || buffer == NULL ||
        count == 0U || !device->initialized) {
        return -1;
    }

    end = lba + (uint64_t)count;
    if (end < lba) {
        return -1;
    }
    if (device->block_count != 0ULL && end > device->block_count) {
        return -1;
    }
    return 0;
}

int block_init(struct block_device *device)
{
    int result;

    if (device == NULL || device->ops == NULL || device->ops->init == NULL) {
        return -1;
    }
    if (device->initialized) {
        return 0;
    }

    result = device->ops->init(device);
    if (result == 0) {
        device->initialized = true;
    }
    return result;
}

int block_read(struct block_device *device, uint64_t lba,
               uint32_t count, void *buffer)
{
    if (block_request_valid(device, lba, count, buffer) != 0 ||
        device->ops->read == NULL) {
        return -1;
    }
    return device->ops->read(device, lba, count, buffer);
}

int block_write(struct block_device *device, uint64_t lba,
                uint32_t count, const void *buffer)
{
    if (block_request_valid(device, lba, count, buffer) != 0 ||
        device->ops->write == NULL) {
        return -1;
    }
    return device->ops->write(device, lba, count, buffer);
}
