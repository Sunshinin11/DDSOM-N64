// N64 (port_n64): contadores de chamadas para a telemetria. So contam; nao mudam nada do que acontece.
#pragma once
#include <cstdint>

namespace cps2recomp {
#ifdef CPS2RECOMP_N64
struct PerfCounters {
    uint32_t at68 = 0, inside68 = 0;     // consultas a tabela de blocos do 68000 (recomp.cpp)
    uint32_t dispatchMiss68 = 0;         // ... que o cache de despacho nao respondeu (busca binaria)
    uint32_t atZ80 = 0, insideZ80 = 0;   // ... do Z80 (recomp_z80.cpp)
    uint32_t bus68r = 0, bus68w = 0;     // Machine::Read8/16 e Write8/16 (o que nao e RAM nem ROM no codigo traduzido)
    uint32_t busZ80r = 0, busZ80w = 0;   // Machine::Read/Write do Z80
};
extern PerfCounters g_perf;
extern bool g_profileNative;
void ProfileNative(uint32_t pc,uint32_t ticks);
void ReportNativeProfile();
#define N64_PERF_COUNT(field) (++g_perf.field)
// Mede o custo medio de uma consulta a cada tabela (n consultas cronometradas em bloco), em nanossegundos
uint32_t BenchDispatch68k(uint32_t n);
uint32_t BenchDispatchZ80(uint32_t n);
#else
#define N64_PERF_COUNT(field) ((void)0)
#endif
} // namespace cps2recomp
