#include <usr/apps.h>

/* ANSI/VT100 terminal viewport. Keep within the host terminal's actual size. */
#ifndef EDIT_SCREEN_COLS
#define EDIT_SCREEN_COLS 80u
#endif
#ifndef EDIT_SCREEN_ROWS
#define EDIT_SCREEN_ROWS 24u
#endif

#if EDIT_SCREEN_COLS < 40 || EDIT_SCREEN_ROWS < 8
#error "Editor viewport must be at least 40 columns by 8 rows"
#endif

#define EDIT_BODY_ROWS (EDIT_SCREEN_ROWS - 4u)
#define EDIT_GUTTER 5u
#define EDIT_TEXT_COLS (EDIT_SCREEN_COLS - EDIT_GUTTER - 1u)
#define EDIT_TAB_WIDTH 4u

enum {
    KEY_EOF = -1,
    KEY_UP = 256, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
    KEY_HOME, KEY_END, KEY_DELETE, KEY_PGUP, KEY_PGDN
};
enum { DRAW_CURSOR, DRAW_LINE, DRAW_ALL };

struct screen_editor {
    struct editor_context *ctx;
    struct app_io *io;
    size_t row, col, top, left, wanted;
    const char *notice;
    char clipboard[APP_LINE_CAP];
    unsigned have_clipboard;
};

static void move_to(struct app_io *io, size_t row, size_t col)
{
    app_puts(io, "\033[");
    app_number(io, (int32_t)row);
    app_puts(io, ";");
    app_number(io, (int32_t)col);
    app_puts(io, "H");
}

static void limited(struct app_io *io, const char *s, size_t limit)
{
    while (*s && limit--) {
        unsigned char c = (unsigned char)*s++;
        io->putc(io->user, c >= 32 && c <= 126 ? (char)c : ' ');
    }
}

static size_t visual_col(const char *s, size_t col)
{
    size_t i, v = 0;
    for (i = 0; i < col && s[i]; ++i)
        v += s[i] == '\t' ? EDIT_TAB_WIDTH - v % EDIT_TAB_WIDTH : 1u;
    return v;
}

static size_t byte_col(const char *s, size_t wanted)
{
    size_t i = 0, v = 0;
    while (s[i]) {
        size_t next = v + (s[i] == '\t' ? EDIT_TAB_WIDTH - v % EDIT_TAB_WIDTH : 1u);
        if (next > wanted) break;
        v = next;
        ++i;
    }
    return i;
}

#if defined(__aarch64__)
/* Kernel tasks run at EL1. Use elapsed time, not a count of task_yield calls. */
static uint64_t key_ticks(void)
{
    uint64_t value;
    __asm__ volatile("isb; mrs %0, cntpct_el0" : "=r"(value));
    return value;
}

static uint64_t key_timeout_ticks(void)
{
    uint64_t frequency;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(frequency));
    return frequency / 4u ? frequency / 4u : 1u;
}
#endif

/* Read a key without app_readline(), which deliberately ignores arrow keys. */
static int read_key(struct screen_editor *e)
{
    unsigned state = 0, parameter = 0, modifier = 0, length = 0;
#if defined(__aarch64__)
    uint64_t started = 0, timeout = key_timeout_ticks();
#else
    unsigned waits = 0;
#endif
    for (;;) {
        char raw;
        unsigned char c;
        int rc;
        if (e->io->cancelled && e->io->cancelled(e->io->user)) return 3;
        rc = e->io->try_getc(e->io->user, &raw);
        if (rc < 0) return KEY_EOF;
        if (!rc) {
            e->io->yield(e->io->user);
            /* A lone Escape must not wait forever for a CSI sequence. */
            #if defined(__aarch64__)
            if (state && key_ticks() - started >= timeout) return 27;
#else
            if (state && ++waits >= 10000u) return 27;
#endif
            continue;
        }
#if defined(__aarch64__)
        started = key_ticks();
#else
        waits = 0;
#endif
        c = (unsigned char)raw;
        if (e->io->skip_lf) {
            e->io->skip_lf = 0;
            if (c == '\n') continue;
        }
        if (c == 3) return 3;
        if (!state) {
            if (c == 27) { state = 1; continue; }
            if (c == '\r') { e->io->skip_lf = 1; return '\n'; }
            return c;
        }
        if (state == 1) {
            if (c == '[' || c == 'O') { state = 2; continue; }
            return 27;
        }
        if (++length > 24u) return 27;
        if (c >= '0' && c <= '9') {
            if (!modifier && parameter < 1000u) parameter = parameter * 10u + (c - '0');
            continue;
        }
        if (c == ';') { modifier = 1; continue; }
        if (c < 0x40 || c > 0x7e) continue;
        switch (c) {
        case 'A': return KEY_UP;
        case 'B': return KEY_DOWN;
        case 'C': return KEY_RIGHT;
        case 'D': return KEY_LEFT;
        case 'H': return KEY_HOME;
        case 'F': return KEY_END;
        case '~':
            if (parameter == 1 || parameter == 7) return KEY_HOME;
            if (parameter == 4 || parameter == 8) return KEY_END;
            if (parameter == 3) return KEY_DELETE;
            if (parameter == 5) return KEY_PGUP;
            if (parameter == 6) return KEY_PGDN;
            break;
        default: break;
        }
        return 27;
    }
}

static void draw_line(struct screen_editor *e, size_t screen_row)
{
    size_t index = e->top + screen_row, i, v = 0;
    struct editor_buffer *b = &e->ctx->buffer;
    move_to(e->io, screen_row + 2u, 1);
    app_puts(e->io, "\033[0m\033[2K");
    if (index >= b->count) return;
    app_puts(e->io, "\033[90m");
    if (index + 1 < 1000) app_puts(e->io, " ");
    if (index + 1 < 100) app_puts(e->io, " ");
    if (index + 1 < 10) app_puts(e->io, " ");
    app_number(e->io, (int32_t)(index + 1));
    app_puts(e->io, " \033[0m");
    for (i = 0; b->lines[index][i]; ++i) {
        char c = b->lines[index][i];
        size_t width = c == '\t' ? EDIT_TAB_WIDTH - v % EDIT_TAB_WIDTH : 1u;
        while (width--) {
            if (v >= e->left && v < e->left + EDIT_TEXT_COLS)
                e->io->putc(e->io->user, c == '\t' ? ' ' : c);
            ++v;
        }
        if (v >= e->left + EDIT_TEXT_COLS) break;
    }
}

static void render(struct screen_editor *e, int mode)
{
    size_t i, v = visual_col(e->ctx->buffer.lines[e->row], e->col);
    size_t old_top = e->top, old_left = e->left;
    if (e->row < e->top) e->top = e->row;
    if (e->row >= e->top + EDIT_BODY_ROWS) e->top = e->row - EDIT_BODY_ROWS + 1u;
    if (v < e->left) e->left = v;
    if (v >= e->left + EDIT_TEXT_COLS) e->left = v - EDIT_TEXT_COLS + 1u;
    if (e->top != old_top || e->left != old_left) mode = DRAW_ALL;
    app_puts(e->io, "\033[?25l");
    if (mode != DRAW_CURSOR) {
        move_to(e->io, 1, 1);
        app_puts(e->io, "\033[0m\033[2K\033[7m EDIT ");
        app_puts(e->io, e->ctx->dirty ? "[+] " : "    ");
        limited(e->io, e->ctx->path[0] ? e->ctx->path : "[untitled]", EDIT_SCREEN_COLS - 12u);
        app_puts(e->io, "\033[0m");
    }
    if (mode == DRAW_ALL) {
        for (i = 0; i < EDIT_BODY_ROWS; ++i) draw_line(e, i);
        move_to(e->io, EDIT_SCREEN_ROWS - 1u, 1);
        app_puts(e->io, "\033[2K\033[7m");
        limited(e->io, "^S Save  ^O Save as  ^R Open  ^X Exit", EDIT_SCREEN_COLS - 1u);
        app_puts(e->io, "\033[0m");
        move_to(e->io, EDIT_SCREEN_ROWS, 1);
        app_puts(e->io, "\033[2K\033[7m");
        limited(e->io, "^K Cut line  ^U Paste line  ^G Help  ^L Redraw", EDIT_SCREEN_COLS - 1u);
        app_puts(e->io, "\033[0m");
    } else if (mode == DRAW_LINE) draw_line(e, e->row - e->top);
    move_to(e->io, EDIT_SCREEN_ROWS - 2u, 1);
    app_puts(e->io, "\033[0m\033[2K");
    if (e->notice) limited(e->io, e->notice, EDIT_SCREEN_COLS - 1u);
    else {
        app_puts(e->io, "Ln "); app_number(e->io, (int32_t)(e->row + 1));
        app_puts(e->io, "  Col "); app_number(e->io, (int32_t)(v + 1));
        app_puts(e->io, "  Lines "); app_number(e->io, (int32_t)e->ctx->buffer.count);
    }
    move_to(e->io, e->row - e->top + 2u, EDIT_GUTTER + v - e->left + 1u);
    app_puts(e->io, "\033[?25h");
}

/* Editable filename prompt. 1=accepted, 0=cancelled, -1=console closed. */
static int prompt(struct screen_editor *e, const char *label, char *text, size_t cap)
{
    size_t n = app_strlen(text), cursor = n, left = 0;
    size_t prefix = app_strlen(label), width = EDIT_SCREEN_COLS - prefix - 1u;
    for (;;) {
        int key;
        size_t i;
        if (cursor < left) left = cursor;
        if (cursor >= left + width) left = cursor - width + 1u;
        move_to(e->io, EDIT_SCREEN_ROWS - 2u, 1);
        app_puts(e->io, "\033[0m\033[2K");
        app_puts(e->io, label);
        limited(e->io, text + left, width);
        move_to(e->io, EDIT_SCREEN_ROWS - 2u, prefix + cursor - left + 1u);
        key = read_key(e);
        if (key == KEY_EOF) return -1;
        if (key == 27 || key == 3 || key == 24) return 0;
        if (key == '\n') return 1;
        if (key == KEY_LEFT) { if (cursor) --cursor; }
        else if (key == KEY_RIGHT) { if (cursor < n) ++cursor; }
        else if (key == KEY_HOME || key == 1) cursor = 0;
        else if (key == KEY_END || key == 5) cursor = n;
        else if (key == 8 || key == 127) {
            if (cursor) {
                for (i = cursor; i <= n; ++i) text[i - 1] = text[i];
                --cursor; --n;
            }
        } else if (key == KEY_DELETE || key == 4) {
            if (cursor < n) {
                for (i = cursor + 1; i <= n; ++i) text[i - 1] = text[i];
                --n;
            }
        } else if (key >= 32 && key <= 126 && n + 1 < cap) {
            for (i = n + 1; i > cursor; --i) text[i] = text[i - 1];
            text[cursor++] = (char)key; ++n;
        }
    }
}

static int save_file(struct screen_editor *e, int choose_name)
{
    struct editor_context *ctx = e->ctx;
    char path[APP_PATH_CAP], absolute[APP_PATH_CAP];
    size_t i, j, n = 0;
    app_copy(path, ctx->path);
    if (choose_name || !path[0]) {
        if (prompt(e, "Save as: ", path, sizeof(path)) != 1) {
            e->notice = "Save cancelled"; return 0;
        }
    }
    if (!path[0]) { e->notice = "Filename is empty"; return 0; }
    if (app_resolve_path(e->io, path, absolute) != 0) {
        e->notice = "Invalid path or storage unavailable"; return 0;
    }
    app_copy(path, absolute);
    if (!e->io->write_file) { e->notice = "Storage is read-only or unavailable"; return 0; }
    for (i = 0; i < ctx->buffer.count; ++i) {
        size_t len = app_strlen(ctx->buffer.lines[i]);
        if (n + len + (i != 0) > sizeof(ctx->file_data)) {
            e->notice = "File too large"; return 0;
        }
        if (i) ctx->file_data[n++] = '\n';
        for (j = 0; j < len; ++j) ctx->file_data[n++] = ctx->buffer.lines[i][j];
    }
    if (e->io->write_file(e->io->fs, path, ctx->file_data, n)) {
        e->notice = "Save failed; buffer retained"; return 0;
    }
    app_copy(ctx->path, path);
    ctx->dirty = 0;
    e->notice = "Saved";
    return 1;
}

static int allow_leave(struct screen_editor *e)
{
    if (!e->ctx->dirty) return 1;
    for (;;) {
        int key;
        e->notice = "Save changes? Y=yes  N=no  Esc=cancel";
        render(e, DRAW_CURSOR);
        key = read_key(e);
        if (key == 'y' || key == 'Y') return save_file(e, 0);
        if (key == 'n' || key == 'N') return 1;
        if (key == 27 || key == 3 || key == 24 || key == KEY_EOF) {
            e->notice = "Cancelled"; return 0;
        }
    }
}

static int load_file(struct screen_editor *e, const char *path)
{
    struct editor_context *ctx = e->ctx;
    size_t n, pos = 0, row = 0, col = 0, i;
    char absolute[APP_PATH_CAP];
    if (app_resolve_path(e->io, path, absolute) != 0) {
        e->notice = "Invalid path or storage unavailable"; return 0;
    }
    path = absolute;
    if (!e->io->read_file ||
        e->io->read_file(e->io->fs, path, ctx->file_data, sizeof(ctx->file_data), &n) ||
        n > sizeof(ctx->file_data)) {
        e->notice = "Open failed; current buffer retained"; return 0;
    }
    ctx->staging.count = 1;
    ctx->staging.lines[0][0] = 0;
    while (pos < n) {
        unsigned char c = (unsigned char)ctx->file_data[pos++];
        if (c == '\r' || c == '\n') {
            if (c == '\r' && pos < n && ctx->file_data[pos] == '\n') ++pos;
            if (row + 1 >= EDITOR_MAX_LINES) {
                e->notice = "Too many lines; current buffer retained"; return 0;
            }
            ++row; col = 0;
            ctx->staging.lines[row][0] = 0;
            ctx->staging.count = row + 1;
        } else {
            if ((c < 32 && c != '\t') || c > 126 || col + 1 >= APP_LINE_CAP) {
                e->notice = "Non-ASCII or long line; current buffer retained"; return 0;
            }
            ctx->staging.lines[row][col++] = (char)c;
            ctx->staging.lines[row][col] = 0;
        }
    }
    for (i = 0; i < ctx->staging.count; ++i) app_copy(ctx->buffer.lines[i], ctx->staging.lines[i]);
    ctx->buffer.count = ctx->staging.count;
    app_copy(ctx->path, path); ctx->dirty = 0;
    e->row = e->col = e->top = e->left = e->wanted = 0;
    e->notice = "Opened";
    return 1;
}

static int insert_char(struct screen_editor *e, char c)
{
    char *s = e->ctx->buffer.lines[e->row];
    size_t n = app_strlen(s), i;
    if (n + 1 >= APP_LINE_CAP) { e->notice = "Line full (191 bytes max)"; return DRAW_CURSOR; }
    for (i = n + 1; i > e->col; --i) s[i] = s[i - 1];
    s[e->col++] = c;
    e->ctx->dirty = 1;
    return DRAW_LINE;
}

static int split_line(struct screen_editor *e)
{
    struct editor_buffer *b = &e->ctx->buffer;
    size_t i;
    if (b->count >= EDITOR_MAX_LINES) { e->notice = "Buffer full (128 lines max)"; return DRAW_CURSOR; }
    for (i = b->count; i > e->row + 1; --i) app_copy(b->lines[i], b->lines[i - 1]);
    app_copy(b->lines[e->row + 1], b->lines[e->row] + e->col);
    b->lines[e->row][e->col] = 0;
    ++b->count; ++e->row; e->col = 0;
    e->ctx->dirty = 1;
    return DRAW_ALL;
}

static int join_next(struct screen_editor *e)
{
    struct editor_buffer *b = &e->ctx->buffer;
    size_t n = app_strlen(b->lines[e->row]), i;
    if (e->row + 1 >= b->count) return DRAW_CURSOR;
    if (n + app_strlen(b->lines[e->row + 1]) >= APP_LINE_CAP) {
        e->notice = "Cannot join: line would exceed 191 bytes"; return DRAW_CURSOR;
    }
    app_copy(b->lines[e->row] + n, b->lines[e->row + 1]);
    for (i = e->row + 2; i < b->count; ++i) app_copy(b->lines[i - 1], b->lines[i]);
    --b->count; e->ctx->dirty = 1;
    return DRAW_ALL;
}

static int erase_char(struct screen_editor *e, int backward)
{
    struct editor_buffer *b = &e->ctx->buffer;
    size_t i, n = app_strlen(b->lines[e->row]);
    if (backward) {
        if (e->col) --e->col;
        else if (e->row) {
            size_t previous = app_strlen(b->lines[e->row - 1]);
            if (previous + n >= APP_LINE_CAP) {
                e->notice = "Cannot join: line would exceed 191 bytes"; return DRAW_CURSOR;
            }
            --e->row; e->col = previous;
            return join_next(e);
        } else return DRAW_CURSOR;
    } else if (e->col == n) return join_next(e);
    for (i = e->col + 1; i <= n; ++i) b->lines[e->row][i - 1] = b->lines[e->row][i];
    e->ctx->dirty = 1;
    return DRAW_LINE;
}

static int clipboard_line(struct screen_editor *e, int paste)
{
    struct editor_buffer *b = &e->ctx->buffer;
    size_t i;
    if (paste) {
        if (!e->have_clipboard) { e->notice = "Clipboard empty"; return DRAW_CURSOR; }
        if (b->count >= EDITOR_MAX_LINES) { e->notice = "Buffer full"; return DRAW_CURSOR; }
        for (i = b->count; i > e->row; --i) app_copy(b->lines[i], b->lines[i - 1]);
        app_copy(b->lines[e->row], e->clipboard); ++b->count;
    } else {
        app_copy(e->clipboard, b->lines[e->row]); e->have_clipboard = 1;
        for (i = e->row + 1; i < b->count; ++i) app_copy(b->lines[i - 1], b->lines[i]);
        if (b->count > 1) --b->count;
        else b->lines[0][0] = 0;
        if (e->row >= b->count) e->row = b->count - 1;
    }
    e->col = 0; e->ctx->dirty = 1;
    return DRAW_ALL;
}

void editor_task(void *argument)
{
    struct editor_context *ctx = argument;
    struct screen_editor e;
    int mode = DRAW_ALL;
    if (!ctx || !app_io_valid(ctx->io)) return;
    e.ctx = ctx; e.io = ctx->io;
    e.row = e.col = e.top = e.left = e.wanted = 0;
    e.clipboard[0] = 0; e.have_clipboard = 0; e.notice = NULL;
    if (!ctx->buffer.count) { ctx->buffer.count = 1; ctx->buffer.lines[0][0] = 0; }
    ctx->io->escape_state = 0;
    app_puts(e.io, "\033[?1049h\033[0m\033[2J\033[H");
    e.notice = "Arrow keys move the cursor. Ctrl+S saves. Ctrl+X exits.";
    for (;;) {
        int key, vertical = 0;
        size_t n;
        render(&e, mode);
        key = read_key(&e);
        if (key == KEY_EOF) break;
        mode = DRAW_CURSOR;
        e.notice = NULL;
        n = app_strlen(ctx->buffer.lines[e.row]);
        if (key == 24) { if (allow_leave(&e)) break; mode = DRAW_ALL; }
        else if (key == 19 || key == 15) { (void)save_file(&e, key == 15); mode = DRAW_ALL; }
        else if (key == 18) {
            char path[APP_PATH_CAP];
            path[0] = 0;
            if (prompt(&e, "Open: ", path, sizeof(path)) == 1 && path[0]) {
                if (allow_leave(&e)) (void)load_file(&e, path);
            } else e.notice = "Open cancelled";
            mode = DRAW_ALL;
        } else if (key == 7) {
            e.notice = "Arrows/Home/End/PgUp/PgDn move. Enter splits. Backspace joins.";
        } else if (key == 12) { app_puts(e.io, "\033[2J"); mode = DRAW_ALL; }
        else if (key == 3 || key == 27) { e.notice = "Cancelled"; mode = DRAW_ALL; }
        else if (key == KEY_LEFT) {
            if (e.col) --e.col;
            else if (e.row) { --e.row; e.col = app_strlen(ctx->buffer.lines[e.row]); }
        } else if (key == KEY_RIGHT) {
            if (e.col < n) ++e.col;
            else if (e.row + 1 < ctx->buffer.count) { ++e.row; e.col = 0; }
        } else if (key == KEY_HOME || key == 1) e.col = 0;
        else if (key == KEY_END || key == 5) e.col = n;
        else if (key == KEY_UP || key == KEY_DOWN || key == KEY_PGUP || key == KEY_PGDN) {
            size_t step = key == KEY_PGUP || key == KEY_PGDN ? EDIT_BODY_ROWS : 1u;
            if (key == KEY_UP || key == KEY_PGUP) e.row = e.row > step ? e.row - step : 0;
            else { e.row += step; if (e.row >= ctx->buffer.count) e.row = ctx->buffer.count - 1; }
            e.col = byte_col(ctx->buffer.lines[e.row], e.wanted);
            vertical = 1;
        } else if (key == '\n') mode = split_line(&e);
        else if (key == 8 || key == 127) mode = erase_char(&e, 1);
        else if (key == KEY_DELETE || key == 4) mode = erase_char(&e, 0);
        else if (key == 11 || key == 21) mode = clipboard_line(&e, key == 21);
        else if (key == '\t' || (key >= 32 && key <= 126)) mode = insert_char(&e, (char)key);
        if (!vertical) e.wanted = visual_col(ctx->buffer.lines[e.row], e.col);
    }
    app_puts(e.io, "\033[0m\033[?25h\033[?1049l");
    if (e.io->finished) e.io->finished(e.io->user);
}
