    .text
    .globl _gate_stack_probe
    .p2align 2
_gate_stack_probe:
    stp x29, x30, [sp, #-48]!
    stp x19, x20, [sp, #16]
    stp x21, x22, [sp, #32]
    mov x29, sp
    mov x20, sp
    mov x22, x0
    mov x0, sp
    bl _dm_set_gate_stack
    mov sp, x22
    mov x19, #0x1919
    bl _dm_gate
    cmp x0, #0x42
    cset w0, eq
    mov x9, #0x1919
    cmp x19, x9
    cset w9, eq
    and w0, w0, w9
    mov x9, #0x2121
    cmp x21, x9
    cset w9, eq
    and w0, w0, w9
    mov sp, x20
    ldp x21, x22, [sp, #32]
    ldp x19, x20, [sp, #16]
    ldp x29, x30, [sp], #48
    ret
