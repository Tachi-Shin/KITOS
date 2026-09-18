#ifndef USR_APPS_H
#define USR_APPS_H

#include <stddef.h>
#include <dos/type.h>

#ifndef INT32_MAX
#define INT32_MAX 2147483647
#endif
#ifndef INT32_MIN
#define INT32_MIN (-2147483647 - 1)
#endif
#ifndef UINT32_C
#define UINT32_C(value) value##U
#endif

#define APP_LINE_CAP 192u
#define APP_PATH_CAP 256u
#define APP_FILE_CAP 32768u
#define BASIC_MAX_LINES 128u
#define BASIC_RETURN_DEPTH 16u
#define EDITOR_MAX_LINES 128u
#define RAMFS_SLOTS 4u

/* All callbacks run in task context. No allocator or hosted libc is required. */
struct app_io {
    void *user;
    /* 1: byte written to out; 0: no input; -1: closed/error. NONBLOCKING. */
    int (*try_getc)(void *user, char *out);
    void (*putc)(void *user, char c);
    /* Must yield or sleep when input is absent. Called once per BASIC statement. */
    void (*yield)(void *user);
    /* Optional, non-consuming cancellation check. It must NOT steal normal input.
       Return nonzero for Ctrl-C/cancellation. Required for breaking endless RUN. */
    int (*cancelled)(void *user);
    /* Optional task completion notification, e.g. release a foreground waiter. */
    void (*finished)(void *user);
    void *fs;
    /* 0: success, nonzero: failure. Reject oversized reads; never truncate.
       write_file replaces the WHOLE file, truncates old contents, and creates it.
       NULL means unavailable. On write failure dirty state is preserved. */
    int (*read_file)(void *fs, const char *path, char *dst, size_t cap, size_t *len);
    int (*write_file)(void *fs, const char *path, const char *src, size_t len);
    /* Optional canonical path resolver. Output must not alias input. */
    int (*resolve_path)(void *fs, const char *path, char *out, size_t capacity);
    /* Terminal state must persist across readline calls and foreground sessions. */
    unsigned skip_lf;
    unsigned escape_state;
};

enum app_read_result { APP_READ_OK, APP_READ_CANCEL, APP_READ_EOF, APP_READ_LONG };
void app_puts(struct app_io *io, const char *s);
void app_number(struct app_io *io, int32_t n);
int app_readline(struct app_io *io, char *dst, size_t cap);
size_t app_strlen(const char *s);
void app_copy(char *dst, const char *src);
void app_space(const char **p);
int app_keyword(const char **p, const char *word);
int app_end(const char *p);
int app_uint(const char **p, uint32_t *n, uint32_t max);
int app_path(const char *p, char *dst); /* bare or quoted path; exact end required */
int app_resolve_path(struct app_io *io, const char *path, char *out);
int app_io_valid(const struct app_io *io);
void app_error(struct app_io *io, const char *s);

struct basic_line { uint32_t number; char text[APP_LINE_CAP]; };
struct basic_program { size_t count; struct basic_line lines[BASIC_MAX_LINES]; };
struct basic_context {
    struct app_io *io;
    struct basic_program program, staging;
    int32_t vars[26];
    size_t returns[BASIC_RETURN_DEPTH], return_count;
    char command[APP_LINE_CAP], file_data[APP_FILE_CAP], path[APP_PATH_CAP];
    unsigned dirty;
};
/* Contexts must be zero-initialized, long-lived, and exclusive to one live task.
   They are intentionally large: allocate in .bss or heap, NEVER task stack.
   Set ctx.io before calling. Normal return is handled by task_bootstrap(). */
void basic_task(void *argument);
/* Returns 0 on success, 1 on BASIC error, 2 on cancellation. Clears variables. */
int basic_run(struct basic_context *ctx);
/* Editor for BASIC source storage: insert/replace/delete "10 PRINT ...". */
int basic_store_line(struct basic_program *program, const char *text);

struct editor_buffer { size_t count; char lines[EDITOR_MAX_LINES][APP_LINE_CAP]; };
struct editor_context {
    struct app_io *io;
    struct editor_buffer buffer, staging;
    char command[APP_LINE_CAP], file_data[APP_FILE_CAP], path[APP_PATH_CAP];
    unsigned dirty;
};
void editor_task(void *argument);

/* Demonstration storage. Volatile across reboot; external serialization required
   if multiple tasks access the SAME ramfs concurrently. */
struct ramfs_file { unsigned used; char path[APP_PATH_CAP]; size_t len; char data[APP_FILE_CAP]; };
struct ramfs { struct ramfs_file files[RAMFS_SLOTS]; };
int ramfs_read(void *fs, const char *path, char *dst, size_t cap, size_t *len);
int ramfs_write(void *fs, const char *path, const char *src, size_t len);

/* OS connection: a single foreground shell owns this console. */
struct uart_device;
enum app_program { APP_PROGRAM_BASIC, APP_PROGRAM_EDITOR };
int apps_run_foreground(struct uart_device *console, enum app_program program);
int apps_shell_command(struct uart_device *console, const char *line);
/* Use in the shell instead of uart_try_getc to retain queued typeahead. */
int apps_console_try_getc(struct uart_device *console, char *out);
#endif
