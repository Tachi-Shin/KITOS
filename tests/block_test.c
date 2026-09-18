#include <stdio.h>
#include <string.h>

#include <block/block.h>

#define TEST_SECTORS 8U

static unsigned char storage[TEST_SECTORS * BLOCK_SECTOR_SIZE];

static int memory_init(struct block_device *device)
{
    (void)device;
    return 0;
}

static int memory_read(struct block_device *device, uint64_t lba,
                       uint32_t count, void *buffer)
{
    (void)device;
    memcpy(buffer, storage + (size_t)lba * BLOCK_SECTOR_SIZE,
           (size_t)count * BLOCK_SECTOR_SIZE);
    return 0;
}

static int memory_write(struct block_device *device, uint64_t lba,
                        uint32_t count, const void *buffer)
{
    (void)device;
    memcpy(storage + (size_t)lba * BLOCK_SECTOR_SIZE, buffer,
           (size_t)count * BLOCK_SECTOR_SIZE);
    return 0;
}

static const struct block_device_ops memory_ops = {
    .init = memory_init,
    .read = memory_read,
    .write = memory_write,
};

/* block.cの固定デバイス登録がリンクできるようにするテスト用stub。 */
struct block_device *sd_get_qemu_block_device(void)
{
    return NULL;
}

struct block_device *sd_get_emmc2_block_device(void)
{
    return NULL;
}

int main(void)
{
    struct block_device device = {
        .name = "memory block test",
        .block_size = BLOCK_SECTOR_SIZE,
        .block_count = TEST_SECTORS,
        .ops = &memory_ops,
    };
    unsigned char written[BLOCK_SECTOR_SIZE * 2U];
    unsigned char read_back[BLOCK_SECTOR_SIZE * 2U];
    size_t index;

    for (index = 0U; index < sizeof(written); index++) {
        written[index] = (unsigned char)((index * 37U + 11U) & 0xFFU);
    }
    memset(read_back, 0, sizeof(read_back));

    if (block_read(&device, 0U, 1U, read_back) == 0 ||
        block_init(&device) != 0 ||
        block_write(&device, 2U, 2U, written) != 0 ||
        block_read(&device, 2U, 2U, read_back) != 0 ||
        memcmp(written, read_back, sizeof(written)) != 0 ||
        block_read(&device, TEST_SECTORS, 1U, read_back) == 0 ||
        block_write(&device, TEST_SECTORS - 1U, 2U, written) == 0) {
        fprintf(stderr, "block LBA test failed\n");
        return 1;
    }

    printf("block LBA read/write test: PASS\n");
    return 0;
}
