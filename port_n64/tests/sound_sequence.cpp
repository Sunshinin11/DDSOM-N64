#include "../sound_sequence.h"
#include <cstdio>
#include <cstdlib>
#include <tuple>
using namespace cps2recomp;
static std::vector<uint8_t> Load(const char*p){FILE*f=fopen(p,"rb");if(!f)abort();fseek(f,0,SEEK_END);size_t n=ftell(f);rewind(f);std::vector<uint8_t>b(n);fread(b.data(),1,n,f);fclose(f);return b;}
int main(){auto rom=Load("work/ddsom/n64fs/z80.bin"),ops=Load("work/ddsom/program.bin.z80");
 std::vector<std::pair<unsigned,unsigned>> x,y;
 NativeSoundSequence a(rom,ops,[&](uint8_t r,uint16_t v){x.emplace_back(r,v);}),b(rom,ops,[&](uint8_t r,uint16_t v){y.emplace_back(r,v);});
 a.cpu.recording=true;unsigned regs[256]={};
 unsigned writes=0,requests=0;
 for(unsigned tick=0;tick<30000;tick++){
  if(tick>100&&tick%25==0){uint8_t req[16]={};unsigned code=(tick/25)%0x490;req[0]=code>>8;req[1]=code;req[7]=16;
   if(tick%1000==0){req[0]=255;req[1]=tick%2000?5:0;}
   if(a.Submit(req)!=b.Submit(req))abort();requests++;
  }
  a.Tick(Z80::Mode::Native);b.Tick(Z80::Mode::Blocks);
  bool same=x==y&&a.cpu.pc==b.cpu.pc&&a.cpu.cycles==b.cpu.cycles&&a.cpu.f==b.cpu.f
   &&!memcmp(a.cpu.r,b.cpu.r,sizeof a.cpu.r)&&a.SameMemory(b)
   &&!memcmp(a.cpu.r2,b.cpu.r2,sizeof a.cpu.r2)&&a.cpu.f2==b.cpu.f2
   &&a.cpu.sp==b.cpu.sp&&a.cpu.ix==b.cpu.ix&&a.cpu.iy==b.cpu.iy
   &&a.cpu.i==b.cpu.i&&a.cpu.refresh==b.cpu.refresh&&a.cpu.iff1==b.cpu.iff1&&a.cpu.iff2==b.cpu.iff2;
  if(!same){fprintf(stderr,"SOUND DIFFERENCE tick=%u pc=%04x/%04x writes=%zu/%zu cycles=%llu/%llu\n",tick,a.cpu.pc,b.cpu.pc,x.size(),y.size(),(unsigned long long)a.cpu.cycles,(unsigned long long)b.cpu.cycles);return 1;}
  writes+=x.size();for(auto&v:x)regs[v.first]++;x.clear();y.clear();
 }
 FILE*out=fopen("port_n64/build/sound-discovered.txt","w");for(auto pc:a.cpu.newEntries)fprintf(out,"%04X\n",pc);fclose(out);
 FILE*hist=fopen("port_n64/build/sound-registers.txt","w");for(int r=0;r<256;r++)if(regs[r])fprintf(hist,"%02X %u\n",r,regs[r]);fclose(hist);
 if(writes<1000){fprintf(stderr,"SOUND inactive: %u writes\n",writes);return 1;}
 if(a.cpu.interpreted){fprintf(stderr,"SOUND FALLBACK %llu\n",(unsigned long long)a.cpu.interpreted);return 1;}
 printf("SOUND SEQUENCE PASS 30000 IRQ ticks, %u commands, %u identical register writes; native fallback=0\n",requests,writes);
}

