#include <usr/apps.h>

#define EXPR_DEPTH 16u
struct parser { const char *p; struct basic_context *ctx; const char *error; unsigned depth; };
static int64_t expression(struct parser *p);
static void fail(struct parser *p, const char *s) { if (!p->error) p->error = s; }
static int64_t checked(struct parser *p, int64_t v) {
    if (v < INT32_MIN || v > INT32_MAX) { fail(p, "Integer overflow"); return 0; } return v;
}
static int variable(struct parser *p) {
    char c; app_space(&p->p); c = *p->p;
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    if (c < 'A' || c > 'Z') { fail(p, "Expected variable A-Z"); return 0; }
    ++p->p; return c - 'A';
}
static int64_t primary(struct parser *p) {
    int64_t v = 0; uint32_t n; char ch; app_space(&p->p);
    if (p->error) return 0;
    if (++p->depth > EXPR_DEPTH) { --p->depth; fail(p, "Expression too deep"); return 0; }
    ch = *p->p;
    if (ch == '+' || ch == '-') {
        ++p->p; v = primary(p); if (ch == '-') v = -v; v = checked(p, v);
    } else if (ch == '(') {
        ++p->p; v = expression(p); app_space(&p->p);
        if (*p->p != ')') fail(p, "Expected )"); else ++p->p;
        v = checked(p, v);
    } else if (ch >= '0' && ch <= '9') {
        /* Permit magnitude 2147483648 only so unary minus can form INT32_MIN. */
        if (!app_uint(&p->p, &n, UINT32_C(2147483648))) fail(p, "Integer overflow");
        else v = n;
    } else if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')) {
        v = p->ctx->vars[variable(p)];
    } else fail(p, "Expected expression");
    --p->depth; return v;
}
static int64_t product(struct parser *p) {
    int64_t a = primary(p);
    while (!p->error) {
        char op; int64_t b; app_space(&p->p); op = *p->p;
        if (op != '*' && op != '/' && op != '%') break;
        ++p->p; a = checked(p, a); b = checked(p, primary(p));
        if (p->error) break;
        if ((op == '/' || op == '%') && b == 0) { fail(p, "Division by zero"); break; }
        if ((op == '/' || op == '%') && a == INT32_MIN && b == -1) {
            fail(p, "Integer overflow"); break;
        }
        a = checked(p, op == '*' ? a * b : op == '/' ? a / b : a % b);
    }
    return a;
}
static int64_t expression(struct parser *p) {
    int64_t a = product(p);
    while (!p->error) {
        char op; int64_t b; app_space(&p->p); op = *p->p;
        if (op != '+' && op != '-') break;
        ++p->p; a = checked(p, a); b = checked(p, product(p));
        if (p->error) break;
        a = checked(p, op == '+' ? a + b : a - b);
    }
    return a;
}
static int32_t value(struct parser *p) { return (int32_t)checked(p, expression(p)); }
static void end_statement(struct parser *p) { if (!app_end(p->p)) fail(p, "Unexpected text"); }
static int expect(struct parser *p, char c) {
    app_space(&p->p); if (*p->p != c) { fail(p, "Syntax error"); return 0; } ++p->p; return 1;
}
static size_t lookup(struct basic_program *program, uint32_t number) {
    size_t i; for (i = 0; i < program->count; ++i) if (program->lines[i].number == number) return i;
    return program->count;
}
static void copy_line(struct basic_line *dst, const struct basic_line *src) {
    dst->number = src->number; app_copy(dst->text, src->text);
}
int basic_store_line(struct basic_program *program, const char *text) {
    uint32_t number; size_t i, j; const char *p = text;
    if (app_strlen(text) >= APP_LINE_CAP) return -1;
    if (!app_uint(&p, &number, 65535) || !number) return -1;
    if (*p && *p != ' ' && *p != '\t') return -1;
    app_space(&p);
    if (app_strlen(p) >= APP_LINE_CAP) return -1;
    for (i = 0; i < program->count && program->lines[i].number < number; ++i) {}
    if (i < program->count && program->lines[i].number == number) {
        if (*p) app_copy(program->lines[i].text, p);
        else { for (j = i + 1; j < program->count; ++j) copy_line(&program->lines[j-1], &program->lines[j]); --program->count; }
        return 0;
    }
    if (!*p) return 0;
    if (program->count >= BASIC_MAX_LINES) return -1;
    for (j = program->count; j > i; --j) copy_line(&program->lines[j], &program->lines[j-1]);
    program->lines[i].number = number; app_copy(program->lines[i].text, p); ++program->count; return 0;
}
/* Executes exactly one statement. next is an index, count means END. */
static int statement(struct basic_context *ctx, const char *text, size_t *next, int running) {
    struct parser p = {text, ctx, NULL, 0}; struct app_io *io = ctx->io;
    const char *start; int32_t v; int idx;
    app_space(&p.p); start = p.p;
    if (!*p.p || app_keyword(&p.p, "REM") || *p.p == '\'') return 0;
    if (app_keyword(&p.p, "PRINT")) {
        int newline = 1;
        while (!p.error && !app_end(p.p)) {
            app_space(&p.p);
            if (*p.p == '"') {
                const char *begin = ++p.p;
                while (*p.p && *p.p != '"') ++p.p;
                if (*p.p != '"') { fail(&p, "Unterminated string"); break; }
                while (begin < p.p) io->putc(io->user, *begin++);
                ++p.p;
            } else { v = value(&p); if (!p.error) app_number(io, v); }
            app_space(&p.p); newline = 1;
            if (*p.p == ';' || *p.p == ',') {
                if (*p.p == ',') app_puts(io, " ");
                ++p.p; newline = 0;
            } else if (!app_end(p.p)) fail(&p, "Expected ; or ,");
        }
        if (newline) app_puts(io, "\n");
    } else if (app_keyword(&p.p, "INPUT")) {
        idx = variable(&p); end_statement(&p);
        if (!p.error) {
            char input[APP_LINE_CAP]; int rc;
            for (;;) {
                struct parser q; app_puts(io, "? "); rc = app_readline(io, input, sizeof(input));
                if (rc == APP_READ_EOF || rc == APP_READ_CANCEL) return 2;
                if (rc == APP_READ_LONG) { app_error(io, "Input too long"); continue; }
                q = (struct parser){input, ctx, NULL, 0}; v = value(&q); end_statement(&q);
                if (q.error) { app_error(io, q.error); continue; }
                ctx->vars[idx] = v; break;
            }
        }
    } else if (app_keyword(&p.p, "IF")) {
        int32_t a = value(&p), b; int op = 0, condition = 0;
        app_space(&p.p);
        if (*p.p == '=') { op = 1; ++p.p; }
        else if (*p.p == '<') { ++p.p; op = 2; if (*p.p == '=') { ++p.p; op = 3; } else if (*p.p == '>') { ++p.p; op = 4; } }
        else if (*p.p == '>') { ++p.p; op = 5; if (*p.p == '=') { ++p.p; op = 6; } }
        else fail(&p, "Expected comparison");
        b = value(&p);
        if (!app_keyword(&p.p, "THEN")) fail(&p, "Expected THEN line-number");
        v = value(&p); end_statement(&p);
        if (!running) fail(&p, "IF requires RUN");
        if (v < 1 || v > 65535) fail(&p, "Invalid line number");
        if (!p.error) {
            switch (op) { case 1: condition=a==b; break; case 2: condition=a<b; break;
                case 3: condition=a<=b; break; case 4: condition=a!=b; break;
                case 5: condition=a>b; break; case 6: condition=a>=b; break; default: break; }
            if (condition) { size_t target=lookup(&ctx->program,(uint32_t)v);
                if (target == ctx->program.count) fail(&p,"No such line"); else *next=target; }
        }
    } else if (app_keyword(&p.p, "GOTO") || (p.p = start, app_keyword(&p.p, "GOSUB"))) {
        const char *q = start; int sub = app_keyword(&q, "GOSUB"); size_t target;
        v = value(&p); end_statement(&p);
        if (!running) fail(&p, "GOTO/GOSUB requires RUN");
        if (v < 1 || v > 65535) fail(&p, "Invalid line number");
        target = lookup(&ctx->program, (uint32_t)v);
        if (target == ctx->program.count) fail(&p, "No such line");
        if (sub && ctx->return_count == BASIC_RETURN_DEPTH) fail(&p, "GOSUB stack full");
        if (!p.error) { if (sub) ctx->returns[ctx->return_count++] = *next; *next = target; }
    } else if (app_keyword(&p.p, "RETURN")) {
        end_statement(&p);
        if (!running || !ctx->return_count) fail(&p, "RETURN without GOSUB");
        if (!p.error) *next = ctx->returns[--ctx->return_count];
    } else if (app_keyword(&p.p, "END") || app_keyword(&p.p, "STOP")) {
        end_statement(&p); if (!p.error) *next = ctx->program.count;
    } else {
        p.p = start; (void)app_keyword(&p.p, "LET"); idx = variable(&p);
        (void)expect(&p, '='); v = value(&p); end_statement(&p);
        if (!p.error) ctx->vars[idx] = v;
    }
    if (p.error) { app_error(io, p.error); return 1; } return 0;
}
int basic_run(struct basic_context *ctx) {
    size_t pc = 0, i; struct app_io *io = ctx->io;
    if (!app_io_valid(io)) return 1;
    for (i = 0; i < 26; ++i) ctx->vars[i] = 0;
    ctx->return_count = 0;
    while (pc < ctx->program.count) {
        size_t next = pc + 1; int rc;
        if (io->cancelled && io->cancelled(io->user)) { app_puts(io, "Break\n"); return 2; }
        rc = statement(ctx, ctx->program.lines[pc].text, &next, 1);
        if (rc) { app_puts(io, rc == 2 ? "Break at " : "At line ");
            app_number(io, (int32_t)ctx->program.lines[pc].number); app_puts(io,"\n"); return rc; }
        pc = next; io->yield(io->user);
    }
    return 0;
}
static void list(struct basic_context *ctx) {
    size_t i; for (i=0;i<ctx->program.count;++i) {
        app_number(ctx->io,(int32_t)ctx->program.lines[i].number); app_puts(ctx->io," ");
        app_puts(ctx->io,ctx->program.lines[i].text); app_puts(ctx->io,"\n");
    }
}
static int load(struct basic_context *ctx, const char *path) {
    char absolute[APP_PATH_CAP];
    if (app_resolve_path(ctx->io, path, absolute) != 0) return -1;
    path = absolute;
    size_t n, pos = 0; struct app_io *io=ctx->io;
    if (!io->read_file || io->read_file(io->fs,path,ctx->file_data,sizeof(ctx->file_data),&n) || n>sizeof(ctx->file_data)) return -1;
    ctx->staging.count=0;
    while (pos<n) {
        char line[APP_LINE_CAP]; size_t len=0;
        while (pos<n && ctx->file_data[pos]!='\n' && ctx->file_data[pos]!='\r') {
            unsigned char c=(unsigned char)ctx->file_data[pos++];
            if (len+1>=sizeof(line) || (c<32 && c!='\t') || c>126) return -1;
            line[len++]=(char)c;
        }
        line[len]=0;
        if (pos<n && ctx->file_data[pos++]=='\r' && pos<n && ctx->file_data[pos]=='\n') ++pos;
        if (!app_end(line) && basic_store_line(&ctx->staging,line)) return -1;
    }
    for (pos=0;pos<ctx->staging.count;++pos) copy_line(&ctx->program.lines[pos],&ctx->staging.lines[pos]);
    ctx->program.count=ctx->staging.count; ctx->dirty=0; app_copy(ctx->path,path); return 0;
}
static int save(struct basic_context *ctx, const char *path) {
    char absolute[APP_PATH_CAP];
    if (app_resolve_path(ctx->io, path, absolute) != 0) return -1;
    path = absolute;
    size_t i, j, n=0; struct app_io *io=ctx->io;
    if (!io->write_file) return -1;
    for (i=0;i<ctx->program.count;++i) {
        char number[5]; size_t k=0; uint32_t v=ctx->program.lines[i].number;
        do { number[k++]=(char)('0'+v%10u); v/=10u; } while(v);
        if (n+k+1+app_strlen(ctx->program.lines[i].text)+1>sizeof(ctx->file_data)) return -1;
        while(k) ctx->file_data[n++]=number[--k];
        ctx->file_data[n++]=' ';
        for(j=0;ctx->program.lines[i].text[j];++j) ctx->file_data[n++]=ctx->program.lines[i].text[j];
        ctx->file_data[n++]='\n';
    }
    if(io->write_file(io->fs,path,ctx->file_data,n)) return -1;
    ctx->dirty=0; app_copy(ctx->path,path); return 0;
}
static int force_or_clean(struct basic_context *ctx, const char *p) {
    app_space(&p);
    if (*p=='!' && app_end(p+1)) return 1;
    if (!app_end(p)) { app_error(ctx->io,"Unexpected text"); return 0; }
    if (ctx->dirty) { app_error(ctx->io,"Unsaved program; SAVE or use !"); return 0; }
    return 1;
}
void basic_task(void *argument) {
    struct basic_context *ctx=argument; struct app_io *io;
    if(!ctx || !app_io_valid(ctx->io)) return;
    io=ctx->io; app_puts(io,"Tiny BASIC / integer A-Z / HELP for commands\n");
    for (;;) {
        const char *p; int rc; size_t next=0; char path[APP_PATH_CAP];
        app_puts(io,"BASIC> "); rc=app_readline(io,ctx->command,sizeof(ctx->command));
        if(rc==APP_READ_EOF) break;
        if(rc==APP_READ_CANCEL) continue;
        if(rc==APP_READ_LONG) { app_error(io,"Line too long"); continue; }
        p=ctx->command; app_space(&p); if(!*p) continue;
        if(*p>='0' && *p<='9') {
            if(basic_store_line(&ctx->program,p)) app_error(io,"Invalid line or program full"); else ctx->dirty=1;
        } else if(app_keyword(&p,"RUN")) {
            if(!app_end(p)) app_error(io,"RUN takes no arguments"); else (void)basic_run(ctx);
        } else if(app_keyword(&p,"LIST")) {
            if(!app_end(p)) app_error(io,"LIST takes no arguments"); else list(ctx);
        } else if(app_keyword(&p,"NEW")) {
            if(force_or_clean(ctx,p)) { ctx->program.count=0; ctx->path[0]=0; ctx->dirty=0; }
        } else if(app_keyword(&p,"QUIT") || app_keyword(&p,"EXIT")) {
            if(force_or_clean(ctx,p)) break;
        } else if(app_keyword(&p,"LOAD")) {
            int force=0; app_space(&p); if(*p=='!') { force=1; ++p; }
            if(!app_path(p,path)) app_error(io,"LOAD[!] filename");
            else if(ctx->dirty && !force) app_error(io,"Unsaved program; SAVE or LOAD! filename");
            else if(load(ctx,path)) app_error(io,"Load failed (missing, oversized or invalid source)");
            else app_puts(io,"Loaded\n");
        } else if(app_keyword(&p,"SAVE")) {
            if(app_end(p) && ctx->path[0]) app_copy(path,ctx->path);
            else if(!app_path(p,path)) { app_error(io,"SAVE filename"); continue; }
            if(save(ctx,path)) app_error(io,"Save failed / storage unavailable"); else app_puts(io,"Saved\n");
        } else if(app_keyword(&p,"HELP")) {
            app_puts(io,"10 PRINT \"HELLO\"  |  10 alone deletes line\n"
                "RUN LIST NEW[!] LOAD[!] file SAVE [file] QUIT[!]\n"
                "LET A=expr, A=expr, PRINT \"text\";expr, INPUT A\n"
                "IF expr <|<=|=|<>|>=|> expr THEN line\n"
                "GOTO line, GOSUB line, RETURN, REM, END, STOP\n"
                "Operators: + - * / % ( ); signed 32-bit; one statement/line\n");
        } else (void)statement(ctx,ctx->command,&next,0);
    }
    if(io->finished) io->finished(io->user);
}
