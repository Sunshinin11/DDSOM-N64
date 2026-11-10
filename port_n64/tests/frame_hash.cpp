#include "../board.h"
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace cps2recomp;
static std::vector<uint8_t> Load(const char*p){FILE*f=fopen(p,"rb");if(!f)abort();fseek(f,0,SEEK_END);size_t n=ftell(f);rewind(f);std::vector<uint8_t>b(n);if(fread(b.data(),1,n,f)!=n)abort();fclose(f);return b;}
static uint64_t H(uint64_t h,const void*p,size_t n){auto*q=(const uint8_t*)p;for(size_t i=0;i<n;i++){h^=q[i];h*=1099511628211ull;}return h;}
// One line per frame: a hash of what the game made visible and audible (the CPU's time is not part of it).
int main(int argc,char**argv){
 auto program=Load("work/ddsom/n64fs_native/program.bin"),opcodes=Load("work/ddsom/n64fs_native/opcodes.bin");
 NativeBoard*a=new NativeBoard;a->program=program.data();a->opcodes=opcodes.data();a->programSize=program.size();a->Reset();
 a->cpu.recording=true;a->optimizePolling=true;
 unsigned frames=argc>1?atoi(argv[1]):3000;
 for(unsigned f=0;f<frames;f++){
  a->players[0]=getenv("FHNOPAD")?0:((f%300)==180?kPadCoin:0)|((f%300)==240?kPadStart:0)|(f>60?kPadRight:0)|((f%20)<5?kPadB1:0);
  a->RunFrame(M68K::Mode::Native);
  uint64_t h=1469598103934665603ull;
  h=H(h,a->ram,sizeof a->ram);h=H(h,a->videoRam,sizeof a->videoRam);h=H(h,a->objectRam,sizeof a->objectRam);
  h=H(h,a->palette,sizeof a->palette);h=H(h,a->cpsA,sizeof a->cpsA);h=H(h,a->cpsB,sizeof a->cpsB);
  uint8_t x[16];while(a->PopSound(x))h=H(h,x,16);
  if(getenv("FHV"))fprintf(stderr,"f%u cycles=%llu skipped=%llu pc=%06x\n",f,(unsigned long long)a->cpu.cycles,(unsigned long long)a->pollSkipped,a->cpu.pc);printf("%u %016llx\n",f,(unsigned long long)h);
 }
 fprintf(stderr,"blocks/cycles total=%llu interpreted=%llu\n",(unsigned long long)a->cpu.cycles,(unsigned long long)a->cpu.interpreted);
}
