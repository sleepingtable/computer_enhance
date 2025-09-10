global Read_x1
global Read_x2
global Read_x3
global Read_x4

section .text

;
; NOTE(casey): These ASM routines are written for the Windows
; 64-bit ABI. They expect the count in rcx and the data pointer in rdx.
;

Read_x1:
	align 64
.loop:
    mov rax, [rdi]
    sub rsi, 1
    jnle .loop
    ret

Read_x2:
	align 64
.loop:
    mov rax, [rdi]
    mov rax, [rdi]
    sub rsi, 2
    jnle .loop
    ret

Read_x3:
    align 64
.loop:
    mov rax, [rdi]
    mov rax, [rdi]
    mov rax, [rdi]
    sub rsi, 3
    jnle .loop
    ret

Read_x4:
	align 64
.loop:
    mov rax, [rdi]
    mov rax, [rdi]
    mov rax, [rdi]
    mov rax, [rdi]
    sub rsi, 4
    jnle .loop
    ret
