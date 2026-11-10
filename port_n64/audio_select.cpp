#include <libdragon.h>
#include "audio.h"
#include "run_options.h"
#ifndef DDSOM_WITH_NATIVE_AUDIO
#define DDSOM_WITH_NATIVE_AUDIO 1
#endif
namespace ddsom {
void RecordedAudioInit();void RecordedAudioRequest(const uint8_t*);void RecordedAudioPoll();void RecordedAudioReport();
void NativeAudioInit();void NativeAudioRequest(const uint8_t*);void NativeAudioPoll();void NativeAudioReport();
namespace { uint64_t spent=0;unsigned depth=0; }
struct Measure {uint32_t start;bool outer;Measure():start(TICKS_READ()),outer(depth++==0){}~Measure(){depth--;if(outer)spent+=uint32_t(TICKS_READ()-start);}};
uint64_t AudioTimeTicks(){return spent;}
void AudioInit(){
 debugf("[DDSOM_AUDIO_MODE] mode=%u\n",unsigned(g_options.audio));
 if(g_options.audio==AudioMode::Recorded)RecordedAudioInit();
#if DDSOM_WITH_NATIVE_AUDIO
 else if(g_options.audio==AudioMode::NativeDriver)NativeAudioInit();
#else
 else if(g_options.audio==AudioMode::NativeDriver)assertf(false,"Native driver is only available in comparison builds");
#endif
}
void AudioRequest(const uint8_t*r){Measure m;if(g_options.audio==AudioMode::Recorded)RecordedAudioRequest(r);
#if DDSOM_WITH_NATIVE_AUDIO
else if(g_options.audio==AudioMode::NativeDriver)NativeAudioRequest(r);
#endif
}
void AudioPoll(){if(g_options.audio==AudioMode::Silent)return;
 // (the callers poll far more often than the audio needs, a buffer is some 20 ms: no more than a poll a millisecond)
 static uint32_t last=0;const uint32_t now=TICKS_READ();if(last&&now-last<TICKS_FROM_US(1000))return;last=now;
 Measure m;if(g_options.audio==AudioMode::Recorded)RecordedAudioPoll();
#if DDSOM_WITH_NATIVE_AUDIO
else NativeAudioPoll();
#endif
}
void AudioReport(){if(g_options.audio==AudioMode::Recorded)RecordedAudioReport();
#if DDSOM_WITH_NATIVE_AUDIO
else if(g_options.audio==AudioMode::NativeDriver)NativeAudioReport();
#endif
debugf("[DDSOM_AUDIO_TOTAL] us=%llu\n",(unsigned long long)TICKS_TO_US(spent));}
}
