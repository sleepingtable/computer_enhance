
global Read_4x2
global Read_8x2
global Read_16x2
global Read_32x2
global Read_64x2

section .text

;
; NOTE(casey): These ASM routines are written for the Windows
; 64-bit ABI. They expect RCX to be the first parameter (the count),
; and in the case of MOVAllBytesASM, RDX to be the second
; parameter (the data pointer). To use these on a platform
; with a different ABI, you would have to change those registers
; to match the ABI.
;

Read_4x2:
    xor rax, rax
	align 64
.loop:
    mov r8d, [rdi ]
    mov r8d, [rdi + 4]
    add rax, 8
    cmp rax, rsi
    jb .loop
    ret

Read_8x2:
    xor rax, rax
	align 64
.loop:
    mov r8, [rdi ]
    mov r8, [rdi + 8]
    add rax, 16
    cmp rax, rsi
    jb .loop
    ret

Read_16x2:
    xor rax, rax
	align 64
.loop:
    vmovdqu xmm0, [rdi]
    vmovdqu xmm0, [rdi + 16]
    add rax, 32
    cmp rax, rsi
    jb .loop
    ret

Read_32x2:
    xor rax, rax
	align 64
.loop:
    vmovdqu ymm0, [rdi]
    vmovdqu ymm0, [rdi + 32]
    add rax, 64
    cmp rax, rsi
    jb .loop
    ret

Read_64x2:
    xor rax, rax
	align 64
.loop:
    vmovdqu8 zmm0, [rdi]
    vmovdqu8 zmm0, [rdi + 64]
    add rax, 128
    cmp rax, rsi
    jb .loop
    ret
