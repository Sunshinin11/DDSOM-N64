// N64: int32_t e long no mips64-elf; o std::max(int32_t, int) do soundtrack.cpp precisa de um par igual
#include <algorithm>
#include <cstdint>
namespace std {
inline int32_t max(int32_t a, int b) { return a > b ? a : b; }
}
#include "../machine/soundtrack.cpp"
