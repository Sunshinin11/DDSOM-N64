#include "../audio_protocol.h"
#include <cassert>
#include <cstdio>
int main(){ddsom::AudioLevels l;uint8_t r[16]={0xff,7,0,1,0};l.Command(r);for(int i=0;i<256;i++)l.Tick();assert(l.music==0&&l.step==0);r[1]=6;r[2]=254;l.Command(r);for(int i=0;i<256;i++)l.Tick();assert(l.music==65536);r[1]=0x0d;l.Command(r);for(int i=0;i<256;i++)l.Tick();assert(l.all==0);r[1]=0;assert(l.Command(r)==2&&l.all==65536);r[1]=5;assert(l.Command(r)==1);r[1]=0xab;l.Command(r);assert(l.unknown==1);puts("AUDIO PROTOCOL PASS stop, fade directions, target levels, bounded endpoints, unknown command accounting");}
