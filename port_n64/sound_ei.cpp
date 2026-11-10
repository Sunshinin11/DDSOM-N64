#include "../runtime/native_z80.h"
namespace cps2recomp::z80native {
// Exact post-EI instructions of the validated DDSOM 1.15 driver. This is not an opcode decoder.
bool AfterEI(Z80&c){switch(c.pc){
 case 0x008a:c.refresh++;c.pc=c.Pop();c.cycles+=10;return true;
 case 0x008c:c.refresh++;c.SetPair(2,0,0xf002);c.cycles+=10;c.pc=0x008f;return true;
 case 0x0176:c.refresh++;c.r[7]=c.Rd(0xcfff);c.cycles+=13;c.pc=0x0179;return true;
 default:return false;
}}
void Idle115(Z80&c){
 // 017D..0198 polls three interrupt counters: 99 cycles/12 refreshes per empty pass.
 if(c.cycles>=c.budget||c.budget-c.cycles<=99)return;
 if(c.Rd(0xf001)||c.Rd(0xf002)||c.Rd(0xf003))return;
 uint64_t n=(c.budget-c.cycles-1)/99;
 c.cycles+=n*99;c.refresh+=uint8_t(n*12);c.SetPair(2,0,0xf002);c.r[7]=0;c.f=0x44;
}
}
