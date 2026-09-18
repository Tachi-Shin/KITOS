#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <block/block.h>
#include <fs/fat32.h>

struct file_context {
    int fd;
};

static int file_init(struct block_device *device)
{
    (void)device;
    return 0;
}

static int file_read(struct block_device *device, uint64_t lba,
                     uint32_t count, void *buffer)
{
    struct file_context *context = device->private_data;
    size_t bytes = (size_t)count * BLOCK_SECTOR_SIZE;
    return pread(context->fd, buffer, bytes,
                 (off_t)(lba * BLOCK_SECTOR_SIZE)) == (ssize_t)bytes ? 0 : -1;
}

static int file_write(struct block_device *device, uint64_t lba,
                      uint32_t count, const void *buffer)
{
    struct file_context *context = device->private_data;
    size_t bytes = (size_t)count * BLOCK_SECTOR_SIZE;
    return pwrite(context->fd, buffer, bytes,
                  (off_t)(lba * BLOCK_SECTOR_SIZE)) == (ssize_t)bytes ? 0 : -1;
}

static const struct block_device_ops file_ops = {
    .init = file_init,
    .read = file_read,
    .write = file_write,
};

struct block_device *sd_get_qemu_block_device(void) { return NULL; }
struct block_device *sd_get_emmc2_block_device(void) { return NULL; }

static int open_device(const char *path, struct block_device *device,
                       struct file_context *context)
{
    struct stat status;
    context->fd = open(path, O_RDWR);
    if (context->fd < 0 || fstat(context->fd, &status) != 0) {
        perror(path);
        return -1;
    }
    memset(device, 0, sizeof(*device));
    device->name = path;
    device->block_size = BLOCK_SECTOR_SIZE;
    device->block_count = (uint64_t)status.st_size / BLOCK_SECTOR_SIZE;
    device->ops = &file_ops;
    device->private_data = context;
    return block_init(device);
}

static uint32_t little32(const unsigned char *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static void store32(unsigned char *data, uint32_t value)
{
    data[0] = (unsigned char)value;
    data[1] = (unsigned char)(value >> 8);
    data[2] = (unsigned char)(value >> 16);
    data[3] = (unsigned char)(value >> 24);
}

static int prepare_mbr(struct block_device *device)
{
    unsigned char mbr[BLOCK_SECTOR_SIZE];
    memset(mbr, 0, sizeof(mbr));
    mbr[446U + 4U] = 0x0CU;
    store32(mbr + 446U + 8U, 2048U);
    store32(mbr + 446U + 12U, (uint32_t)device->block_count - 2048U);
    mbr[510] = 0x55U;
    mbr[511] = 0xAAU;
    return block_write(device, 0U, 1U, mbr);
}

static int read_fsinfo_count(struct fat32_fs *fs, uint32_t *count)
{
    unsigned char sector[BLOCK_SECTOR_SIZE];
    if (block_read(fs->device, fs->volume_lba + fs->fsinfo_sector,
                   1U, sector) != 0) {
        return -1;
    }
    *count = little32(sector + 488U);
    return 0;
}

static int fats_match(struct fat32_fs *fs)
{
    unsigned char first[BLOCK_SECTOR_SIZE];
    unsigned char other[BLOCK_SECTOR_SIZE];
    uint32_t sector;
    uint8_t fat;
    for (sector = 0U; sector < fs->sectors_per_fat; sector++) {
        if (block_read(fs->device,
                       fs->volume_lba + fs->first_fat_sector + sector,
                       1U, first) != 0) {
            return -1;
        }
        for (fat = 1U; fat < fs->fat_count; fat++) {
            if (block_read(fs->device, fs->volume_lba + fs->first_fat_sector +
                           (uint32_t)fat * fs->sectors_per_fat + sector,
                           1U, other) != 0 ||
                memcmp(first, other, sizeof(first)) != 0) {
                return -1;
            }
        }
    }
    return 0;
}

static int expect_file(struct fat32_fs *fs, const char *path,
                       const unsigned char *expected, uint32_t size)
{
    unsigned char *buffer = malloc(size == 0U ? 1U : size);
    uint32_t read_size = 0U;
    int result = buffer == NULL ? -1 :
        fat32_read_file(fs, path, buffer, size, &read_size);
    if (result != 0 || read_size != size ||
        memcmp(buffer, expected, size) != 0) {
        fprintf(stderr, "file verification failed: %s\n", path);
        free(buffer);
        return -1;
    }
    free(buffer);
    return 0;
}

static int test_direct_image(const char *path)
{
    struct block_device device;
    struct file_context context;
    struct fat32_fs fs;
    struct fat32_file_info info;
    unsigned char large[7000];
    unsigned char small[19];
    uint32_t free_before;
    uint32_t free_after_create;
    uint32_t free_after_remove;
    size_t i;
    int result = -1;

    for (i = 0U; i < sizeof(large); i++) large[i] = (unsigned char)(i * 29U);
    for (i = 0U; i < sizeof(small); i++) small[i] = (unsigned char)(0xA0U + i);
    if (open_device(path, &device, &context) != 0 ||
        fat32_mount(&fs, &device) != 0 || fs.volume_lba != 0U ||
        fat32_write_file(&fs, "/HELLO.TXT", "initial-data\n", 13U) != 0 ||
        expect_file(&fs, "/HELLO.TXT", (const unsigned char *)"initial-data\n", 13U) != 0 ||
        expect_file(&fs, "/Long File Name.txt", (const unsigned char *)"long-initial\n", 13U) != 0 ||
        read_fsinfo_count(&fs, &free_before) != 0 ||
        fat32_write_file(&fs, "/Generated Long Filename.bin", large,
                         sizeof(large)) != 0 ||
        fat32_stat(&fs, "/Generated Long Filename.bin", &info) != 0 ||
        info.size != sizeof(large) ||
        expect_file(&fs, "/Generated Long Filename.bin", large,
                    sizeof(large)) != 0 ||
        read_fsinfo_count(&fs, &free_after_create) != 0 ||
        (free_before != 0xFFFFFFFFU && free_after_create >= free_before) ||
        fat32_write_file(&fs, "/Generated Long Filename.bin", small,
                         sizeof(small)) != 0 ||
        expect_file(&fs, "/Generated Long Filename.bin", small,
                    sizeof(small)) != 0 ||
        fat32_write_file(&fs, "/HELLO.TXT", large, 1800U) != 0 ||
        expect_file(&fs, "/HELLO.TXT", large, 1800U) != 0 ||
        fats_match(&fs) != 0 ||
        fat32_remove_file(&fs, "/Generated Long Filename.bin") != 0 ||
        fat32_write_file(&fs, "/HELLO.TXT", "initial-data\n", 13U) != 0 ||
        read_fsinfo_count(&fs, &free_after_remove) != 0 ||
        (free_before != 0xFFFFFFFFU && free_after_remove != free_before) ||
        fat32_stat(&fs, "/Generated Long Filename.bin", &info) == 0 ||
        fats_match(&fs) != 0) {
        fprintf(stderr, "direct FAT32 test failed\n");
        goto out;
    }
    result = 0;
out:
    close(context.fd);
    return result;
}

static int test_mbr_image(const char *path)
{
    struct block_device device;
    struct file_context context;
    struct fat32_fs fs;
    int result = -1;
    if (open_device(path, &device, &context) != 0 ||
        prepare_mbr(&device) != 0 || fat32_mount(&fs, &device) != 0 ||
        fs.volume_lba != 2048U ||
        fat32_mount_at(&fs, &device, 2048U) != 0 ||
        fs.volume_lba != 2048U ||
        expect_file(&fs, "/MBRFILE.TXT",
                    (const unsigned char *)"mbr-data\n", 9U) != 0) {
        fprintf(stderr, "MBR FAT32 test failed\n");
        goto out;
    }
    result = 0;
out:
    close(context.fd);
    return result;
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: %s DIRECT_IMAGE MBR_IMAGE\n", argv[0]);
        return 2;
    }
    if (test_direct_image(argv[1]) != 0 || test_mbr_image(argv[2]) != 0) {
        return 1;
    }
    printf("FAT32 direct/MBR, 8.3/LFN, read/write test: PASS\n");
    return 0;
}
