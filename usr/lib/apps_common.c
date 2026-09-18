#include <usr/apps.h>

size_t app_strlen(const char *s) { size_t n = 0; while (s[n]) ++n; return n; }
void app_copy(char *d, const char *s) { while ((*d++ = *s++) != 0) {} }
void app_space(const char **p) { while (**p == ' ' || **p == '\t') ++*p; }
static char upper(char c) { return c >= 'a' && c <= 'z' ? (char)(c - 'a' + 'A') : c; }
static int wordchar(char c) {
    c = upper(c);
    return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}
int app_keyword(const char **p, const char *word) {
    const char *s = *p; size_t n = 0; app_space(&s);
    while (word[n] && upper(s[n]) == word[n]) ++n;
    if (word[n] || wordchar(s[n])) return 0;
    *p = s + n; return 1;
}
int app_end(const char *p) { app_space(&p); return *p == 0; }
int app_uint(const char **p, uint32_t *n, uint32_t max) {
    const char *s = *p; uint32_t v = 0; app_space(&s);
    if (*s < '0' || *s > '9') return 0;
    do {
        uint32_t digit = (uint32_t)(*s++ - '0');
        if (digit > max || v > (max - digit) / 10u) return 0;
        v = v * 10u + digit;
    } while (*s >= '0' && *s <= '9');
    *p = s; *n = v; return 1;
}
int app_path(const char *p, char *dst) {
    size_t n = 0; int quoted; app_space(&p); quoted = *p == '"'; if (quoted) ++p;
    while (*p && (quoted ? *p != '"' : (*p != ' ' && *p != '\t'))) {
        if (n + 1 >= APP_PATH_CAP || (unsigned char)*p < 32 || (unsigned char)*p > 126) return 0;
        dst[n++] = *p++;
    }
    if (quoted && *p++ != '"') return 0;
    if (!n || !app_end(p)) return 0;
    dst[n] = 0; return 1;
}
int app_io_valid(const struct app_io *io) { return io && io->try_getc && io->putc && io->yield; }
void app_puts(struct app_io *io, const char *s) {
    while (*s) { if (*s == '\n') io->putc(io->user, '\r'); io->putc(io->user, *s++); }
}
void app_number(struct app_io *io, int32_t n) {
    char b[11]; size_t i = 0; uint32_t v;
    if (n < 0) { io->putc(io->user, '-'); v = (uint32_t)(-(int64_t)n); }
    else v = (uint32_t)n;
    do { b[i++] = (char)('0' + v % 10u); v /= 10u; } while (v);
    while (i) io->putc(io->user, b[--i]);
}
void app_error(struct app_io *io, const char *s) { app_puts(io, "? "); app_puts(io, s); app_puts(io, "\n"); }
int app_readline(struct app_io *io, char *dst, size_t cap) {
    size_t n = 0; unsigned overflow = 0;
    if (!cap) return APP_READ_LONG;
    dst[0] = 0;
    for (;;) {
        char ch; int rc;
        if (io->cancelled && io->cancelled(io->user)) {
            io->escape_state = 0; dst[0] = 0; app_puts(io, "^C\n"); return APP_READ_CANCEL;
        }
        rc = io->try_getc(io->user, &ch);
        if (rc < 0) { dst[0] = 0; return APP_READ_EOF; }
        if (!rc) { io->yield(io->user); continue; }
        if (io->skip_lf) { io->skip_lf = 0; if (ch == '\n') continue; }
        if (ch == 3) { io->escape_state = 0; dst[0] = 0; app_puts(io, "^C\n"); return APP_READ_CANCEL; }
        if (ch == '\r' || ch == '\n') {
            io->escape_state = 0; io->skip_lf = ch == '\r'; dst[n] = 0;
            app_puts(io, "\n"); return overflow ? APP_READ_LONG : APP_READ_OK;
        }
        /* Consume ANSI arrow/function-key sequences; this is a line editor. */
        if (io->escape_state) {
            if (io->escape_state == 1) io->escape_state = (ch == '[' || ch == 'O') ? 2u : 0u;
            else if ((unsigned char)ch >= 0x40 && (unsigned char)ch <= 0x7e) io->escape_state = 0;
            continue;
        }
        if (ch == 27) { io->escape_state = 1; continue; }
        /* A rejected long line is drained completely; it can never execute a prefix. */
        if (overflow) continue;
        if (ch == 8 || ch == 127) { if (n) { --n; app_puts(io, "\b \b"); } continue; }
        if (((unsigned char)ch < 32 && ch != '\t') || (unsigned char)ch > 126) continue;
        if (n + 1 >= cap) { overflow = 1; continue; }
        dst[n++] = ch; io->putc(io->user, ch);
    }
}

static int same(const char *a, const char *b) {
    while (*a && *a == *b) { ++a; ++b; } return *a == *b;
}
int ramfs_read(void *opaque, const char *path, char *dst, size_t cap, size_t *len) {
    struct ramfs *fs = opaque; size_t i, j;
    if (!fs || !path || !dst || !len) return -1;
    for (i = 0; i < RAMFS_SLOTS; ++i) if (fs->files[i].used && same(path, fs->files[i].path)) {
        if (fs->files[i].len > cap) return -1;
        for (j = 0; j < fs->files[i].len; ++j) dst[j] = fs->files[i].data[j];
        *len = fs->files[i].len; return 0;
    }
    return -1;
}
int ramfs_write(void *opaque, const char *path, const char *src, size_t len) {
    struct ramfs *fs = opaque; size_t i, j, slot = RAMFS_SLOTS;
    if (!fs || !path || !src || !*path || app_strlen(path) >= APP_PATH_CAP || len > APP_FILE_CAP) return -1;
    for (i = 0; i < RAMFS_SLOTS; ++i) {
        if (fs->files[i].used && same(path, fs->files[i].path)) { slot = i; break; }
        if (!fs->files[i].used && slot == RAMFS_SLOTS) slot = i;
    }
    if (slot == RAMFS_SLOTS) return -1;
    for (j = 0; j < len; ++j) fs->files[slot].data[j] = src[j];
    app_copy(fs->files[slot].path, path); fs->files[slot].len = len; fs->files[slot].used = 1;
    return 0;
}

int app_resolve_path(struct app_io *io, const char *path, char *out)
{
    if (!path || !*path || app_strlen(path) >= APP_PATH_CAP) return -1;
    if (io->resolve_path) return io->resolve_path(io->fs, path, out, APP_PATH_CAP);
    app_copy(out, path);
    return 0;
}
