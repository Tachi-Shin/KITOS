#ifndef DOS_BLOCK_BLOCK_H
#define DOS_BLOCK_BLOCK_H

#include <dos/type.h>

#define BLOCK_DEVICE_COUNT 2U
#define BLOCK_SECTOR_SIZE  512U

struct block_device;

struct block_device_ops {
    int (*init)(struct block_device *device);
    int (*read)(struct block_device *device, uint64_t lba,
                uint32_t count, void *buffer);
    int (*write)(struct block_device *device, uint64_t lba,
                 uint32_t count, const void *buffer);
};

struct block_device {
    const char *name;
    uint32_t block_size;
    uint64_t block_count;
    const struct block_device_ops *ops;
    void *private_data;
    bool initialized;
};

void block_register_devices(void);
struct block_device *block_get_device(unsigned int index);

int block_init(struct block_device *device);
int block_read(struct block_device *device, uint64_t lba,
               uint32_t count, void *buffer);
int block_write(struct block_device *device, uint64_t lba,
                uint32_t count, const void *buffer);

#endif
