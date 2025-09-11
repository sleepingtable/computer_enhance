global Read_mem
global Read_mem2

section .text


Read_mem:
    ; rdi -> first param, base addr
    ; rsi -> second param, size
    ; rdx -> third param, mem mask
    xor rax, rax
    mov r9, rdi
	align 64
.loop:
    vmovdqu8 zmm0, [r9]
    vmovdqu8 zmm0, [r9 + 64]
    vmovdqu8 zmm0, [r9 + 128]
    vmovdqu8 zmm0, [r9 + 192]
    vmovdqu8 zmm0, [r9 + 256]
    vmovdqu8 zmm0, [r9 + 320]
    vmovdqu8 zmm0, [r9 + 384]
    vmovdqu8 zmm0, [r9 + 448]

    mov r9, rax
    and r9, rdx
    add r9, rdi

    add rax, 512
    cmp rax, rsi
    jb .loop
    ret


Read_mem2:
    ; rdi -> first param, base addr
    ; rsi -> second param, outer count
    ; rdx -> third param, inner size
    xor rax, rax

	align 64
.outer_loop:
    mov r9, rdi
    xor rcx, rcx

    align 64
.inner_loop:
    vmovdqu8 zmm0, [r9]
    vmovdqu8 zmm0, [r9 + 64]
    vmovdqu8 zmm0, [r9 + 128]
    vmovdqu8 zmm0, [r9 + 192]

    add r9, 256
    add rcx, 256
    cmp rcx, rdx
    jb .inner_loop

    inc rax
    cmp rax, rsi
    jb .outer_loop
    ret
