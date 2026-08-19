.syntax unified
.arm
.align 4

/* ------------------------------------------------------------------------- */
/* Normal Luma path: B9S-compatible handoff. Used whenever any key is held at
   power-on, so Luma itself owns SELECT/START/payload/config behavior exactly. */
.section .launchstub.normal,"ax",%progbits
.global arm9_normal_stub_start
.global arm9_normal_stub_end
.type arm9_normal_stub_start,%function

.equ NORMAL_MAGIC, 0x0003BEEF
.equ N_ARGV_LOC,   0x1FFFFCC0
.equ N_PATH_LOC,   0x1FFFFCC8
.equ N_FBPTR_LOC,  0x1FFFFDC8
.equ MPCORE_ENTRY, 0x1FFFFFFC

arm9_normal_stub_start:
    /* Framebuffer argv, same convention used by BAX/B9S chainloaders. */
    ldr r2, =N_FBPTR_LOC
    ldr r3, =0x18300000
    ldr r4, =0x18300000
    ldr r5, =0x18346500
    stmia r2!, {r3-r5}
    ldr r3, =0x18400000
    ldr r4, =0x18400000
    ldr r5, =0x18446500
    stmia r2!, {r3-r5}

    /* r0 = target FIRM, r1 = ASCII path. Save path before self-overwrite. */
    ldr r2, =N_PATH_LOC
    add r3, r1, #0x100
0:
    ldrb r4, [r1], #1
    cmp r4, #0
    cmpne r1, r3
    strbne r4, [r2], #1
    bne 0b
    mov r4, #0
    strb r4, [r2]

    mov r11, r0
    add r10, r11, #0x40
    mov r9, #4
1:
    ldmia r10, {r0-r2}
    cmp r2, #0
    addne r0, r0, r11
    ldrne r3, =0xFFFF0374
    blxne r3
    subs r9, r9, #1
    addne r10, r10, #0x30
    bne 1b

    /* Match BAX target-FIRM CPU cleanup state. */
    msr cpsr_c, #0xD3
    ldr r3, =0xFFFF0830
    ldr r4, =0xFFFF0AB4
    ldr r5, =0xFFFF0C58
    blx r3
    blx r4
    blx r5

    /* The screen-init flag controls whether the framebuffer struct is passed. */
    ldr r6, =N_ARGV_LOC
    ldr r3, =N_PATH_LOC
    str r3, [r6, #0]
    ldrb r7, [r11, #0x10]
    tst r7, #1
    moveq r0, #1
    movne r0, #2
    mov r1, r6
    ldr r2, =NORMAL_MAGIC
    cmp r0, #2
    ldreq r3, =N_FBPTR_LOC
    streq r3, [r6, #4]

    ldr r3, [r11, #0x08]   /* target ARM11 entry */
    ldr r4, [r11, #0x0C]   /* target ARM9 entry */
    ldr r12, =MPCORE_ENTRY
    str r3, [r12]
    bx r4
.pool
arm9_normal_stub_end:

/* ------------------------------------------------------------------------- */
/* Animated path: exact target Luma image, but entered through Luma's own
   FIRMLAUNCH ABI. This makes stock Luma skip its final deinitScreens(), which
   lets our ARM11 animation cover the pre-NATIVE black interval. */
.section .launchstub.animated,"ax",%progbits
.global arm9_animated_stub_start
.global arm9_animated_stub_end
.type arm9_animated_stub_start,%function

.equ FIRMLAUNCH_MAGIC, 0x0000BABE
.equ A_ARGV_LOC,       0x1FFFFCC0
.equ A_PATH16_LOC,     0x1FFFFD00
.equ A_LOWTID_LOC,     0x1FFFFE00
.equ LUMA_OPERATION,   0x1FF80004
.equ SHARED,           0x1FFFFB00
.equ LOAD_RUNNING,     6
.equ ARM11_READY,      8

arm9_animated_stub_start:
    /* argv[0] = UTF-16 target path. */
    ldr r2, =A_PATH16_LOC
    mov r3, #0
2:
    ldrb r4, [r1], #1
    strh r4, [r2], #2
    add r3, r3, #1
    cmp r4, #0
    beq 3f
    cmp r3, #80
    blo 2b
    mov r4, #0
    strh r4, [r2]
3:
    /* argv[1] = UTF-16 low TID "00000002" => NATIVE_FIRM. */
    ldr r2, =A_LOWTID_LOC
    ldr r3, =0x00300030
    str r3, [r2, #0]
    str r3, [r2, #4]
    str r3, [r2, #8]
    ldr r3, =0x00320030
    str r3, [r2, #12]
    mov r3, #0
    strh r3, [r2, #16]

    ldr r2, =A_ARGV_LOC
    ldr r3, =A_PATH16_LOC
    str r3, [r2, #0]
    ldr r3, =A_LOWTID_LOC
    str r3, [r2, #4]

    /* Copy target FIRM sections unchanged. */
    mov r11, r0
    add r10, r11, #0x40
    mov r9, #4
4:
    ldmia r10, {r0-r2}
    cmp r2, #0
    addne r0, r0, r11
    ldrne r3, =0xFFFF0374
    blxne r3
    subs r9, r9, #1
    addne r10, r10, #0x30
    bne 4b

    /* Our ARM11 renderer temporarily stands in for Luma's helper. */
    ldr r0, =LUMA_OPERATION
    mov r1, #ARM11_READY
    str r1, [r0]
    ldr r0, =MPCORE_ENTRY
    mov r1, #0
    str r1, [r0]
    ldr r0, =SHARED
    mov r1, #LOAD_RUNNING
    str r1, [r0, #4]

    msr cpsr_c, #0xD3
    ldr r3, =0xFFFF0830
    ldr r4, =0xFFFF0AB4
    ldr r5, =0xFFFF0C58
    blx r3
    blx r4
    blx r5

    mov r0, #2
    ldr r1, =A_ARGV_LOC
    ldr r2, =FIRMLAUNCH_MAGIC
    ldr r4, [r11, #0x0C]
    bx r4
.pool
arm9_animated_stub_end:
