#pragma once
#include <stdint.h>
namespace ddsom{
struct PcmRange{uint16_t base;uint32_t count,loop,start;};
inline PcmRange MakePcmRange(uint16_t start,uint16_t end,uint16_t loop){
 uint32_t count=uint16_t(end-start);if(!count)count=65536;
 if(loop>count)return {uint16_t(end-loop),loop,loop,uint16_t(start-uint16_t(end-loop))};
 return {start,count,loop,0};
}
inline uint32_t PcmBankAddress(uint16_t bank,uint16_t base,uint32_t pos){return (uint32_t(bank&0x7fff)<<16)|uint16_t(base+pos);}
}
