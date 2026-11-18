; Windows x64 ABI:
; RCX = blocks, RDX = block count, R8 = query (left, bottom, right, top),
; R9 = output bytes. Each block stores four aligned arrays of four floats.

bits 64
section .text align=16
global gf_aabb_masks

gf_aabb_masks:
    test    rdx, rdx
    jz      masks_done

    movups  xmm0, [r8]
    movaps  xmm1, xmm0
    movaps  xmm2, xmm0
    movaps  xmm3, xmm0
    shufps  xmm0, xmm0, 00h        ; query.left
    shufps  xmm1, xmm1, 55h        ; query.bottom
    shufps  xmm2, xmm2, 0AAh       ; query.right
    shufps  xmm3, xmm3, 0FFh       ; query.top

masks_loop:
    movaps  xmm4, [rcx]
    cmpleps xmm4, xmm2            ; candidate.left <= query.right
    movaps  xmm5, [rcx + 16]
    cmpleps xmm5, xmm3            ; candidate.bottom <= query.top
    andps   xmm4, xmm5
    movaps  xmm5, xmm0
    cmpleps xmm5, [rcx + 32]
    andps   xmm4, xmm5
    movaps  xmm5, xmm1
    cmpleps xmm5, [rcx + 48]
    andps   xmm4, xmm5
    movmskps eax, xmm4
    mov     [r9], al

    add     rcx, 64
    inc     r9
    dec     rdx
    jnz     masks_loop
masks_done:
    ret
