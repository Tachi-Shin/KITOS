#include <usr/fs_commands.h>
#include <drivers/uart/uart.h>

static struct fat32_fs *volume;
static char working_directory[APP_PATH_CAP] = "/";

/* ここから追加 */
const char *apps_fs_cwd(void)
{
    return working_directory;
}
/* ここまで追加 */

/* この下のapps_fs_bind()以降は既存のまま */

int apps_fs_bind(struct fat32_fs *fs)
{
    if (!fs || !fs->mounted) return -1;
    volume = fs;
    working_directory[0] = '/'; working_directory[1] = 0;
    return 0;
}

/* Resolve into a separate buffer. Every traversed component must be a directory,
   including the component before '..'. A missing final name is allowed. */
int apps_fs_resolve(void *unused, const char *path, char *out, size_t capacity)
{
    size_t n, start, count;
    const char *p = path;
    struct fat32_file_info info;
    (void)unused;
    if (!volume || !p || !*p || !out || capacity < 2U) return -1;
    if (*p == '/') { out[0] = '/'; out[1] = 0; n = 1U; }
    else {
        n = app_strlen(working_directory);
        if (n >= capacity) return -1;
        app_copy(out, working_directory);
    }
    while (*p) {
        while (*p == '/') ++p;
        if (!*p) break;
        start = 0U;
        while (p[start] && p[start] != '/') ++start;
        count = start;
        if (count == 1U && p[0] == '.') { p += count; continue; }
        if (count == 2U && p[0] == '.' && p[1] == '.') {
            while (n > 1U && out[n - 1U] != '/') --n;
            if (n > 1U) --n;
            out[n] = 0; p += count; continue;
        }
        if (n + (n > 1U) + count >= capacity) return -1;
        if (n > 1U) out[n++] = '/';
        while (count--) out[n++] = *p++;
        out[n] = 0;
        if (*p == '/' && (fat32_stat(volume, out, &info) != 0 ||
            !(info.attributes & FAT32_ATTR_DIRECTORY))) return -1;
    }
    return 0;
}

int apps_fs_read(void *unused, const char *path, char *dst,
                 size_t capacity, size_t *length)
{
    char absolute[APP_PATH_CAP];
    struct fat32_file_info info;
    uint32_t received;
    (void)unused;
    if (length) *length = 0U;
    if (!dst || !length || apps_fs_resolve(NULL, path, absolute, sizeof(absolute)) != 0 ||
        fat32_stat(volume, absolute, &info) != 0 ||
        (info.attributes & FAT32_ATTR_DIRECTORY) || info.size > capacity) return -1;
    if (fat32_read_file(volume, absolute, dst, info.size, &received) != 0 ||
        received != info.size) return -1;
    *length = received;
    return 0;
}

int apps_fs_write(void *unused, const char *path, const char *src, size_t length)
{
    char absolute[APP_PATH_CAP];
    (void)unused;
    if (length > 0xFFFFFFFFU ||
        apps_fs_resolve(NULL, path, absolute, sizeof(absolute)) != 0) return -1;
    return fat32_write_file(volume, absolute, src, (uint32_t)length);
}

static void say(struct uart_device *console, const char *text)
{
    while (*text) uart_putc(console, *text++);
}

static void safe_name(struct uart_device *console, const char *text)
{
    while (*text) {
        unsigned char c = (unsigned char)*text++;
        uart_putc(console, c < 32U || c == 127U ? '?' : (char)c);
    }
}

static int list_one(void *user, const struct fat32_file_info *info)
{
    struct uart_device *console = user;
    char digits[10];
    unsigned int n = 0U;
    uint32_t size = info->size;
    say(console, (info->attributes & FAT32_ATTR_DIRECTORY) ? "d " : "- ");
    do { digits[n++] = (char)('0' + size % 10U); size /= 10U; } while (size);
    while (n) uart_putc(console, digits[--n]);
    say(console, "  "); safe_name(console, info->name); say(console, "\r\n");
    return 0;
}

int apps_fs_command(struct uart_device *console, const char *line)
{
    enum { LS, PWD, CD, MKDIR, TOUCH } command;
    const char *p = line;
    char argument[APP_PATH_CAP], absolute[APP_PATH_CAP];
    struct fat32_file_info info;
    int rc = -1;
    if (!console || !line) return 0;
    if (app_keyword(&p, "LS") || app_keyword(&p, "DIR")) command = LS;
    else if (app_keyword(&p, "PWD")) command = PWD;
    else if (app_keyword(&p, "CD")) command = CD;
    else if (app_keyword(&p, "MKDIR")) command = MKDIR;
    else if (app_keyword(&p, "TOUCH")) command = TOUCH;
    else return 0;
    if (!volume) { say(console, "Filesystem is not mounted/bound.\r\n"); return 1; }
    if (command == PWD) {
        if (!app_end(p)) say(console, "Usage: pwd\r\n");
        else { safe_name(console, working_directory); say(console, "\r\n"); }
        return 1;
    }
    if (app_end(p) && (command == LS || command == CD))
        app_copy(argument, command == CD ? "/" : ".");
    else if (!app_path(p, argument)) {
        say(console, "Expected one path; quote paths containing spaces.\r\n"); return 1;
    }
    if (apps_fs_resolve(NULL, argument, absolute, sizeof(absolute)) != 0) {
        say(console, "Invalid path, missing parent, or path too long.\r\n"); return 1;
    }
    if (command == LS) {
        rc = fat32_stat(volume, absolute, &info);
        if (rc == 0) rc = (info.attributes & FAT32_ATTR_DIRECTORY)
            ? fat32_list_dir(volume, absolute, list_one, console) : list_one(console, &info);
    } else if (command == CD) {
        rc = fat32_stat(volume, absolute, &info);
        if (rc == 0 && (info.attributes & FAT32_ATTR_DIRECTORY))
            app_copy(working_directory, absolute);
        else rc = -1;
    } else if (command == MKDIR) rc = fat32_mkdir(volume, absolute);
    else rc = fat32_touch(volume, absolute);
    if (rc != 0) say(console, "Filesystem operation failed (path, space, or device I/O).\r\n");
    return 1;
}
