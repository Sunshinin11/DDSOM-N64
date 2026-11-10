// Shadow over Mystara N64: som de alto nivel (audio.cpp)
#pragma once
#include <cstdint>

namespace ddsom {
void AudioInit();
void AudioRequest(const uint8_t *request); // os 16 bytes do pedido do jogo
void AudioPoll();                           // uma vez por frame: alimenta o AI com o mixer
} // namespace ddsom

namespace ddsom { void AudioReport(); }

namespace ddsom { uint64_t AudioTimeTicks(); }
