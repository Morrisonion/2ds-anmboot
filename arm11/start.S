.syntax unified
.arm
.section .text.start,"ax",%progbits
.align 4
.global _start
.extern __bss_start__
.extern __bss_end__
.extern arm11_main
_start:
    cpsid aif
    clrex
    mov r0, #0
    mcr p15, 0, r0, c7, c10, 0
    mcr p15, 0, r0, c7, c7, 0
    mcr p15, 0, r0, c7, c10, 4
    mcr p15, 0, r0, c7, c10, 5
    mcr p15, 0, r0, c7, c5, 4
    ldr r0, =0x54078
    mcr p15, 0, r0, c1, c0, 0
    nop
    nop
    ldr sp, =0x1FFBE000
    ldr r0, =__bss_start__
    ldr r1, =__bss_end__
    mov r2, #0
1:  cmp r0, r1
    strlo r2, [r0], #4
    blo 1b
    bl arm11_main
2:  wfi
    b 2b
.pool

/* Neutral ARM11 wait stub for the normal Luma/B9S path. It does not touch the
   Luma operation mailbox; it only waits for ARM9 to publish Luma's own ARM11
   entrypoint at 0x1FFFFFFC, then jumps to the unmodified helper. */
.section .normalstub,"ax",%progbits
.align 4
.global arm11_normal_stub_start
.global arm11_normal_stub_end
arm11_normal_stub_start:
    cpsid aif
    clrex
    mov r0, #0
    mcr p15, 0, r0, c7, c10, 0
    mcr p15, 0, r0, c7, c7, 0
    mcr p15, 0, r0, c7, c10, 4
    mcr p15, 0, r0, c7, c10, 5
    mcr p15, 0, r0, c7, c5, 4
    ldr r0, =0x54078
    mcr p15, 0, r0, c1, c0, 0
    nop
    nop
    ldr r0, =0x1FFFFFFC
3:
    ldr r1, [r0]
    cmp r1, #0
    beq 3b
    bx r1
.pool
arm11_normal_stub_end:

/* Semantic equivalent of Luma's own prepareForFirmlaunch stub. Called after
   Scene C as stub(ARM11_READY, &operation): release stock Luma ARM9, then wait
   for NATIVE_FIRM's ARM11 entrypoint. */
.section .firmstub,"ax",%progbits
.align 4
.global arm11_firmstub_start
.global arm11_firmstub_end
arm11_firmstub_start:
    /* Split handoff experiment.  Release stock Luma immediately.  Luma then
       copies NATIVE_FIRM, writes its ARM11 entry to 0x1FFFFFFC and jumps to
       the NATIVE ARM9 entry. Keep this tiny stub for ~0.5 seconds so ARM9 can
       run ahead while the full logo remains on-screen. Then perform the
       official 15-step Scene-C alpha curve using the real LCD backlight duty
       registers. Each duty update is issued in the middle of the PDC VBlank
       window (0x192..0x196), not on its leading edge, to avoid a left/right
       brightness split on a frame.  At alpha=0, clear both retained framebuffer sets while the
       backlight is dark, restore the exact original duty values, and only then
       enter NATIVE ARM11.  Luma itself is never modified. */
    str r0, [r1]
    mov r0, #0x20000000
4:
    ldr r8, [r0, #-4]
    cmp r8, #0
    beq 4b

    /* ARM9 head-start: 30 VBlanks ~= 0.5 seconds. */
    ldr r4, =0x10400454      /* PDC0 VCount */
    ldr r5, =0x192           /* VBlank start */
    mov r6, #30
5:
6:
    ldr r7, [r4]
    lsl r7, r7, #20
    lsr r7, r7, #20
    cmp r7, r5
    bhs 6b
7:
    ldr r7, [r4]
    lsl r7, r7, #20
    lsr r7, r7, #20
    cmp r7, r5
    blo 7b
    subs r6, r6, #1
    bne 5b

    /* Preserve the exact current top/bottom backlight duty. */
    ldr r4, =0x10202240
    ldr r5, =0x10202A40
    ldr r9, [r4]
    ldr r10, [r5]
    adr r11, 12f
    mov r6, #0

    /* Scene C alpha: 252,243,228,210,189,165,140,115,90,66,45,27,12,3,0. */
8:
    /* Wait for a fresh VBlank, then move two scanlines into the asserted
       0x192..0x196 window. Updating at 0x192 itself occasionally raced the last
       active pixels and produced a one-frame left/right brightness split. */
    ldr r0, =0x10400454
    ldr r1, =0x194
9:
    ldr r3, [r0]
    lsl r3, r3, #20
    lsr r3, r3, #20
    cmp r3, r1
    bhs 9b
10:
    ldr r3, [r0]
    lsl r3, r3, #20
    lsr r3, r3, #20
    cmp r3, r1
    blo 10b

    ldrb r2, [r11, r6]
    mul r0, r9, r2
    add r0, r0, #128
    lsr r0, r0, #8
    str r0, [r4]
    mul r0, r10, r2
    add r0, r0, #128
    lsr r0, r0, #8
    str r0, [r5]

    add r6, r6, #1
    cmp r6, #15
    blo 8b

    /* Backlight is now at zero. Clear framebuffer A then B invisibly. */
    ldr r0, =0x10400010      /* PSC0 */
    ldr r1, =0x03060000      /* 0x18300000 >> 3 */
    ldr r2, =0x0306FD20      /* (0x18300000 + 0x7E900) >> 3 */
    str r1, [r0, #0]
    str r2, [r0, #4]
    mov r3, #0
    str r3, [r0, #8]
    ldr r3, =0x201
    str r3, [r0, #12]
11:
    ldr r3, [r0, #12]
    tst r3, #1
    bne 11b

    ldr r1, =0x03080000      /* 0x18400000 >> 3 */
    ldr r2, =0x0308FD20      /* (0x18400000 + 0x7E900) >> 3 */
    str r1, [r0, #0]
    str r2, [r0, #4]
    mov r3, #0
    str r3, [r0, #8]
    ldr r3, =0x201
    str r3, [r0, #12]
13:
    ldr r3, [r0, #12]
    tst r3, #1
    bne 13b

    /* Restore only the top backlight here.  Restoring the bottom duty while
       its retained framebuffer is already black produced a visible grey/bright
       flash on real hardware.  Keep the bottom at zero and let NATIVE/HOME turn
       it back on only when the system display stack has valid lower-screen
       content ready. */
    str r9, [r4]
    mov r0, #0
    str r0, [r5]
    bx r8

12:
    .byte 252,243,228,210,189,165,140,115,90,66,45,27,12,3,0
    .align 2
.pool
arm11_firmstub_end:
