// arch/arm64/boot/start.S
/*
    起動コアを1つに限定する。
    セカンダリコアは停止ループに入れる。
    BSSのクリアは start.S では行わず、main.c の clearbss() で行う。
    現在の Exception Level を確認する。
    EL2で起動していた場合はEL1へ遷移する。
    EL1で起動していた場合はそのまま続行する。
    EL3はこの最小実装では未対応とする。
    アラインメントチェックを必要に応じて無効化する。
    カーネル用スタックポインタを設定する。
    main 関数へ遷移する。
*/

#include <arch/arm64/armv8def.h>

    .section .reset,"ax",@progbits
    .global _start

_start:
    // Core 0 以外を停止する。
    mrs x0, mpidr_el1
    and x0, x0, #0xff
    cbz x0, primary_core

secondary_core:
    wfe
    b secondary_core

primary_core:
    // 現在の Exception Level を確認する。
    // CurrentEL[3:2] に現在ELが入っているので、2bit右シフトする。
    mrs x0, CurrentEL
    lsr x0, x0, #2

    cmp x0, #2
    b.eq CurrentEL_is_EL2

    cmp x0, #1
    b.eq EL1_start

    // EL3 または想定外のELは、この最小実装では停止する。
hang:
    wfe
    b hang

CurrentEL_is_EL2:
    // EL2 から EL1h へ遷移する。
    // HCR_EL2.RW = 1 にして、EL1をAArch64で動かす。
    mov x3, #(HCR_EL2_RW)
    msr HCR_EL2, x3
    isb

    // eret 後の復帰先を EL1_start にする。
    ldr x0, =EL1_start
    msr ELR_EL2, x0

    // EL1h, AArch64, 割り込みマスク状態でEL1へ降りる。
    mov x0, #(PSR_A_BIT | PSR_I_BIT | PSR_F_BIT | PSR_MODE64_BIT | PSR_MODE_EL1h)
    msr SPSR_EL2, x0

    eret

EL1_start:
    // EL1のスタックを設定する。
    ldr x4, =_stack_end
    mov sp, x4

    // 初期ブート中はアラインメントチェックを無効化する。
    mrs x0, sctlr_el1
    bic x0, x0, #(1 << 1)     // A: alignment check disable
    bic x0, x0, #(1 << 3)     // SA: SP alignment check disable
    bic x0, x0, #(1 << 4)     // SA0: EL0 SP alignment check disable
    msr sctlr_el1, x0
    isb

    // C言語のmain関数へ遷移する。
    // BSSクリアは main.c 側の clearbss() で行う。
    bl main

halt:
    wfe
    b halt