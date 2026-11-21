// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "../machine/m68k.h"

namespace cps2recomp {

void RegisterSpriteRewrite();

// Rotinas nativas de montagem e emissao de sprites do DDSOM
void DrawSpritePieces(M68K &c);
void DrawSpriteList1(M68K &c);
void DrawSpriteList2(M68K &c);
void DrawShadowSprites(M68K &c);

} // namespace cps2recomp
