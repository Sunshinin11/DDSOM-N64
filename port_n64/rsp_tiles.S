##############################################################################
# Shadow over Mystara N64: tiles dos cenarios (e sprites) desenhados pelo RSP.
#
# A CPU so escreve uma lista compacta (16 bytes por tile, rdp_video.cpp) e manda um comando; aqui o RSP faz o que
# o Blit fazia na CPU: escolhe a posicao da paleta e da textura na TMEM (a usada ha mais tempo sai), gera os
# comandos de carga, de descritor e o retangulo, com as mesmas sincronizacoes que o autosync da libdragon poria.
#
# Entrada da lista (4 palavras):
#   w0: [31..24] grupo de paleta (0..0xBF)  [23..0] x1 (10.2, 12 bits) << 12 | y1 (10.2, 12 bits)   (canto inferior)
#   w1: [31..30] tamanho (0: 16x8, 1: 16x16, 2: 32x32)  [25] flipY  [24] flipX  [23..0] x0 << 12 | y0  (superior)
#   w2: s0 << 16 | t0 (10.5)
#   w3: endereco fisico do tile CI4 (RDRAM)
##############################################################################
#include <rsp_queue.inc>
#include <rdpq_macros.h>

    .data

    RSPQ_BeginOverlayHeader
        RSPQ_DefineCommand DDT_Cmd_Tiles, 12   # 0: a0 = quantas (24 bits), a1 = lista (RDRAM), a2 = paletas (RDRAM)
    RSPQ_EndOverlayHeader

    RSPQ_BeginSavedState
DDT_UNUSED:         .word 0
    RSPQ_EndSavedState

    .data
    .align 3
DDT_TLUT_BASE:      .word 0         # endereco das 0xC00 cores RGBA5551 (16 por grupo)
DDT_LEFT:           .word 0         # entradas ainda por trazer da RDRAM
DDT_LIST:           .word 0         # RDRAM: a proxima leva da lista
DDT_SAVE_S1:        .word 0
DDT_SAVE_S2:        .word 0
DDT_SAVE_RA:        .word 0
DDT_CLOCK:          .word 0         # relogio das paletas (TlutSlot)
DDT_TCLOCK:         .word 0         # relogio da TMEM (g_tmemTick)
DDT_USED:           .word 0         # autosync: bits 0..7 tiles, 8 TMEM, 9 pipe
DDT_LAST_SLOT:      .word 0
DDT_LAST_PAL:       .word 0
DDT_LAST_STRIDE:    .word 0
DDT_LAST_W:         .word 0
DDT_LAST_H:         .word 0
    # (estas tres tabelas seguidas: zeradas juntas no inicio de cada comando)
DDT_TMEM_TAG:       .fill 13, 4, 0  # endereco do tile em cada posicao da TMEM (12 de 128 bytes + a de 512)
DDT_TMEM_TICK:      .fill 13, 4, 0
DDT_TLUT_TICK:      .fill 16, 4, 0
DDT_TABLES_END:
    # (estas duas seguidas: 0xFF = nenhum)
DDT_SLOT_OF_GROUP:  .fill 192, 1, 0
DDT_GROUP_OF_SLOT:  .fill 16, 1, 0
DDT_MAPS_END:

    .bss
    .align 4
DDT_LIST_BUF:       .ds.b 512       # 32 entradas
DDT_OUT_BUF:        .ds.b 512       # comandos do RDP a enviar

    .text

    #############################################################
    # Emite um comando de 8 bytes (hi, lo) em s7
    #############################################################
    .macro emit8 hi, lo
    sw \hi, 0(s7)
    sw \lo, 4(s7)
    addiu s7, s7, 8
    .endm

    #############################################################
    # Sincronizacoes (como o autosync da libdragon): emitidas so se o recurso esta em uso. Usam t8, t9.
    #############################################################
    .func DDT_SyncPipe
DDT_SyncPipe:
    lw t8, %lo(DDT_USED)
    andi t9, t8, 0x200
    beqz t9, 1f
    xori t8, t8, 0x200
    sw t8, %lo(DDT_USED)
    lui t9, 0x2700
    emit8 t9, zero
1:  jr ra
    nop
    .endfunc

    # t9 = 1 << tile (o tile que vai mudar)
    .func DDT_SyncTile
DDT_SyncTile:
    lw t8, %lo(DDT_USED)
    and t9, t8, t9
    beqz t9, 1f
    andi t8, t8, 0x300          # SYNC_TILE libera todos os tiles
    sw t8, %lo(DDT_USED)
    lui t9, 0x2800
    emit8 t9, zero
1:  jr ra
    nop
    .endfunc

    .func DDT_SyncLoad
DDT_SyncLoad:
    lw t8, %lo(DDT_USED)
    andi t9, t8, 0x100
    beqz t9, 1f
    xori t8, t8, 0x100
    sw t8, %lo(DDT_USED)
    lui t9, 0x3100
    emit8 t9, zero
1:  jr ra
    nop
    .endfunc

    # Marca recursos em uso: t9 = bits
    .func DDT_Use
DDT_Use:
    lw t8, %lo(DDT_USED)
    or t8, t8, t9
    jr ra
    sw t8, %lo(DDT_USED)
    .endfunc

    #############################################################
    # Envia o que esta em DDT_OUT_BUF ao RDP (RDPQ_Send espera o DMA terminar).
    #############################################################
    .func DDT_Flush
DDT_Flush:
    sw ra, %lo(DDT_SAVE_RA)
    sw s1, %lo(DDT_SAVE_S1)
    sw s2, %lo(DDT_SAVE_S2)
    li s4, %lo(DDT_OUT_BUF)
    jal RDPQ_Send
    move s3, s7
    lw s1, %lo(DDT_SAVE_S1)
    lw s2, %lo(DDT_SAVE_S2)
    lw ra, %lo(DDT_SAVE_RA)
    jr ra
    li s7, %lo(DDT_OUT_BUF)
    .endfunc

    #############################################################
    # DDT_Cmd_Tiles (comando 0, 12 bytes)
    #############################################################
    .func DDT_Cmd_Tiles
DDT_Cmd_Tiles:
    sw a2, %lo(DDT_TLUT_BASE)
    sw a1, %lo(DDT_LIST)
    sll t0, a0, 8
    srl t0, t0, 8
    sw t0, %lo(DDT_LEFT)
    # o estado da TMEM e das paletas e desconhecido no inicio: nada conhecido, tudo "em uso" (sincroniza na 1a vez)
    li t1, %lo(DDT_TMEM_TAG)
    li t2, %lo(DDT_TABLES_END)
1:  sw zero, 0(t1)
    addiu t1, t1, 4
    bne t1, t2, 1b
    nop
    li t3, 0xFF
    li t1, %lo(DDT_SLOT_OF_GROUP)
    li t2, %lo(DDT_MAPS_END)
2:  sb t3, 0(t1)
    addiu t1, t1, 1
    bne t1, t2, 2b
    nop
    li t3, -1
    sw t3, %lo(DDT_LAST_SLOT)
    sw t3, %lo(DDT_LAST_PAL)
    sw t3, %lo(DDT_LAST_STRIDE)
    sw t3, %lo(DDT_LAST_W)
    sw t3, %lo(DDT_LAST_H)
    li t3, 0x3FF
    sw t3, %lo(DDT_USED)
    sw zero, %lo(DDT_CLOCK)
    sw zero, %lo(DDT_TCLOCK)
    li s7, %lo(DDT_OUT_BUF)

DDT_NextChunk:
    lw t0, %lo(DDT_LEFT)
    beqz t0, DDT_Done
    move s2, t0
    sltiu t1, t0, 33
    bnez t1, 3f
    nop
    li s2, 32
3:  sub t0, t0, s2
    sw t0, %lo(DDT_LEFT)
    lw s0, %lo(DDT_LIST)
    sll t0, s2, 4
    add t1, s0, t0
    sw t1, %lo(DDT_LIST)
    addiu t0, t0, -1
    jal DMAIn
    li s4, %lo(DDT_LIST_BUF)
    li s1, %lo(DDT_LIST_BUF)

DDT_NextEntry:
    beqz s2, DDT_NextChunk
    nop
    lw a0, 0(s1)
    lw a1, 4(s1)

    ##### paleta (TlutSlot) #####
    lw v0, %lo(DDT_CLOCK)
    addiu v0, v0, 1
    sw v0, %lo(DDT_CLOCK)
    srl t0, a0, 24                          # grupo
    lbu v1, %lo(DDT_SLOT_OF_GROUP)(t0)      # posicao (0..15) ou 0xFF
    li t2, 0xFF
    bne v1, t2, DDT_TlutHit
    nop
    # a posicao usada ha mais tempo (a primeira em caso de empate, como a CPU)
    li v1, 0
    lw t3, %lo(DDT_TLUT_TICK)
    li t4, 1
4:  sll t5, t4, 2
    lw t6, %lo(DDT_TLUT_TICK)(t5)
    sltu t8, t6, t3
    beqz t8, 5f
    nop
    move v1, t4
    move t3, t6
5:  addiu t4, t4, 1
    li t8, 16
    bne t4, t8, 4b
    nop
    lbu t5, %lo(DDT_GROUP_OF_SLOT)(v1)
    beq t5, t2, 6f
    nop
    sb t2, %lo(DDT_SLOT_OF_GROUP)(t5)
6:  sb t0, %lo(DDT_GROUP_OF_SLOT)(v1)
    sb v1, %lo(DDT_SLOT_OF_GROUP)(t0)
    # carga: SET_TEXTURE_IMAGE (RGBA16, 260 de largura) / SET_TILE (tile 7, I4, linha 33) / LOAD_TLUT (16 cores)
    jal DDT_SyncPipe
    nop
    lui t3, 0x3D10                          # 0x3D | (FMT_RGBA16 = 2) << 19
    ori t3, t3, 259
    lw t4, %lo(DDT_TLUT_BASE)
    sll t5, t0, 5                           # grupo * 32 bytes
    add t4, t4, t5
    emit8 t3, t4
    jal DDT_SyncTile
    li t9, 0x80
    lui t3, 0x3580                          # 0x35 | (FMT_I4 = 16) << 19
    ori t3, t3, (33 << 9) | 0x100           # linha 33, TMEM 0x800 / 8
    sll t5, v1, 4                           # + posicao * 128 / 8
    or t3, t3, t5
    lui t4, 0x0708                          # tile 7, ct (1 << 19)
    ori t4, t4, 0x0200                      # cs (1 << 9)
    emit8 t3, t4
    jal DDT_SyncLoad
    nop
    lui t3, 0x3000
    lui t4, 0x0703                          # tile 7, (0 + 16 - 1) << 14 = 0x3C000
    ori t4, t4, 0xC000
    emit8 t3, t4
    jal DDT_Use
    li t9, 0x80
DDT_TlutHit:
    sll t5, v1, 2
    sw v0, %lo(DDT_TLUT_TICK)(t5)
    move fp, v1                             # fp = posicao da paleta

    ##### textura na TMEM #####
    lw a3, 12(s1)                           # endereco do tile
    srl t6, a1, 30                          # tamanho
    lw v0, %lo(DDT_TCLOCK)
    addiu v0, v0, 1
    sw v0, %lo(DDT_TCLOCK)
    li t8, 2
    bne t6, t8, 7f
    nop
    b DDT_HaveSlot
    li v1, 12                               # 32x32: a posicao de 512 bytes
7:  lw v1, %lo(DDT_LAST_SLOT)
    bltz v1, 8f
    sltiu t8, v1, 12
    beqz t8, 8f
    sll t5, v1, 2
    lw t5, %lo(DDT_TMEM_TAG)(t5)
    beq t5, a3, DDT_HaveSlot                # o tile do ultimo retangulo de novo
    nop
8:  li v1, 0                                # procura nas 12
9:  sll t5, v1, 2
    lw t5, %lo(DDT_TMEM_TAG)(t5)
    beq t5, a3, DDT_HaveSlot
    nop
    addiu v1, v1, 1
    li t8, 12
    bne v1, t8, 9b
    nop
    # nao esta: a usada ha mais tempo (a primeira em caso de empate)
    li v1, 0
    lw t3, %lo(DDT_TMEM_TICK)
    li t4, 1
10: sll t5, t4, 2
    lw t5, %lo(DDT_TMEM_TICK)(t5)
    sltu t8, t5, t3
    beqz t8, 11f
    nop
    move v1, t4
    move t3, t5
11: addiu t4, t4, 1
    li t8, 12
    bne t4, t8, 10b
    nop
DDT_HaveSlot:
    sll t5, v1, 2
    sw v0, %lo(DDT_TMEM_TICK)(t5)
    lw t3, %lo(DDT_TMEM_TAG)(t5)
    beq t3, a3, DDT_Loaded
    nop
    sw a3, %lo(DDT_TMEM_TAG)(t5)
    # carga do tile: SET_TEXTURE_IMAGE (RGBA16, bytes/2 de largura) / SET_TILE (tile 1) / LOAD_BLOCK
    jal DDT_SyncPipe
    nop
    li t8, 2
    beq t6, t8, 12f
    li t3, 255                              # 32x32: 512 bytes -> 256 texels de 16 bits
    li t3, 63                               # 16x16: 128 bytes
    bnez t6, 12f
    nop
    li t3, 31                               # 16x8: 64 bytes
12: move s6, t3                             # texels - 1
    lui t4, 0x3D10
    or t4, t4, t3
    emit8 t4, a3
    jal DDT_SyncTile
    li t9, 0x02
    lui t3, 0x3510                          # 0x35 | RGBA16 << 19, linha 0
    sll t4, v1, 4                           # TMEM = posicao * 128 / 8
    or t3, t3, t4
    lui t4, 0x0108
    ori t4, t4, 0x0200                      # tile 1, ct, cs
    emit8 t3, t4
    jal DDT_SyncLoad
    nop
    lui t3, 0x3300
    sll t4, s6, 12
    lui t5, 0x0100
    or t4, t4, t5
    li t8, 2
    beq t6, t8, 13f
    ori t5, zero, 0x400                     # dxt: linha de 16 bytes (32x32) -> 2048 / 2
    ori t5, zero, 0x800                     # linha de 8 bytes -> 2048
13: or t4, t4, t5
    emit8 t3, t4
    jal DDT_Use
    li t9, 0x02
DDT_Loaded:

    ##### descritor do tile 0 #####
    li t3, 8                                # stride
    li t8, 2
    bne t6, t8, 14f
    nop
    li t3, 16
14: lw t4, %lo(DDT_LAST_SLOT)
    bne t4, v1, 15f
    lw t4, %lo(DDT_LAST_PAL)
    bne t4, fp, 15f
    lw t4, %lo(DDT_LAST_STRIDE)
    beq t4, t3, 16f
    nop
15: sw v1, %lo(DDT_LAST_SLOT)
    sw fp, %lo(DDT_LAST_PAL)
    sw t3, %lo(DDT_LAST_STRIDE)
    move s6, t3
    jal DDT_SyncTile
    li t9, 0x01
    lui t4, 0x3540                          # 0x35 | (FMT_CI4 = 8) << 19
    srl t5, s6, 3                           # linha = stride / 8
    sll t5, t5, 9
    or t4, t4, t5
    sll t5, v1, 4
    or t4, t4, t5
    sll t5, fp, 20                          # paleta
    lui t3, 0x0008
    ori t3, t3, 0x0200                      # ct, cs
    or t5, t5, t3
    emit8 t4, t5
16:
    ##### tamanho do tile 0 #####
    li t3, 16                               # largura
    li t4, 8                                # altura
    beqz t6, 17f
    nop
    li t4, 16
    li t8, 2
    bne t6, t8, 17f
    nop
    li t3, 32
    li t4, 32
17: lw t5, %lo(DDT_LAST_W)
    bne t5, t3, 18f
    lw t5, %lo(DDT_LAST_H)
    beq t5, t4, 19f
    nop
18: sw t3, %lo(DDT_LAST_W)
    sw t4, %lo(DDT_LAST_H)
    move s6, t3
    move s5, t4
    jal DDT_SyncTile
    li t9, 0x01
    lui t3, 0x3200
    sll t4, s6, 2
    addiu t4, t4, -4
    sll t4, t4, 12
    sll t5, s5, 2
    addiu t5, t5, -4
    or t4, t4, t5
    emit8 t3, t4
19:
    ##### retangulo #####
    lui t3, 0xE400                          # TEXTURE_RECTANGLE (como o rdpq o escreve), tile 0
    sll t4, a0, 8
    srl t4, t4, 8
    or t3, t3, t4                           # x1 | y1
    sll t4, a1, 8
    srl t4, t4, 8                           # tile 0 | x0 | y0
    lw t5, 8(s1)                            # s | t
    # dsdx / dtdy: +1,0 (0x0400) ou -1,0 (0xFC00) pelo espelhamento
    li t9, 0x0400                           # dtdy
    lui t8, 0x0200                          # bit 25: flipY
    and t8, a1, t8
    beqz t8, 20f
    nop
    li t9, 0xFC00
20: lui t8, 0x0100                          # bit 24: flipX
    and t8, a1, t8
    beqz t8, 22f
    lui t8, 0x0400                          # dsdx +1,0 (no delay slot: vale se nao espelha)
    lui t8, 0xFC00
22: or t9, t9, t8
21: sw t3, 0(s7)
    sw t4, 4(s7)
    sw t5, 8(s7)
    sw t9, 12(s7)
    addiu s7, s7, 16
    jal DDT_Use
    li t9, 0x301                            # pipe, TMEM, tile 0

    addiu s1, s1, 16
    addiu s2, s2, -1
    li t0, %lo(DDT_OUT_BUF) + 512 - 176     # cabe mais uma entrada completa?
    sltu t1, s7, t0
    bnez t1, DDT_NextEntry
    nop
    jal DDT_Flush
    nop
    j DDT_NextEntry
    nop

DDT_Done:
    jal DDT_Flush
    nop
    j RSPQ_Loop
    nop
    .endfunc

RDPQ_Triangle_Cull:
    jr ra
    nop

RDPQ_Triangle_Clip:
    jr ra
    nop

#include <rsp_rdpq.inc>
