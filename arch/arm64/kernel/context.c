// arch/arm64/kernel/context.c

#include <arch/arm64/kernel/context.h>
#include <stddef.h>

void arch_context_init(
    struct arch_context *context,
    void *stack_top,
    void (*entry)(void))
{
    context->x19 = 0;
    context->x20 = 0;
    context->x21 = 0;
    context->x22 = 0;
    context->x23 = 0;
    context->x24 = 0;
    context->x25 = 0;
    context->x26 = 0;
    context->x27 = 0;
    context->x28 = 0;
    context->x29 = 0;

    /*
     * arch_context_switch()のretによって
     * 最初に実行されるアドレス。
     */
    context->x30 = (uint64_t)entry;

    /*
     * AArch64のSPは16バイト境界へ合わせる。
     */
    context->sp =
        ((uint64_t)stack_top) & ~0xFULL;
}

_Static_assert(
    offsetof(struct arch_context, x19) == 0x00,
    "invalid x19 offset");

_Static_assert(
    offsetof(struct arch_context, x30) == 0x58,
    "invalid x30 offset");

_Static_assert(
    offsetof(struct arch_context, sp) == 0x60,
    "invalid sp offset");

_Static_assert(
    sizeof(struct arch_context) == 0x68,
    "invalid arch_context size");