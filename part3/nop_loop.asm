;  ========================================================================
;
;  (C) Copyright 2023 by Molly Rocket, Inc., All Rights Reserved.
;
;  This software is provided 'as-is', without any express or implied
;  warranty. In no event will the authors be held liable for any damages
;  arising from the use of this software.
;
;  Please see https://computerenhance.com for more information
;
;  ========================================================================

;  ========================================================================
;  LISTING 132
;  ========================================================================

global MOVAllBytesASM
global NOPAllBytesASM
global CMPAllBytesASM
global DECAllBytesASM
global nop_1x3_ASM
global nop_3x1_ASM
global nop_9x1_ASM

section .text

;
; NOTE(casey): These ASM routines are written for the Windows
; 64-bit ABI. They expect RCX to be the first parameter (the count),
; and in the case of MOVAllBytesASM, RDX to be the second
; parameter (the data pointer). To use these on a platform
; with a different ABI, you would have to change those registers
; to match the ABI.
;

MOVAllBytesASM:
    xor rax, rax
.loop:
    mov [rsi + rax], al
    inc rax
    cmp rax, rdi
    jb .loop
    ret

NOPAllBytesASM:
    xor rax, rax
.loop:
    db 0x0f, 0x1f, 0x00 ; NOTE(casey): This is the byte sequence for a 3-byte NOP
    inc rax
    cmp rax, rdi
    jb .loop
    ret

CMPAllBytesASM:
    xor rax, rax
.loop:
    inc rax
    cmp rax, rdi
    jb .loop
    ret

DECAllBytesASM:
.loop:
    dec rdi
    jnz .loop
    ret

nop_1x3_ASM:
    xor rax, rax
.loop:
    db 0x0f, 0x1f, 0x00
    inc rax
    cmp rax, rdi
    jb .loop
    ret

nop_3x1_ASM:
    xor rax, rax
.loop:
    nop
    nop
    nop
    inc rax
    cmp rax, rdi
    jb .loop
    ret

nop_9x1_ASM:
    xor rax, rax
.loop:
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    inc rax
    cmp rax, rdi
    jb .loop
    ret
