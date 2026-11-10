#include <libdragon.h>
#include "audio.h"
#include "sound_sequence.h"
#include "pcm_voice.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>
#include <cstring>
#include <malloc.h>
namespace ddsom{
namespace{
constexpr unsigned voices=16,chipRate=24038;
struct Context{uint16_t bank,base;};
struct Voice{
 uint16_t bank=0x8000,at=0,step=0,phase=0,loop=0,end=0,pan=0x120;
 int16_t volume=0;
 bool dirty=false,active=false;unsigned which=0;
 Context ctx[2];waveform_t wave[2];
} v[voices];
std::vector<uint8_t> program,opcodes;
cps2recomp::NativeSoundSequence* sequence=nullptr;
pi_addr_t sampleRom[2]={};unsigned sampleCount=0,outputRate=0,phase=0;
uint32_t requests=0,polls=0,lastPoll=0,maxGap=0,streamBytes=0,waitTicks=0,keyons=0,badSamples=0,gainClamps=0;
uint64_t sequenceUs=0,applyUs=0,pollUs=0;uint32_t soundTicks=0;
struct Resident {uint8_t*data=nullptr;unsigned capacity=0,count=0,last=0;uint16_t bank=0,base=0;};
Resident resident[32];unsigned residentBytes=0,residentHits=0,residentMisses=0,residentFallback=0;
bool ready=false;
const void* ResidentPcm(uint16_t bank,uint16_t base,unsigned count){
 for(auto&r:resident)if(r.data&&r.bank==bank&&r.base==base&&r.count==count){r.last=soundTicks;residentHits++;return r.data;}
 Resident*best=nullptr;unsigned capacity=(count+47)&~15u;
 for(auto&r:resident){
  bool used=false;for(auto&c:v)if(r.data&&c.active&&c.wave[c.which].mem==r.data){used=true;break;}
  if(used)continue;
  if(residentBytes-r.capacity+capacity>384*1024)continue;
  if(!best||!r.data||r.last<best->last)best=&r;
  if(!r.data)break;
 }
 if(!best){residentFallback++;return nullptr;}
 auto&r=*best;
 if(r.data){rspq_wait();free(r.data);residentBytes-=r.capacity;}
 r.data=static_cast<uint8_t*>(memalign(16,capacity));r.count=0;r.capacity=0;
 if(!r.data){residentFallback++;return nullptr;}
 r.capacity=capacity;residentBytes+=capacity;
 memset(r.data,0,capacity);data_cache_hit_writeback_invalidate(r.data,capacity);
 for(unsigned pos=0;pos<count;){
  unsigned address=PcmBankAddress(bank,base,pos),n=std::min(count-pos,65536u-uint16_t(base+pos));
  if(address+n>sampleCount){badSamples++;free(r.data);residentBytes-=r.capacity;r=Resident{};return nullptr;}
  auto*dst=r.data+pos;pi_addr_t pi=sampleRom[0]+address;
  if((reinterpret_cast<uintptr_t>(dst)^pi)&1)pi=sampleRom[1]+address+1;
  dma_read(dst,pi,n);streamBytes+=n;pos+=n;
 }
 r.bank=bank;r.base=base;r.count=count;r.last=soundTicks;residentMisses++;return r.data;
}
std::vector<uint8_t> Load(const char*p){FILE*f=fopen(p,"rb");assertf(f,"Missing sound data %s",p);fseek(f,0,SEEK_END);long size=ftell(f);rewind(f);assertf(size>0,"Empty sound data");std::vector<uint8_t>b(size);assertf(fread(b.data(),1,b.size(),f)==b.size(),"Truncated sound data");fclose(f);return b;}
void Read(void*ctx,samplebuffer_t*sbuf,int pos,int length,bool){
 auto&c=*static_cast<Context*>(ctx);
 while(length>0){
  unsigned address=PcmBankAddress(c.bank,c.base,pos);
  int slot=(sbuf->head+sbuf->widx)%sbuf->size,room=sbuf->size-slot;
  int n=std::min(length,room);n=std::min(n,int(65536-uint16_t(c.base+pos)));
  void*ram=samplebuffer_append(sbuf,n);
  uint32_t begin=TICKS_READ();samplebuffer_dma_wait(sbuf);waitTicks+=TICKS_READ()-begin;
  if(address+unsigned(n)>sampleCount){memset(ram,0,n);badSamples++;}
  else{
   pi_addr_t pi=sampleRom[0]+address;
   if((reinterpret_cast<uintptr_t>(ram)^pi)&1)pi=sampleRom[1]+address+1;
   sbuf->dma_ticket=dma_read_async(ram,pi,n);streamBytes+=n;
  }
  pos+=n;length-=n;
 }
}
void Register(uint8_t reg,uint16_t value){
 if(reg<0x80){unsigned index=reg/8;auto&c=v[index];
  switch(reg&7){
   case 0:{auto&n=v[(index+1)&15];n.dirty|=n.bank!=value;n.bank=value;break;}
   case 1:c.dirty|=c.at!=value;c.at=value;break;
   case 2:c.step=value;break;
   case 3:c.phase=value;break;
   case 4:c.dirty|=c.loop!=value;c.loop=value;break;
   case 5:c.dirty|=c.end!=value;c.end=value;break;
   case 6:c.volume=int16_t(value);break;
  }
 }else if(reg<0x90)v[reg-0x80].pan=value;
 // Echo/spatial DSP registers are intentionally separate from PCM playback.
}
void Apply(){
 for(unsigned i=0;i<voices;i++){
  auto&c=v[i];
  if(!c.volume||!c.step){if(c.active)mixer_ch_stop(i);c.active=false;continue;}
  if(!c.active||c.dirty){
   c.which^=1;auto&ctx=c.ctx[c.which];auto&w=c.wave[c.which];
   auto range=MakePcmRange(c.at,c.end,c.loop);ctx={c.bank,range.base};
   w=waveform_t{};w.name="DDSOM PCM";w.bits=8;w.channels=1;w.frequency=chipRate;w.len=range.count;w.loop_len=range.loop;
   w.read=Read;w.ctx=&ctx;w.async_read=true;
   w.mem=ResidentPcm(c.bank,range.base,range.count);
   mixer_ch_play(i,&w);if(range.start||c.phase)mixer_ch_set_pos(i,double(range.start)+double(c.phase>>4)/4096.0);
   c.active=true;c.dirty=false;keyons++;
  }
  float frequency=float(chipRate)*c.step/4096.0f;mixer_ch_set_freq(i,frequency);
  float gain=float(c.volume)/16384.0f;
  if(gain>1||gain< -1){gainClamps++;gain=std::clamp(gain,-1.0f,1.0f);}
  float pan=std::clamp(int(c.pan)-0x110,0,32)/32.0f;
  mixer_ch_set_vol(i,gain*sqrtf(1-pan),gain*sqrtf(pan));
 }
}
int Tick(void*){uint32_t t=get_ticks_us();sequence->Tick();sequenceUs+=get_ticks_us()-t;t=get_ticks_us();Apply();applyUs+=get_ticks_us()-t;soundTicks++;phase+=outputRate;unsigned n=phase/250;phase%=250;return n?n:1;}
}
void NativeAudioInit(){
 program=Load("rom:/sound_program.bin");opcodes=Load("rom:/sound_opcodes.bin");
 assertf(program.size()>=0x8000&&!memcmp(program.data()+0x16,"1.15",4),"Unsupported sound driver version");
 assertf(opcodes.size()>=0x8000&&opcodes[0x89]==0xfb&&opcodes[0x8b]==0xfb&&opcodes[0x175]==0xfb,"Invalid sound opcode data");
 sampleRom[0]=dfs_rom_addr("/sample8.bin");sampleRom[1]=dfs_rom_addr("/sample8_shifted.bin");assertf(sampleRom[0]&&sampleRom[1],"Missing native PCM banks");
 FILE*f=fopen("rom:/sample8.bin","rb");assertf(f,"Missing PCM resource");fseek(f,0,SEEK_END);sampleCount=ftell(f);fclose(f);
 audio_init(12019,AUDIO_INIT_LATENCY_MS(80));outputRate=audio_get_frequency();mixer_init(voices);
 for(unsigned i=0;i<voices;i++)mixer_ch_set_limits(i,8,chipRate*16,18432);
 sequence=new cps2recomp::NativeSoundSequence(program,opcodes,Register);
 mixer_add_event(0,Tick,nullptr);ready=true;
 debugf("[DDSOM_AUDIO_NATIVE] driver=1.15 voices=16 output_hz=%u aot=1 prerecorded_tracks=0\n",outputRate);
}
void NativeAudioRequest(const uint8_t*r){if(!ready)return;if(requests<4)debugf("[DDSOM_SOUND_REQUEST] command=%02x%02x\n",unsigned(r[0]),unsigned(r[1]));while(!sequence->Submit(r))AudioPoll();requests++;}
void NativeAudioPoll(){if(!ready)return;uint32_t now=get_ticks_us();if(lastPoll&&now-lastPoll>maxGap)maxGap=now-lastPoll;lastPoll=now;polls++;uint32_t begin=get_ticks_us();mixer_try_play();pollUs+=get_ticks_us()-begin;}
void NativeAudioReport(){if(!ready)return;debugf("[DDSOM_PCM_CACHE] bytes=%u hits=%u misses=%u streaming=%u\n",residentBytes,residentHits,residentMisses,residentFallback);debugf("[DDSOM_AUDIO_COST] ticks=%lu sequence_us=%llu apply_us=%llu poll_us=%llu\n",(unsigned long)soundTicks,(unsigned long long)sequenceUs,(unsigned long long)applyUs,(unsigned long long)pollUs);debugf("[DDSOM_AUDIO] requests=%lu keyons=%lu polls=%lu max_gap_us=%lu pcm_bytes=%lu dma_wait_us=%lu invalid_reads=%lu gain_clamps=%lu driver_fallback=%llu\n",(unsigned long)requests,(unsigned long)keyons,(unsigned long)polls,(unsigned long)maxGap,(unsigned long)streamBytes,(unsigned long)TICKS_TO_US(waitTicks),(unsigned long)badSamples,(unsigned long)gainClamps,(unsigned long long)sequence->cpu.interpreted);}
}






