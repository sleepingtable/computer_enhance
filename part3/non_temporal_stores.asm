global read_write_temporal
global read_write_non_temporal

section .text


read_write_temporal:
    ; rdi -> first param, base addr
    ; rsi -> second param, inner size
    ; rdx -> third param, outer count
    ; rcx -> 4th param, dest addr
    mov r11, rdi
    add r11, rsi

	align 64
.outer_loop:
    mov r9, rdi

	align 64
.loop:
    vmovdqa ymm0, [r9]
    vmovdqa [rcx], ymm0
    vmovdqa ymm1, [r9 + 32]
    vmovdqa [rcx + 32], ymm1
    vmovdqa ymm2, [r9 + 64]
    vmovdqa [rcx + 64], ymm2
    vmovdqa ymm3, [r9 + 96]
    vmovdqa [rcx + 96], ymm3
    vmovdqa ymm4, [r9 + 128]
    vmovdqa [rcx + 128], ymm4
    vmovdqa ymm5, [r9 + 160]
    vmovdqa [rcx + 160], ymm5
    vmovdqa ymm6, [r9 + 192]
    vmovdqa [rcx + 192], ymm6
    vmovdqa ymm7, [r9 + 224]
    vmovdqa [rcx + 224], ymm7

    add r9, 256
    add rcx, 256
    cmp r9, r11
    jb .loop

    dec rdx
    jnz .outer_loop
    ret


read_write_non_temporal:
    ; rdi -> first param, base addr
    ; rsi -> second param, inner size
    ; rdx -> third param, outer count
    ; rcx -> 4th param, dest addr
    mov r11, rdi
    add r11, rsi

	align 64
.outer_loop:
    mov r9, rdi

	align 64
.loop:
    vmovdqa ymm0, [r9]
    vmovntdq [rcx], ymm0
    vmovdqa ymm1, [r9 + 32]
    vmovntdq [rcx + 32], ymm1
    vmovdqa ymm2, [r9 + 64]
    vmovntdq [rcx + 64], ymm2
    vmovdqa ymm3, [r9 + 96]
    vmovntdq [rcx + 96], ymm3
    vmovdqa ymm4, [r9 + 128]
    vmovntdq [rcx + 128], ymm4
    vmovdqa ymm5, [r9 + 160]
    vmovntdq [rcx + 160], ymm5
    vmovdqa ymm6, [r9 + 192]
    vmovntdq [rcx + 192], ymm6
    vmovdqa ymm7, [r9 + 224]
    vmovntdq [rcx + 224], ymm7

    add r9, 256
    add rcx, 256
    cmp r9, r11
    jb .loop

    dec rdx
    jnz .outer_loop
    ret
