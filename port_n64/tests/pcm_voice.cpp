#include "../pcm_voice.h"
#include <cassert>
#include <initializer_list>
#include <cstdio>
int main(){
 for(unsigned start=0;start<65536;start+=127)for(unsigned span=1;span<=65536;span+=257){
  uint16_t end=start+span;
  for(unsigned loop:{0u,1u,511u,32768u,65535u}){
   auto r=ddsom::MakePcmRange(start,end,loop);
   assert(r.count>0&&r.count<=65536&&r.loop<=r.count&&r.start<r.count);
   assert(ddsom::PcmBankAddress(0x8003,r.base,r.start)==(0x30000u|start));
   assert(uint16_t(r.base+r.count)==end);
   if(loop)assert(uint16_t(r.base+r.count-r.loop)==uint16_t(end-loop));
  }
 }
 puts("PCM PASS bank wrapping, full-bank samples, long and short terminal loops");
}

