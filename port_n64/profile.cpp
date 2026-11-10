#include <libdragon.h>
#include "perf_n64.h"
namespace cps2recomp {
bool g_profileNative=false;
namespace {struct Entry{uint32_t pc=0,calls=0;uint64_t ticks=0;};Entry entries[1024];uint32_t lost=0;}
void ProfileNative(uint32_t pc,uint32_t ticks){
 unsigned at=((pc>>1)*2654435761u)>>22;
 for(unsigned n=0;n<1024;n++){auto&e=entries[(at+n)&1023];if(!e.calls||e.pc==pc){e.pc=pc;e.calls++;e.ticks+=ticks;return;}}
 lost++;
}
void ReportNativeProfile(){
 for(unsigned n=0;n<20;n++){Entry*best=nullptr;for(auto&e:entries)if(e.calls&&(!best||e.ticks>best->ticks))best=&e;if(!best)break;
 debugf("[DDSOM_CPU_HOT] pc=%06lx calls=%lu us=%llu\n",(unsigned long)best->pc,(unsigned long)best->calls,(unsigned long long)TICKS_TO_US(best->ticks));best->calls=0;best->ticks=0;}
 debugf("[DDSOM_CPU_PROFILE] lost=%lu\n",(unsigned long)lost);
}
}
