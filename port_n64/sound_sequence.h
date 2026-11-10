#pragma once
#include "../sound/z80.h"
#include <vector>
#include <functional>
#include <cstring>
namespace cps2recomp {
class NativeSoundSequence final:public Z80Bus{
public:
 using Sink=std::function<void(uint8_t,uint16_t)>;
 NativeSoundSequence(const std::vector<uint8_t>&program,const std::vector<uint8_t>&opcodes,Sink callback):cpu(*this),rom(program),ops(opcodes),sink(callback){Reset();}
 Z80 cpu;
 uint32_t registerWrites=0,submitted=0,overflows=0;
 void Reset(){memset(shared,0,sizeof shared);memset(ram,0,sizeof ram);shared[15]=255;shared[0xffd]=0x88;shared[0xffe]=1;read=write=0;clock=0;bank=0;latch=0;ready=false;cpu.Reset();cpu.cycles=0;}
 bool Submit(const uint8_t*r){if(write-read==128){overflows++;return false;}memcpy(queue[write++%128],r,16);submitted++;return true;}
 void Tick(Z80::Mode mode=Z80::Mode::Native){
  if(ready&&shared[15]==255&&read!=write){memcpy(shared,queue[read++%128],16);shared[15]=0;}
  clock+=32000;cpu.RaiseInterrupt();cpu.Run(clock,mode);
 }
 uint8_t Read(uint16_t a) override{
  if(a<0x8000)return a<rom.size()?rom[a]:255;
  if(a<0xc000){size_t off=0x8000+bank*0x4000+a-0x8000;return off<rom.size()?rom[off]:255;}
  if(a<0xd000){if(a==0xcfff&&shared[0xfff]==0x77)shared[0xfff]=255;return shared[a-0xc000];}
  if(a==0xd007)return 0x80; // Native register sink is immediately ready.
  if(a>=0xf000)return ram[a-0xf000];
  return 255;
 }
 uint8_t ReadOpcode(uint16_t a) override{return a<ops.size()?ops[a]:Read(a);}
 void Write(uint16_t a,uint8_t v) override{
  if(a>=0xc000&&a<0xd000){shared[a-0xc000]=v;if(a==0xcfff&&v==0x77)ready=true;}
  else if(a==0xd000)latch=(latch&255)|(uint16_t(v)<<8);
  else if(a==0xd001)latch=(latch&0xff00)|v;
  else if(a==0xd002){sink(v,latch);registerWrites++;}
  else if(a==0xd003)bank=v&15;
  else if(a>=0xf000)ram[a-0xf000]=v;
 }
 bool SameMemory(const NativeSoundSequence&o)const{return !memcmp(shared,o.shared,sizeof shared)&&!memcmp(ram,o.ram,sizeof ram)&&bank==o.bank;}
private:
 const std::vector<uint8_t>&rom;const std::vector<uint8_t>&ops;Sink sink;
 uint8_t shared[4096]={},ram[4096]={},queue[128][16]={};
 uint64_t clock=0;uint32_t read=0,write=0,bank=0;uint16_t latch=0;bool ready=false;
};
}

