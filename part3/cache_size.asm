global Read_mem
global Read_mem2
global Read_mem_32_aligned
global Read_mem_32_unaligned
global Read_mem_cache_set

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


Read_mem_32_aligned:
    ; rdi -> first param, base addr
    ; rsi -> second param, size
    ; rdx -> third param, mem mask
    xor rax, rax
    mov r9, rdi
	align 64
.loop:
    vmovdqu ymm0, [r9]
    vmovdqu ymm0, [r9 + 32]
    vmovdqu ymm0, [r9 + 64]
    vmovdqu ymm0, [r9 + 96]
    vmovdqu ymm0, [r9 + 128]
    vmovdqu ymm0, [r9 + 160]
    vmovdqu ymm0, [r9 + 192]
    vmovdqu ymm0, [r9 + 224]

    mov r9, rax
    and r9, rdx
    add r9, rdi

    add rax, 256
    cmp rax, rsi
    jb .loop
    ret


Read_mem_32_unaligned:
    ; rdi -> first param, base addr
    ; rsi -> second param, size
    ; rdx -> third param, mem mask
    xor rax, rax
    mov r9, rdi
	align 64
.loop:
    vmovdqu ymm0, [r9]
    vmovdqu ymm0, [r9 + 33]
    vmovdqu ymm0, [r9 + 66]
    vmovdqu ymm0, [r9 + 99]
    vmovdqu ymm0, [r9 + 132]
    vmovdqu ymm0, [r9 + 165]
    vmovdqu ymm0, [r9 + 198]
    vmovdqu ymm0, [r9 + 231]

    mov r9, rax
    and r9, rdx
    add r9, rdi

    add rax, 256
    cmp rax, rsi
    jb .loop


Read_mem_cache_set:
    ; rdi -> first param, base addr
    ; rsi -> second param, outer count
    ; rdx -> third param, N-way count
    ; rcx -> fourth param, inner count
	align 64
.outer_loop:
    mov r10, rdx ; N-way count
    mov r8, rdi

    ; will loop between 1 and 32 times on bits above the bottom 6 (so add 4096 each time)
    ; this effectively is a test for N-way (between 1-way and 32-way)
	align 64
.associative_loop:

    mov r11, rcx ; inner count
    mov r9, r8

    ; will read between 2 cache lines (1 loop) and 64 cache lines (32 loops) -- inside the bottom 6 bits used for lookup
	align 64
.way_inner_lookup_loop:
    vmovdqu ymm0, [r9]
    vmovdqu ymm0, [r9 + 32]
    vmovdqu ymm0, [r9 + 64]
    vmovdqu ymm0, [r9 + 96]

    add r9, 128  ; two more cache lines
    dec r11
    jnz .way_inner_lookup_loop

    add r8, 4096  ; move to the next 6 + 6 bits
    dec r10
    jnz .associative_loop

    dec rsi
    jnz .outer_loop
    ret
