// SPDX-License-Identifier: GPL-3.0-only
// Reescrita nativa da rotina de montagem de sprites 0x01E062 do DDSOM.
// Percorre a definicao do sprite do objeto e emite as pecas na memoria de objetos (0x708000).

#include "lift_sprites.h"
#include "board.h"
#include "cart_n64.h"
#include "lift.h"
#include "native.h"

#include <cstdlib>

namespace cps2recomp {

namespace {

// Leitura de 16 bits da RAM de trabalho
static inline uint16_t RamWord(const uint8_t *p) {
#if defined(CPS2RECOMP_N64) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    typedef uint16_t Alias16 __attribute__((may_alias, aligned(1)));
    return *reinterpret_cast<const Alias16*>(p);
#else
    return static_cast<uint16_t>((p[0] << 8) | p[1]);
#endif
}

// Escrita de 16 bits na RAM de trabalho
static inline void SetRamWord(uint8_t *p, uint16_t v) {
#if defined(CPS2RECOMP_N64) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    typedef uint16_t Alias16 __attribute__((may_alias, aligned(1)));
    *reinterpret_cast<Alias16*>(p) = v;
#else
    p[0] = static_cast<uint8_t>(v >> 8);
    p[1] = static_cast<uint8_t>(v);
#endif
}

// Leitura de 32 bits da RAM de trabalho
static inline uint32_t RamLong(const uint8_t *p) {
#if defined(CPS2RECOMP_N64) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    typedef uint32_t Alias32 __attribute__((may_alias, aligned(1)));
    return *reinterpret_cast<const Alias32*>(p);
#else
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
#endif
}

// Leitura de 16 bits da ROM (com cache no N64, ou RAM de trabalho caso o endereco seja alto)
static inline uint16_t Rom16(M68K &c, uint32_t endereco) {
    endereco &= 0xFFFFFE;
    if (__builtin_expect(endereco < 0xFF0000, 1)) {
#ifdef CPS2RECOMP_N64
        return static_cast<uint16_t>(RomWord(g_romData, endereco));
#else
        if (endereco + 1 < c.romSize) {
            const uint8_t *p = &c.rom[endereco];
            return static_cast<uint16_t>((CartByte(p) << 8) | CartByte(p + 1));
        }
        return static_cast<uint16_t>(c.bus.Read16(endereco));
#endif
    }
    return RamWord(&c.ram[endereco & 0xFFFF]);
}

// Leitura de 32 bits da ROM (com cache no N64, ou RAM de trabalho caso o endereco seja alto)
static inline uint32_t Rom32(M68K &c, uint32_t endereco) {
    endereco &= 0xFFFFFE;
    if (__builtin_expect(endereco < 0xFF0000, 1)) {
#ifdef CPS2RECOMP_N64
        return RomLong(g_romData, endereco);
#else
        if (endereco + 3 < c.romSize) {
            const uint8_t *p = &c.rom[endereco];
            return (static_cast<uint32_t>(CartByte(p)) << 24) |
                   (static_cast<uint32_t>(CartByte(p + 1)) << 16) |
                   (static_cast<uint32_t>(CartByte(p + 2)) << 8) |
                   static_cast<uint32_t>(CartByte(p + 3));
        }
        return (static_cast<uint32_t>(c.bus.Read16(endereco)) << 16) | c.bus.Read16(endereco + 2);
#endif
    }
    return RamLong(&c.ram[endereco & 0xFFFF]);
}

// Leitura de 8 bits da ROM (com cache no N64, ou RAM de trabalho caso o endereco seja alto)
static inline uint8_t Rom8(M68K &c, uint32_t endereco) {
    endereco &= 0xFFFFFF;
    if (__builtin_expect(endereco < 0xFF0000, 1)) {
#ifdef CPS2RECOMP_N64
        return *RomPtr(g_romData, endereco);
#else
        if (endereco < c.romSize) {
            return CartByte(&c.rom[endereco]);
        }
        return c.bus.Read8(endereco);
#endif
    }
    return c.ram[endereco & 0xFFFF];
}

// Escreve uma peca de sprite de 8 bytes na memoria de objetos (0x708000)
static inline void EscrevePecaSprite(M68K &c, uint32_t coord32, uint16_t tile, uint16_t attr) {
    const uint32_t ea = c.a[4];
    WRITE_HASH(c, ea, coord32);
    WRITE_HASH(c, ea + 4, tile);
    WRITE_HASH(c, ea + 6, attr);
    if (__builtin_expect(c.objWrite != nullptr, 1)) {
        uint8_t *dest = c.objWrite + (ea & 0x1FFF);
#if defined(CPS2RECOMP_N64) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        typedef uint32_t Alias32 __attribute__((may_alias));
        typedef uint16_t Alias16 __attribute__((may_alias));
        *reinterpret_cast<Alias32*>(dest) = coord32;
        *reinterpret_cast<Alias16*>(dest + 4) = tile;
        *reinterpret_cast<Alias16*>(dest + 6) = attr;
#else
        dest[0] = static_cast<uint8_t>(coord32 >> 24);
        dest[1] = static_cast<uint8_t>(coord32 >> 16);
        dest[2] = static_cast<uint8_t>(coord32 >> 8);
        dest[3] = static_cast<uint8_t>(coord32);
        dest[4] = static_cast<uint8_t>(tile >> 8);
        dest[5] = static_cast<uint8_t>(tile);
        dest[6] = static_cast<uint8_t>(attr >> 8);
        dest[7] = static_cast<uint8_t>(attr);
#endif
    } else {
        c.bus.Write16(ea, static_cast<uint16_t>(coord32 >> 16));
        c.bus.Write16(ea + 2, static_cast<uint16_t>(coord32));
        c.bus.Write16(ea + 4, tile);
        c.bus.Write16(ea + 6, attr);
    }
    c.a[4] += 8;
}

} // namespace

// Rotina principal reescrita: fn_01E062 (DrawSpritePieces)
void DrawSpritePieces(M68K &c) {
    using namespace native;

    // 01E062: tst.b (a0)
    // Se o objeto estiver inativo (byte zero), encerra imediatamente
    const uint32_t a0_off = c.a[0] & 0xFFFF;
    const uint8_t *obj = &c.ram[a0_off];
    const uint8_t status = obj[0];
    c.Logic(status, 0);
    if (status == 0) {
        return;
    }

    // 01E068: move.l 0x34(a0), d0
    // Ponteiro para a definicao do sprite do objeto na ROM
    const uint32_t def_sprite = RamLong(obj + 0x34);
    c.d[0] = def_sprite;
    c.Logic(def_sprite, 2);
    if (def_sprite == 0) {
        return;
    }
    c.a[1] = def_sprite;

    // 01E072: move.w 0x5B8(a5), d0
    // 01E076: move.w (a1)+, d6
    // 01E078: sub.w d6, d0
    // 01E07A: bcs $1DF12
    // Limite de pecas de sprite restantes no quadro (orcamento)
    const uint32_t ea_orcamento = (c.a[5] + 0x5B8) & 0xFFFF;
    uint8_t *p_orcamento = &c.ram[ea_orcamento];
    const uint16_t orcamento = RamWord(p_orcamento);
    const uint16_t pecas_total = Rom16(c, c.a[1]);
    c.a[1] += 2;
    Set16(c.d[6], pecas_total);

    const uint32_t sub_orcamento = c.Sub(orcamento, pecas_total, 1, false, false);
    Set16(c.d[0], sub_orcamento);
    if (c.flagC) {
        return;
    }

    // 01E07E: move.w d0, 0x5B8(a5)
    SetRamWord(p_orcamento, static_cast<uint16_t>(sub_orcamento));
    WRITE_HASH(c, c.a[5] + 0x5B8, static_cast<uint16_t>(sub_orcamento));

    // 01E082: move.w (a1)+, d4
    uint16_t d4 = Rom16(c, c.a[1]);
    c.a[1] += 2;
    Set16(c.d[4], d4);

    // 01E084: move.w 0x8(a0), d5
    const uint16_t obj_x = RamWord(obj + 0x8);
    Set16(c.d[5], obj_x);

    // 01E088: move.w 0x10(a0), d7
    // 01E08C: add.w 0xC(a0), d7
    // 01E090: sub.w 0xC(a6), d7
    // 01E094: not.w d7
    // 01E096: addi.w #$101, d7
    const uint16_t obj_y = RamWord(obj + 0x10);
    const uint16_t obj_z = RamWord(obj + 0xC);
    const uint8_t *cam = &c.ram[c.a[6] & 0xFFFF];
    const uint16_t cam_y = RamWord(cam + 0xC);
    uint16_t d7 = (obj_y + obj_z - cam_y) & 0xFFFF;
    d7 = (~d7 + 0x101) & 0xFFFF;
    Set16(c.d[7], d7);

    // 01E09A: moveq #0, d3
    // 01E09C: move.b (a1)+, d3
    // 01E09E: swap d3
    // 01E0A0: move.b 0x3F(a0), d3
    // 01E0A4: ror.l #3, d3
    const uint8_t b1 = Rom8(c, c.a[1]);
    c.a[1] += 1;
    const uint8_t b2 = obj[0x3F];
    uint32_t d3 = (static_cast<uint32_t>(b1) << 16) | b2;
    d3 = (d3 >> 3) | (d3 << 29);
    c.d[3] = d3;

    // 01E0A6: tst.b (a1)+
    const uint8_t flag_formato = Rom8(c, c.a[1]);
    c.a[1] += 1;
    c.Logic(flag_formato, 0);

    const uint16_t cam_x = RamWord(cam + 0x8);
    const uint16_t base_x = (obj_x - cam_x) & 0xFFFF;

    if (flag_formato == 0) {
        // Formato A: atributo/paleta compartilhado para todas as pecas
        const uint16_t loop_count = uint16_t(c.d[6]); // pecas_total
        Set16(c.d[6], (loop_count - 1) & 0xFFFF);

        // 01E22C: btst #0, 0x2A(a0)
        c.flagZ = !(obj[0x2A] & 1);
        if (!c.flagZ) {
            d4 = (d4 & 0xFFE0) | obj[0x2B];
            Set16(c.d[4], d4);
        }

        // 01E23E: movea.l (a1)+, a2
        c.a[2] = Rom32(c, c.a[1]);
        c.a[1] += 4;

        // 01E240: tst.b 0x2C(a0)
        c.Logic(obj[0x2C], 0);
        const bool inverte_x = !c.flagZ;

        if (inverte_x) {
            // 01E248: bchg #5, d4
            d4 ^= (1 << 5);
            Set16(c.d[4], d4);

            // Loop 1A (0x1E24C): invertido horizontalmente
            uint16_t tam = d4 & 0xF00;
            c.Logic(tam, 1);
            if (tam) tam >>= 4;
            Set16(c.d[0], tam);

            for (uint32_t i = 0; i < loop_count; i++) {
                const uint32_t offs = Rom32(c, c.a[2]);
                c.a[2] += 4;
                const uint16_t off_x = static_cast<uint16_t>(offs >> 16);
                const uint16_t off_y = static_cast<uint16_t>(offs);

                const uint16_t pos_x = (base_x - off_x + 0x30 - tam) & 0xFFFF;
                const uint16_t pos_y = (d7 + off_y) & 0xFFFF;
                const uint32_t coord32 = ((static_cast<uint32_t>(pos_x) & 0x3FF) << 16) |
                                         (static_cast<uint32_t>(pos_y) & 0x3FF) | d3;
                c.d[1] = coord32;

                const uint16_t tile = Rom16(c, c.a[1]);
                c.a[1] += 2;

                EscrevePecaSprite(c, coord32, tile, d4);
            }
        } else {
            // Loop 1B (0x1E284): orientacao normal
            for (uint32_t i = 0; i < loop_count; i++) {
                const uint32_t offs = Rom32(c, c.a[2]);
                c.a[2] += 4;
                const uint16_t off_x = static_cast<uint16_t>(offs >> 16);
                const uint16_t off_y = static_cast<uint16_t>(offs);

                const uint16_t pos_x = (base_x + off_x + 0x40) & 0xFFFF;
                const uint16_t pos_y = (d7 + off_y) & 0xFFFF;
                const uint32_t coord32 = ((static_cast<uint32_t>(pos_x) & 0x3FF) << 16) |
                                         (static_cast<uint32_t>(pos_y) & 0x3FF) | d3;
                c.d[1] = coord32;

                const uint16_t tile = Rom16(c, c.a[1]);
                c.a[1] += 2;

                EscrevePecaSprite(c, coord32, tile, d4);
            }
        }

        Set16(c.d[6], 0xFFFF);
        c.Logic(d4, 1);
    } else {
        // Formato B: cada peca possui seus proprios atributos na tabela
        const uint16_t loop_count = d4;
        Set16(c.d[6], (loop_count - 1) & 0xFFFF);

        // 01E2AA: movea.l (a1)+, a2
        c.a[2] = Rom32(c, c.a[1]);
        c.a[1] += 4;

        // 01E2AC: btst #0, 0x2A(a0)
        c.flagZ = !(obj[0x2A] & 1);
        const bool paleta_custom = !c.flagZ;

        if (!paleta_custom) {
            // 01E2B6: tst.b 0x2C(a0)
            c.Logic(obj[0x2C], 0);
            const bool inverte_x = !c.flagZ;

            if (inverte_x) {
                // Loop 2.1 (0x1E2BE): invertido horizontalmente, sem paleta custom
                uint16_t ultimo_attr = 0;
                for (uint32_t i = 0; i < loop_count; i++) {
                    const uint32_t offs = Rom32(c, c.a[2]);
                    c.a[2] += 4;
                    const uint16_t off_x = static_cast<uint16_t>(offs >> 16);
                    const uint16_t off_y = static_cast<uint16_t>(offs);

                    const uint32_t peca = Rom32(c, c.a[1]);
                    c.a[1] += 4;
                    const uint16_t tile = static_cast<uint16_t>(peca >> 16);
                    uint16_t attr = static_cast<uint16_t>(peca);

                    uint16_t tam = attr & 0xF00;
                    c.Logic(tam, 1);
                    if (tam) tam >>= 4;
                    Set16(c.d[4], tam);

                    const uint16_t pos_x = (base_x - off_x + 0x30 - tam) & 0xFFFF;
                    const uint16_t pos_y = (d7 + off_y) & 0xFFFF;
                    const uint32_t coord32 = ((static_cast<uint32_t>(pos_x) & 0x3FF) << 16) |
                                             (static_cast<uint32_t>(pos_y) & 0x3FF) | d3;

                    attr ^= (1 << 5);
                    EscrevePecaSprite(c, coord32, tile, attr);

                    c.d[1] = (coord32 & 0xFFFF0000) | attr;
                    ultimo_attr = attr;
                }
                Set16(c.d[6], 0xFFFF);
                c.Logic(ultimo_attr, 1);
            } else {
                // Loop 2.2 (0x1E2FE): orientacao normal, sem paleta custom
                uint16_t ultimo_attr = 0;
                for (uint32_t i = 0; i < loop_count; i++) {
                    const uint32_t offs = Rom32(c, c.a[2]);
                    c.a[2] += 4;
                    const uint16_t off_x = static_cast<uint16_t>(offs >> 16);
                    const uint16_t off_y = static_cast<uint16_t>(offs);

                    const uint16_t pos_x = (base_x + off_x + 0x40) & 0xFFFF;
                    const uint16_t pos_y = (d7 + off_y) & 0xFFFF;
                    const uint32_t coord32 = ((static_cast<uint32_t>(pos_x) & 0x3FF) << 16) |
                                             (static_cast<uint32_t>(pos_y) & 0x3FF) | d3;

                    const uint32_t peca = Rom32(c, c.a[1]);
                    c.a[1] += 4;
                    const uint16_t tile = static_cast<uint16_t>(peca >> 16);
                    const uint16_t attr = static_cast<uint16_t>(peca);

                    EscrevePecaSprite(c, coord32, tile, attr);

                    c.d[1] = (coord32 & 0xFFFF0000) | attr;
                    ultimo_attr = attr;
                }
                Set16(c.d[6], 0xFFFF);
                c.Logic(ultimo_attr, 1);
            }
        } else {
            // 01E326: move.b 0x2B(a0), d2
            const uint8_t d2 = obj[0x2B];
            Set8(c.d[2], d2);

            // 01E32A: tst.b 0x2C(a0)
            c.Logic(obj[0x2C], 0);
            const bool inverte_x = !c.flagZ;

            if (inverte_x) {
                // Loop 2.3 (0x1E332): invertido horizontalmente, com paleta custom
                uint16_t ultimo_attr = 0;
                for (uint32_t i = 0; i < loop_count; i++) {
                    const uint32_t offs = Rom32(c, c.a[2]);
                    c.a[2] += 4;
                    const uint16_t off_x = static_cast<uint16_t>(offs >> 16);
                    const uint16_t off_y = static_cast<uint16_t>(offs);

                    const uint32_t peca = Rom32(c, c.a[1]);
                    c.a[1] += 4;
                    const uint16_t tile = static_cast<uint16_t>(peca >> 16);
                    uint16_t attr = static_cast<uint16_t>(peca);

                    uint16_t tam = attr & 0xF00;
                    c.Logic(tam, 1);
                    if (tam) tam >>= 4;
                    Set16(c.d[4], tam);

                    const uint16_t pos_x = (base_x - off_x + 0x30 - tam) & 0xFFFF;
                    const uint16_t pos_y = (d7 + off_y) & 0xFFFF;
                    const uint32_t coord32 = ((static_cast<uint32_t>(pos_x) & 0x3FF) << 16) |
                                             (static_cast<uint32_t>(pos_y) & 0x3FF) | d3;

                    attr = ((attr ^ (1 << 5)) & 0xFFE0) | d2;
                    EscrevePecaSprite(c, coord32, tile, attr);

                    c.d[1] = (coord32 & 0xFFFF0000) | attr;
                    ultimo_attr = attr;
                }
                Set16(c.d[6], 0xFFFF);
                c.Logic(ultimo_attr, 1);
            } else {
                // Loop 2.4 (0x1E378): orientacao normal, com paleta custom
                uint16_t ultimo_attr = 0;
                for (uint32_t i = 0; i < loop_count; i++) {
                    const uint32_t offs = Rom32(c, c.a[2]);
                    c.a[2] += 4;
                    const uint16_t off_x = static_cast<uint16_t>(offs >> 16);
                    const uint16_t off_y = static_cast<uint16_t>(offs);

                    const uint16_t pos_x = (base_x + off_x + 0x40) & 0xFFFF;
                    const uint16_t pos_y = (d7 + off_y) & 0xFFFF;
                    const uint32_t coord32 = ((static_cast<uint32_t>(pos_x) & 0x3FF) << 16) |
                                             (static_cast<uint32_t>(pos_y) & 0x3FF) | d3;

                    const uint32_t peca = Rom32(c, c.a[1]);
                    c.a[1] += 4;
                    const uint16_t tile = static_cast<uint16_t>(peca >> 16);
                    const uint16_t attr = (static_cast<uint16_t>(peca) & 0xFFE0) | d2;

                    EscrevePecaSprite(c, coord32, tile, attr);

                    c.d[1] = (coord32 & 0xFFFF0000) | attr;
                    ultimo_attr = attr;
                }
                Set16(c.d[6], 0xFFFF);
                c.Logic(ultimo_attr, 1);
            }
        }
    }
}

// 0x01DF14: Laco de objetos 1 (objetos prioritarios / lista 0x5C2)
void DrawSpriteList1(M68K &c) {
    using namespace native;

    const uint32_t a5_off = c.a[5] & 0xFFFF;
    const uint16_t d0_inicial = RamWord(&c.ram[(a5_off + 0x5C2) & 0xFFFF]);
    Set16(c.d[0], d0_inicial);
    c.Logic(d0_inicial, 1);
    if (d0_inicial == 0) {
        return;
    }

    uint16_t curr_obj = d0_inicial;
    while (true) {
        c.a[0] = S16(curr_obj);
        const uint32_t a0_off = curr_obj;
        const uint8_t *obj = &c.ram[a0_off];

        const uint8_t flag40 = obj[0x40];
        c.Logic(flag40, 0);
        if (flag40 == 0) {
            Call(c, 0x001ED2);
            if (!c.flagZ) {
                DrawSpritePieces(c);
                Call(c, 0x01EC4E);
            }
        }

        const uint16_t next_obj = RamWord(obj + 0x26);
        Set16(c.d[0], next_obj);
        c.Logic(next_obj, 1);
        if (static_cast<int16_t>(next_obj) >= 0) {
            break;
        }
        curr_obj = next_obj;
    }
}

// 0x01DF3E: Laco de objetos 2 (lista principal de objetos 0x5BE)
void DrawSpriteList2(M68K &c) {
    using namespace native;

    const uint32_t a5_off = c.a[5] & 0xFFFF;
    const uint16_t d0_inicial = RamWord(&c.ram[(a5_off + 0x5BE) & 0xFFFF]);
    Set16(c.d[0], d0_inicial);
    c.Logic(d0_inicial, 1);
    if (d0_inicial == 0) {
        return;
    }

    uint16_t curr_obj = d0_inicial;
    while (true) {
        c.a[0] = S16(curr_obj);
        const uint32_t a0_off = curr_obj;
        uint8_t *obj = &c.ram[a0_off];

        const uint8_t flag28 = obj[0x28];
        const uint8_t flag40 = obj[0x40];

        // 01DF46: tst.b 0x28(a0); sne d0; and.b 0x40(a0), d0; bne $1E03E
        if (flag28 != 0 && flag40 != 0) {
            goto next_object;
        }

        // 01DF54: tst.b 0x28(a0); bne $1E048
        if (flag28 != 0) {
            // 01E048: btst #0, 0x1(a0); beq $1E03E
            if (obj[0x1] & 1) {
                DrawSpritePieces(c);
                Call(c, 0x01EC4E);
            }
            goto next_object;
        }

        // Caso flag28 == 0:
        // Anexo 1 previo (0x4A)
        {
            const int16_t anexo1 = static_cast<int16_t>(RamWord(obj + 0x4A));
            if (anexo1 < 0) {
                c.a[3] = S16(static_cast<uint16_t>(anexo1));
                const int16_t z_val = static_cast<int16_t>(RamWord(&c.ram[(static_cast<uint32_t>(anexo1) + 0x10) & 0xFFFF]));
                if (z_val < 0 && (obj[0x1] & 1)) {
                    Call(c, 0x01E0B4);
                }
            }
        }

        // Anexo 2 previo (0x48)
        {
            const int16_t anexo2 = static_cast<int16_t>(RamWord(obj + 0x48));
            if (anexo2 < 0) {
                c.a[3] = S16(static_cast<uint16_t>(anexo2));
                const int16_t z_val = static_cast<int16_t>(RamWord(&c.ram[(static_cast<uint32_t>(anexo2) + 0x10) & 0xFFFF]));
                if (z_val < 0 && (obj[0x1] & 1)) {
                    Call(c, 0x01E0B4);
                }
            }
        }

        // Teste de culling / visibilidade na tela
        {
            const uint32_t cam_ea = (c.ram[(a5_off + 0x36) & 0xFFFF] != 0)
                                        ? ((a5_off + 0x54E) & 0xFFFF)
                                        : ((a5_off + 0x4FE) & 0xFFFF);
            obj[0x1] &= ~1; // bclr #0, 0x1(a0)
            WRITE_HASH(c, c.a[0] + 1, obj[0x1]);

            if (!(obj[0x1] & 2)) { // btst #1, 0x1(a0); bne $1DFFE
                const uint16_t cam_x = RamWord(&c.ram[(cam_ea + 0x8) & 0xFFFF]);
                const uint16_t obj_x = RamWord(obj + 0x8);
                const uint16_t dx = (obj_x - cam_x + 0x60) & 0xFFFF;
                if (dx <= 0x240) {
                    const uint16_t cam_y = RamWord(&c.ram[(cam_ea + 0xC) & 0xFFFF]);
                    const uint16_t obj_y = RamWord(obj + 0xC);
                    const uint16_t obj_z = RamWord(obj + 0x10);
                    const uint16_t dy = (obj_y + obj_z - cam_y + 0xC0) & 0xFFFF;
                    if (dy <= 0x1C0) {
                        obj[0x1] |= 1; // bset #0, 0x1(a0)
                        WRITE_HASH(c, c.a[0] + 1, obj[0x1]);
                        DrawSpritePieces(c);
                        Call(c, 0x01EC4E);
                    }
                }
            }
        }

        // Anexo 1 posterior (0x4A)
        {
            const int16_t anexo1 = static_cast<int16_t>(RamWord(obj + 0x4A));
            if (anexo1 < 0) {
                c.a[3] = S16(static_cast<uint16_t>(anexo1));
                const int16_t z_val = static_cast<int16_t>(RamWord(&c.ram[(static_cast<uint32_t>(anexo1) + 0x10) & 0xFFFF]));
                if (z_val >= 0 && (obj[0x1] & 1)) {
                    Call(c, 0x01E0B4);
                }
            }
        }

        // Anexo 2 posterior (0x48)
        {
            const int16_t anexo2 = static_cast<int16_t>(RamWord(obj + 0x48));
            if (anexo2 < 0) {
                c.a[3] = S16(static_cast<uint16_t>(anexo2));
                const int16_t z_val = static_cast<int16_t>(RamWord(&c.ram[(static_cast<uint32_t>(anexo2) + 0x10) & 0xFFFF]));
                if (z_val >= 0 && (obj[0x1] & 1)) {
                    Call(c, 0x01E0B4);
                }
            }
        }

    next_object:
        const uint16_t next_obj = RamWord(obj + 0x26);
        Set16(c.d[0], next_obj);
        c.Logic(next_obj, 1);
        if (static_cast<int16_t>(next_obj) >= 0) {
            break;
        }
        curr_obj = next_obj;
    }
}

// 0x01E3A6: Desenha sombras/indicadores de chao dos objetos de 0x5BE
void DrawShadowSprites(M68K &c) {
    using namespace native;

    const uint32_t a5_off = c.a[5] & 0xFFFF;
    const uint16_t d0_inicial = RamWord(&c.ram[(a5_off + 0x5BE) & 0xFFFF]);
    Set16(c.d[0], d0_inicial);
    c.Logic(d0_inicial, 1);
    if (d0_inicial == 0) {
        return;
    }

    uint16_t curr_obj = d0_inicial;
    while (true) {
        c.a[0] = S16(curr_obj);
        const uint32_t a0_off = curr_obj;
        const uint8_t *obj = &c.ram[a0_off];

        // 01E3B0: jsr $1ED2 (atualiza bit 0 de 0x1(a0))
        Call(c, 0x001ED2);

        // 01E3B6: cmpi.b #$29, 0x34(a5)
        if (c.ram[(a5_off + 0x34) & 0xFFFF] == 0x29) {
            if (RamWord(obj + 0xC) == 0) {
                goto next_shadow;
            }
        }

        // 01E3C8: tst.b 0x28(a0); sne d0; and.b 0x40(a0), d0; bne $1E44E
        if (obj[0x28] != 0 && obj[0x40] != 0) {
            goto next_shadow;
        }

        // 01E3D6: btst #0, 0x1(a0); beq $1E44E
        if (!(obj[0x1] & 1)) {
            goto next_shadow;
        }

        {
            // 01E3E2: tabela indexada por 0x3B(a0) << 4 em 0x1E45C
            uint32_t a2 = 0x1E45C + (static_cast<uint32_t>(obj[0x3B]) << 4);

            const uint32_t ea_orcamento = (a5_off + 0x5B8) & 0xFFFF;
            uint8_t *p_orcamento = &c.ram[ea_orcamento];
            const uint16_t orcamento = RamWord(p_orcamento);
            const uint16_t custo = Rom16(c, a2);
            a2 += 2;

            if (orcamento < custo) {
                // 01E3F6: bcs $1DF12 (orcamento de sprites esgotado)
                return;
            }

            const uint16_t novo_orcamento = orcamento - custo;
            SetRamWord(p_orcamento, novo_orcamento);
            WRITE_HASH(c, c.a[5] + 0x5B8, novo_orcamento);

            // Calculo de coordenadas da peca de sombra
            const uint32_t a6_off = c.a[6] & 0xFFFF;
            const uint16_t cam_x = RamWord(&c.ram[(a6_off + 0x8) & 0xFFFF]);
            const uint16_t cam_y = RamWord(&c.ram[(a6_off + 0xC) & 0xFFFF]);
            const uint16_t obj_x = RamWord(obj + 0x8);
            const uint16_t obj_z = RamWord(obj + 0x10);

            uint16_t d1 = (obj_x - cam_x + 0x40) & 0xFFFF;
            uint16_t d2 = (0x100 - ((obj_z - cam_y + 0xC) & 0xFFFF)) & 0xFFFF;

            const uint16_t tile = Rom16(c, a2); a2 += 2;
            const uint16_t off_x = Rom16(c, a2); a2 += 2;
            uint16_t attr = Rom16(c, a2); a2 += 2;
            uint16_t d5 = Rom16(c, a2); a2 += 2;
            const uint16_t off_y = Rom16(c, a2); a2 += 2;
            const uint16_t d6 = Rom16(c, a2);

            d1 = (d1 + off_x) & 0xFFFF;
            d2 = (d2 - off_y) & 0xFFFF;

            if (obj[0x2C] != 0) {
                d5 = (0 - d5) & 0xFFFF;
                attr ^= (1 << 5); // inverte flip horizontal
            }

            d1 = ((d1 + d5) & 0x3FF) | (d6 & 0xFFFF);
            d2 = (d2 & 0x3FF) | 0x2000;

            const uint32_t coord32 = (static_cast<uint32_t>(d1) << 16) | (static_cast<uint32_t>(d2) & 0xFFFF);
            EscrevePecaSprite(c, coord32, tile, attr);
        }

    next_shadow:
        const uint16_t next_obj = RamWord(obj + 0x26);
        Set16(c.d[0], next_obj);
        c.Logic(next_obj, 1);
        if (static_cast<int16_t>(next_obj) >= 0) {
            break;
        }
        curr_obj = next_obj;
    }
}

void RegisterSpriteRewrite() {
    constexpr uint32_t kLivrePieces = kFreeD0 | kFreeD1 | kFreeD2 | kFreeD3 | kFreeD4 |
                                      kFreeD5 | kFreeD6 | kFreeD7 | kFreeA1 | kFreeA2 | kFreeFlags;
    constexpr uint32_t kLivreList = kFreeD0 | kFreeD1 | kFreeD2 | kFreeD3 | kFreeD4 |
                                    kFreeD5 | kFreeD6 | kFreeD7 | kFreeA0 | kFreeA1 |
                                    kFreeA2 | kFreeA3 | kFreeFlags;

    // (no PC, DDSOM_LIFT = mascara das reescritas ligadas, para isolar uma que nao bata no teste de equivalencia)
    unsigned on = 15;
#ifndef CPS2RECOMP_N64
    if (const char *m = getenv("DDSOM_LIFT")) on = unsigned(strtoul(m, nullptr, 0));
#endif
    if (on & 1) AddRewrite(0x01E062, "DrawSpritePieces", DrawSpritePieces, kLivrePieces);
    if (on & 2) AddRewrite(0x01DF14, "DrawSpriteList1", DrawSpriteList1, kLivreList);
    if (on & 4) AddRewrite(0x01DF3E, "DrawSpriteList2", DrawSpriteList2, kLivreList);
    if (on & 8) AddRewrite(0x01E3A6, "DrawShadowSprites", DrawShadowSprites, kLivreList);
}

} // namespace cps2recomp
