global Write_x1
global Write_x2
global Write_x3
global Write_x4

section .text

;
; NOTE(casey): These ASM routines are written for the Windows
; 64-bit ABI. They expect the count in rcx and the data pointer in rdx.
;

Write_x1:
	align 64
.loop:
    mov [rdi], rax
    sub rsi, 1
    jnle .loop
    ret

Write_x2:
	align 64
.loop:
    mov [rdi], rax
    mov [rdi], rax
    sub rsi, 2
    jnle .loop
    ret

Write_x3:
    align 64
.loop:
    mov [rdi], rax
    mov [rdi], rax
    mov [rdi], rax
    sub rsi, 3
    jnle .loop
    ret

Write_x4:
	align 64
.loop:
    mov [rdi], rax
    mov [rdi], rax
    mov [rdi], rax
    mov [rdi], rax
    sub rsi, 4
    jnle .loop
    ret
