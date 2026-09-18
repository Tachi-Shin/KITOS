#ifndef DOS_FS_FAT32_H
#define DOS_FS_FAT32_H

#include <dos/type.h>

struct block_device;

#define FAT32_MAX_NAME_UTF8 768U

#define FAT32_ATTR_READ_ONLY 0x01U
#define FAT32_ATTR_HIDDEN    0x02U
#define FAT32_ATTR_SYSTEM    0x04U
#define FAT32_ATTR_VOLUME_ID 0x08U
#define FAT32_ATTR_DIRECTORY 0x10U
#define FAT32_ATTR_ARCHIVE   0x20U

struct fat32_file_info {
    char name[FAT32_MAX_NAME_UTF8];
    uint8_t attributes;
    uint32_t first_cluster;
    uint32_t size;
};

struct fat32_fs {
    struct block_device *device;
    uint64_t volume_lba;
    uint32_t total_sectors;
    uint32_t sectors_per_fat;
    uint32_t first_fat_sector;
    uint32_t first_data_sector;
    uint32_t root_cluster;
    uint32_t fsinfo_sector;
    uint32_t backup_boot_sector;
    uint32_t cluster_count;
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint8_t fat_count;
    uint8_t active_fat;
    bool mirror_fats;
    bool mounted;
};

int fat32_mount(struct fat32_fs *fs, struct block_device *device);
int fat32_mount_at(struct fat32_fs *fs, struct block_device *device,
                   uint64_t volume_lba);
int fat32_stat(struct fat32_fs *fs, const char *path,
               struct fat32_file_info *info);
int fat32_read_file(struct fat32_fs *fs, const char *path,
                    void *buffer, uint32_t capacity, uint32_t *bytes_read);
int fat32_write_file(struct fat32_fs *fs, const char *path,
                     const void *data, uint32_t size);
int fat32_remove_file(struct fat32_fs *fs, const char *path);

/* Callback receives one non-dot, non-volume entry; return 0 to continue.
   Operations require external serialization. No callback may mutate this fs. */
typedef int (*fat32_list_callback)(void *user, const struct fat32_file_info *info);
int fat32_list_dir(struct fat32_fs *fs, const char *path,
                   fat32_list_callback callback, void *user);
int fat32_mkdir(struct fat32_fs *fs, const char *path);
/* Existing regular files are preserved. No RTC timestamp update is provided. */
int fat32_touch(struct fat32_fs *fs, const char *path);

#endif
