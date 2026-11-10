#include <libdragon.h>
#include "audio.h"
#include "audio_protocol.h"
#include <cstdio>
#include <cstring>
namespace ddsom {
namespace {
constexpr int kCodes=0x490,kChannels=8,kCache=24;
struct Entry{uint16_t file=0xffff;bool music=false;} table[kCodes];
struct Cached{wav64_t wave;uint16_t file=0xffff;uint32_t used=0;bool open=false;} cache[kCache];
Cached* active[kChannels]={};float pans[kChannels]={};uint32_t started[kChannels]={},serial=0;
bool ready=false,fadeEvent=false;AudioLevels levels;unsigned phase=0,rate=12019;
uint32_t requests=0,missing=0,opens=0,polls=0,maxGap=0,lastPoll=0;
void Volume(){for(int ch=0;ch<kChannels;ch++){
 float gain=float(levels.all)/AudioLevels::full*(ch?0.8f:0.7f);
 if(!ch)gain*=float(levels.music)/AudioLevels::full;
 mixer_ch_set_vol_pan(ch,gain,ch?pans[ch]:0.5f);
}}
int Tick(void*){
 int music=levels.music,all=levels.all;levels.Tick();
 if(music!=levels.music||all!=levels.all)Volume();
 if(!levels.step&&!levels.allStep){fadeEvent=false;return 0;}
 phase+=rate*2;unsigned n=phase/125;phase%=125;return n?n:1;
}
void ScheduleFade(){if(!fadeEvent&&(levels.step||levels.allStep)){fadeEvent=true;phase=0;mixer_add_event(0,Tick,nullptr);}}
Cached* Wave(uint16_t file){
 for(auto&c:cache)if(c.open&&c.file==file){c.used=serial;return &c;}
 Cached* best=nullptr;
 for(auto&c:cache){bool busy=false;for(int ch=0;ch<kChannels;ch++)if(active[ch]==&c&&mixer_ch_playing(ch))busy=true;
  if(!busy&&(!best||!c.open||c.used<best->used))best=&c;if(best&&!best->open)break;
 }
 if(!best)return nullptr;
 char path[32];snprintf(path,sizeof path,"rom:/snd/s%04X.wav64",file);
 FILE*f=fopen(path,"rb");if(!f)return nullptr;fclose(f);
 if(best->open)wav64_close(&best->wave);
 wav64_open(&best->wave,path);best->file=file;best->open=true;best->used=serial;opens++;return best;
}
}
void RecordedAudioInit(){
 FILE*f=fopen("rom:/snd/table.bin","rb");assertf(f,"Missing sound table");
 uint8_t raw[kCodes*3];size_t n=fread(raw,1,sizeof raw,f);fclose(f);assertf(n==sizeof raw,"Truncated sound table");
 for(int c=0;c<kCodes;c++){table[c].file=uint16_t(raw[c*3]<<8|raw[c*3+1]);table[c].music=raw[c*3+2]!=0;}
 audio_init(12019,AUDIO_INIT_LATENCY_MS(100));rate=audio_get_frequency();mixer_init(kChannels);
 for(int ch=0;ch<kChannels;ch++)mixer_ch_set_limits(ch,16,24038,0);
 wav64_init_compression(2);ready=true;
}
void RecordedAudioRequest(const uint8_t*r){
 if(!ready)return;requests++;
 if(r[0]==0xff){int stop=levels.Command(r);if(stop){for(int c=(stop==3?1:0);c<(stop==1?1:kChannels);c++){mixer_ch_stop(c);active[c]=nullptr;}}Volume();ScheduleFade();return;}
 unsigned code=unsigned(r[0])<<8|r[1];if(code>=kCodes||table[code].file==0xffff){missing++;return;}
 serial++;Cached*w=Wave(table[code].file);if(!w){missing++;return;}
 int ch=0;
 if(table[code].music){mixer_ch_stop(0);levels.NewMusic();wav64_set_loop(&w->wave,true);}
 else{
  ch=-1;for(int c=1;c<kChannels;c++)if(!mixer_ch_playing(c)){ch=c;break;}
  if(ch<0){ch=1;for(int c=2;c<kChannels;c++)if(started[c]<started[ch])ch=c;}
  wav64_set_loop(&w->wave,false);
 }
 active[ch]=w;pans[ch]=std::clamp(float(r[7])/32.0f,0.0f,1.0f);started[ch]=serial;
 wav64_play(&w->wave,ch);Volume();
}
void RecordedAudioPoll(){
 if(!ready)return;uint32_t now=get_ticks_us();if(lastPoll&&now-lastPoll>maxGap)maxGap=now-lastPoll;lastPoll=now;polls++;
 mixer_try_play();
}
void RecordedAudioReport(){debugf("[DDSOM_AUDIO] requests=%lu missing=%lu opens=%lu polls=%lu max_gap_us=%lu unknown_commands=%u\n",(unsigned long)requests,(unsigned long)missing,(unsigned long)opens,(unsigned long)polls,(unsigned long)maxGap,levels.unknown);}
}

