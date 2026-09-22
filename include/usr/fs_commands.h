#ifndef USR_FS_COMMANDS_H
#define USR_FS_COMMANDS_H

#include <usr/apps.h>
#include <fs/fat32.h>

/*
 * 起動時のマウント後、シェル起動前に呼ぶ。
 * fsは利用中ずっと有効であること。
 *
 * 同じFAT32ボリュームへのアクセスは、
 * 単一のフォアグラウンドシェルで直列化する。
 */
int apps_fs_bind(struct fat32_fs *fs);

/*
 * 現在のディレクトリへの読み取り専用ポインタを返す。
 * 次の成功したcdまたはbindで内容が変わる。
 * bind前は"/"を返す。
 */
const char *apps_fs_cwd(void);

int apps_fs_resolve(
    void *unused,
    const char *path,
    char *out,
    size_t capacity
);

int apps_fs_read(
    void *unused,
    const char *path,
    char *dst,
    size_t capacity,
    size_t *length
);

int apps_fs_write(
    void *unused,
    const char *path,
    const char *src,
    size_t length
);

int apps_fs_command(
    struct uart_device *console,
    const char *line
);

#endif