#ifndef USR_FS_COMMANDS_H
#define USR_FS_COMMANDS_H
#include <usr/apps.h>
#include <fs/fat32.h>

/* Bind once, after boot mount and before starting the shell. fs must remain live.
   One foreground shell serializes these calls. Other tasks must not access the
   same FAT32 volume concurrently without a filesystem-wide lock. */
int apps_fs_bind(struct fat32_fs *fs);
int apps_fs_resolve(void *unused, const char *path, char *out, size_t capacity);
int apps_fs_read(void *unused, const char *path, char *dst, size_t capacity, size_t *length);
int apps_fs_write(void *unused, const char *path, const char *src, size_t length);
int apps_fs_command(struct uart_device *console, const char *line);
#endif
