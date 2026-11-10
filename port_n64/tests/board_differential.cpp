#include "../board.h"
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace cps2recomp;
static std::vector<uint8_t> Load(const char*p){FILE*f=fopen(p,"rb");if(!f)abort();fseek(f,0,SEEK_END);size_t n=ftell(f);rewind(f);std::vector<uint8_t>b(n);if(fread(b.data(),1,n,f)!=n)abort();fclose(f);return b;}
int main(int argc,char**argv){
 auto program=Load("work/ddsom/n64fs_native/program.bin"),opcodes=Load("work/ddsom/n64fs_native/opcodes.bin");
 NativeBoard* a=new NativeBoard,*b=new NativeBoard;
 for(auto*c:{a,b}){c->program=program.data();c->opcodes=opcodes.data();c->programSize=program.size();c->Reset();}
 a->cpu.recording=true;a->optimizePolling=true;
 unsigned frames=argc>1?atoi(argv[1]):3000;
 for(unsigned frame=0;frame<frames;frame++){
  // Deterministic coin/start, movement and attacks, identical on both executions.
  uint16_t pad=((frame%300)==180?kPadCoin:0)|((frame%300)==240?kPadStart:0)|(frame>60?kPadRight:0)|((frame%20)<5?kPadB1:0);
  a->players[0]=b->players[0]=pad;
  a->RunFrame(M68K::Mode::Native);b->RunFrame(M68K::Mode::Blocks);
  bool equal=a->cpu.pc==b->cpu.pc&&a->cpu.cycles==b->cpu.cycles&&a->cpu.SR()==b->cpu.SR()
   &&!memcmp(a->cpu.d,b->cpu.d,sizeof a->cpu.d)&&!memcmp(a->cpu.a,b->cpu.a,sizeof a->cpu.a)
   &&!memcmp(a->ram,b->ram,sizeof a->ram)&&!memcmp(a->videoRam,b->videoRam,sizeof a->videoRam)
   &&!memcmp(a->objectRam,b->objectRam,sizeof a->objectRam)&&!memcmp(a->palette,b->palette,sizeof a->palette)
   &&!memcmp(a->cpsA,b->cpsA,sizeof a->cpsA)&&!memcmp(a->cpsB,b->cpsB,sizeof a->cpsB)
   &&!memcmp(a->soundRam,b->soundRam,sizeof a->soundRam)&&a->soundWrite==b->soundWrite;
  if(frame==599){for(unsigned i=0;i<16;i++){auto*p=a->ram+0x64+i*16;auto word=[](const uint8_t*q){return unsigned(q[0])<<24|unsigned(q[1])<<16|unsigned(q[2])<<8|q[3];};uint32_t stack=word(p+8);fprintf(stderr,"TASK %u state=%02x pc=%06x stack=%06x return=%06x flags=%04x gate=%02x/%02x\n",i,p[0],word(p+4),stack,stack>=0xff0000&&stack<=0xffffbf?word(a->ram+((stack+60)&65535)):0,unsigned(a->ram[0xffd0])<<8|a->ram[0xffd1],a->ram[0x8030],a->ram[0x8033]);}}
  if(frame%300==299)fprintf(stderr,"BOARD STATE frame=%u pc=%06x sound=%u ready=%02x busy=%02x\n",frame,a->cpu.pc,a->soundRequests,a->soundRam[0xfff],a->soundRam[15]);
  uint8_t x[16],y[16];bool ax,by;do{ax=a->PopSound(x);by=b->PopSound(y);equal&=ax==by&&(!ax||!memcmp(x,y,16));}while(ax||by);
  if(!equal){fprintf(stderr,"BOARD DIFFERENCE frame=%u pc=%06x/%06x cycles=%llu/%llu sr=%04x/%04x\n",frame,a->cpu.pc,b->cpu.pc,(unsigned long long)a->cpu.cycles,(unsigned long long)b->cpu.cycles,a->cpu.SR(),b->cpu.SR());return 1;}
 }
 FILE*discovered=fopen("port_n64/build/discovered_entries.txt","w");for(auto pc:a->cpu.newEntries)fprintf(discovered,"%06X\n",pc);fclose(discovered);
 if(a->cpu.interpreted||a->soundRequests<10||a->cpu.pc==0x19a2){fprintf(stderr,"BOARD incomplete coverage or boot failure\n");return 1;}
 printf("POLL SKIPPED %llu exact repeated passes\n",(unsigned long long)a->pollSkipped);
 printf("NATIVE FALLBACK instructions=%llu entries=%zu\n",(unsigned long long)a->cpu.interpreted,a->cpu.newEntries.size());
 printf("BOARD PASS %u frames: translated vs interpreter, CPU flags/registers/time, RAM, video, palette and sound queue\n",frames);
}




