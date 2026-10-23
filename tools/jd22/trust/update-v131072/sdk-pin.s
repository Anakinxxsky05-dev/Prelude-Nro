.text
ldar x8, [x0]
cbz x8, original
ldrb w9, [x8, #16]
tbz w9, #0, original
ldr x9, [x8, #24]
cmp x9, #64
b.ne original
ldr x8, [x8, #32]
cbz x8, original
adr x9, pins
mov w10, #3
next_pin:
mov x11, #0
compare_byte:
ldrb w12, [x8, x11]
ldrb w13, [x9, x11]
cmp w12, w13
b.ne mismatch
add x11, x11, #1
cmp x11, #64
b.ne compare_byte
mov w0, wzr
ret
mismatch:
add x9, x9, #64
subs w10, w10, #1
b.ne next_pin
original: .inst 0xd503201f
.balign 4, 0
pins:
.ascii "89dee2bda0a92403835983ec792ce408c72fc3a350a3c675a33446a0363700f8"
.ascii "40b40a72f695e284fafc5409673c616e498552069ee2b8b46f5a67e7c0b93eab"
.ascii "a16c840dbd72eb67a3e121be9335ffb16d70f995b83d5b44ab8e52990338608f"
