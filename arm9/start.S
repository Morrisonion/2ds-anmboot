.syntax unified
.arm
.section .text.start,"ax",%progbits
.align 4
.global _start
.type _start,%function
.extern __bss_start__
.extern __bss_end__
.extern arm9_main
_start:
    /* Match the known-good BAX bare-metal entry conditions closely. */
    msr cpsr_cxsf, #0xD3

    mov r0,#0
    mcr p15,0,r0,c7,c5,0
    mcr p15,0,r0,c7,c6,0
    mcr p15,0,r0,c7,c10,4

    /* MPU/caches/TCMs off, high vectors selected. */
    ldr r0,=0x2078
    mcr p15,0,r0,c1,c0,0
    nop
    nop

    /* Same ITCM/DTCM mapping BAX establishes. The launch trampoline lives in ITCM. */
    ldr r0,=0x3000000A
    ldr r1,=0x00000024
    mcr p15,0,r0,c9,c1,0
    mcr p15,0,r1,c9,c1,1
    mrc p15,0,r0,c1,c0,0
    orr r0,r0,#0x50000
    mcr p15,0,r0,c1,c0,0
    nop
    nop

    ldr sp,=0x080FF000
    ldr r0,=__bss_start__
    ldr r1,=__bss_end__
    mov r2,#0
1:
    cmp r0,r1
    strlo r2,[r0],#4
    blo 1b

    /* BAX resets every NDMA channel before touching SD. B9S itself used DMA/SD
       just before entering us, so preserving that state is unsafe. */
    ldr r0,=0x10002000
    mov r1,#0
    str r1,[r0,#28]
    str r1,[r0,#56]
    str r1,[r0,#84]
    str r1,[r0,#112]
    str r1,[r0,#140]
    str r1,[r0,#168]
    str r1,[r0,#196]
    str r1,[r0,#224]
    mov r1,#1
    str r1,[r0]

    /* BAX's SDMC state fix. */
    ldr r0,=0x10000000
    ldrh r1,[r0,#0x20]
    bic r1,r1,#0x001
    orr r1,r1,#0x200
    strh r1,[r0,#0x20]

    bl arm9_main
2:
    mov r0,#0
    mcr p15,0,r0,c7,c0,4
    b 2b
.pool
