#include "../save_format.h"
#include <cassert>
#include <cstdio>
int main(){uint16_t w[64],out[64];uint8_t r[144];for(int i=0;i<64;i++)w[i]=i*997;ddsom::SaveEncode(r,w,0xffffffff);assert(ddsom::SaveValid(r));ddsom::SaveDecode(r,out);for(int i=0;i<64;i++)assert(w[i]==out[i]);for(unsigned i=0;i<144;i++){r[i]^=1;assert(!ddsom::SaveValid(r));r[i]^=1;}uint32_t next=ddsom::Be32(r+4)+1;assert(next==0&&int32_t(next-ddsom::Be32(r+4))>0);ddsom::SaveEncode(r,w,next);assert(ddsom::SaveValid(r));puts("SAVE PASS BE encoding, CRC, every byte corruption and sequence wrap");}
