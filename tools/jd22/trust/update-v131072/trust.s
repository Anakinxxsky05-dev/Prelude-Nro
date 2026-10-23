.text
stp x29, x30, [sp, #-96]!
stp x19, x20, [sp, #16]
stp x21, x22, [sp, #32]
stp x0, x1, [sp, #48]
ldr x8, [x0, #0x488]
ldr x19, [x8, #0x1e0]
cbnz x19, have_store
ldr x8, [x0, #0x598]
ldr x19, [x8, #0x20]
have_store:
cbz x19, verify_original
mov w21, wzr
adr x8, anchor_0
str x8, [sp, #64]
mov x0, xzr
add x1, sp, #64
mov x2, #831
.inst 0xd503201f
cbz x0, next_0
mov x20, x0
mov x0, x19
mov x1, x20
.inst 0xd503201f
cmp w0, #1
cset w22, eq
orr w21, w21, w22
mov x0, x20
.inst 0xd503201f
next_0:
adr x8, anchor_1
str x8, [sp, #64]
mov x0, xzr
add x1, sp, #64
mov x2, #794
.inst 0xd503201f
cbz x0, next_1
mov x20, x0
mov x0, x19
mov x1, x20
.inst 0xd503201f
cmp w0, #1
cset w22, eq
orr w21, w21, w22
mov x0, x20
.inst 0xd503201f
next_1:
cbz w21, verify_original
ldr x8, [sp, #48]
ldr x0, [x8, #0xd0]
cbz x0, verify_original
mov x1, #0x80000
.inst 0xd503201f
verify_original:
ldp x0, x1, [sp, #48]
.inst 0xd503201f
ldp x21, x22, [sp, #32]
ldp x19, x20, [sp, #16]
ldp x29, x30, [sp], #96
ret
.balign 8, 0
anchor_0:
.incbin "anchor-0.der"
.balign 8, 0
anchor_1:
.incbin "anchor-1.der"
