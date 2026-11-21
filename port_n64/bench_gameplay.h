#pragma once
#include "board.h"

namespace ddsom {
// Same coin, start and first-character confirmation timings as gameplay_routes.cpp.
inline uint16_t FirstGameplayPad(unsigned frame) {
    using namespace cps2recomp;
    uint16_t pad = 0;
    if (frame < 2000 && frame % 120 >= 80 && frame % 120 < 88)
        pad |= kPadCoin;
    if (frame >= 1970 && frame < 2070 && frame % 24 < 8)
        pad |= kPadStart;
    if (frame >= 2280 && frame < 2288)
        pad |= kPadB1;
    if (frame >= 2800) {
        pad |= kPadRight;
        if (frame % 20 < 8) pad |= kPadB1;
    }
    return pad;
}
}
