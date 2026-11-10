#include <libdragon.h>
#include "../board.h"
#include "../rdp_video.h"
#include "../run_options.h"
#include "../../../video/video.h"
#include <cassert>
using namespace cps2recomp;
namespace cps2recomp{
struct NativeVideoReference{
 static void Load(Video&r,const NativeBoard&v){
  auto old=r.m_src;uint8_t bytes[0x1800];uint16_t a[32]={},b[32]={};
  for(int i=0;i<0xc00;i++){bytes[i*2]=v.palette[i]>>8;bytes[i*2+1]=v.palette[i];}
  b[0x30/2]=0x3f;r.m_src.videoRam=bytes;r.m_src.cpsA=a;r.m_src.cpsB=b;r.CopyPalette();r.m_src=old;
  memcpy(r.m_objects,v.objects,sizeof v.objects);r.m_lastObject=v.lastObject;r.m_priorityControl=v.priorityControl;
 }
};}
static void Word(uint8_t*p,unsigned at,uint16_t v){p[at]=v>>8;p[at+1]=v;}
int main(){
 fake::gfx.assign(0x1000000,255);
 for(unsigned region:{0u,0x800000u})for(unsigned at=0;at<0x10000;at++)fake::gfx[region+at]=uint8_t((at*73+(at>>5)*19)^0x5a);
 std::vector<uint8_t> planar(fake::gfx.size()),empty((fake::gfx.size()+255)/256,0);
 for(unsigned at=0;at<planar.size();at+=4)for(unsigned x=0;x<8;x++){
  unsigned pen=(fake::gfx[at+x/2]>>((x&1)?0:4))&15;
  for(unsigned bit=0;bit<4;bit++)planar[at+bit]|=((pen>>bit)&1)<<(7-x);
 }
 RdpVideoInit(1,fake::gfx.size(),empty.data());
 static uint16_t pixels[384*240];surface_t fb{pixels,384,240};
 NativeBoard v;unsigned rng=77;
 for(unsigned mode:{0u,1u,2u,3u})for(unsigned test=0;test<80;test++){
  ddsom::g_options.videoFastPaths=mode&1;RdpVideoSetTest(true,mode&2);
  v.Reset();v.paletteChanged=true;v.cpsA[1]=0x9000;v.cpsA[2]=0x9040;v.cpsA[3]=0x9080;v.cpsA[4]=0x9200;
  v.cpsA[17]=12|(test%3?1:0);v.cpsA[16]=test*7;
  unsigned order[3]={1,2,3};if(test&1)std::swap(order[0],order[2]);if(test&2)std::swap(order[0],order[1]);
  v.cpsB[0x26/2]=14|(order[0]<<8)|(order[1]<<10)|(order[2]<<12);
  v.priorityControl=((test%8)<<4)|(((test*3)%8)<<8)|(((test*5)%8)<<12);
  v.objectRegs[4]=64;v.objectRegs[5]=16;
  for(unsigned i=0;i<0xc00;i++)v.palette[i]=uint16_t(0xf000|((i*73+test*17)&0xfff));
  for(unsigned plane=0;plane<3;plane++){
   v.cpsA[6+plane*2]=test*3+plane*7;v.cpsA[7+plane*2]=test*5;
   for(unsigned i=0;i<4096;i++){
    rng=rng*1664525+1013904223;Word(v.videoRam,plane*0x4000+i*4,(rng>>16)%(test>=60?2048:64));
    Word(v.videoRam,plane*0x4000+i*4+2,((rng>>27)&31)|((rng>>20)&0x60));
   }
  }
  for(unsigned y=0;y<1024;y++)Word(v.videoRam,0x20000+y*2,(test&4)?y*3:0);
  for(unsigned i=0;i<30;i++){
   unsigned off=i*4;v.objects[off]=uint16_t(64+(test>=40?(-12+int(i%5)*95):(60+int(i%5)*7))+((i+test)%8)*8192);
   v.objects[off+1]=16+(test>=40?(-12+int(i%4)*74):(40+int(i%4)*9));v.objects[off+2]=i*5;
   v.objects[off+3]=uint16_t((i&31)|((i&3)<<5)|((i%3)<<8)|(((i/3)%3)<<12));
  }
  v.lastObject=29*4;
  Video::Sources src;src.graphics=planar.data();src.graphicsSize=planar.size();src.videoRam=v.videoRam;src.cpsA=v.cpsA;src.cpsB=v.cpsB;src.objectRegs=v.objectRegs;
  Video ref(src);ref.Reset();NativeVideoReference::Load(ref,v);
  memset(pixels,0,sizeof pixels);Stats stats;rdpq_attach(&fb,nullptr);RdpVideoDraw(v,stats,&fb);rdpq_detach_wait();
  uint32_t row[384];for(int y=0;y<224;y++){
   ref.RenderLine(y,row);for(int x=0;x<384;x++){
    unsigned c=row[x];uint16_t expected=(((c&255)>>3)<<11)|((((c>>8)&255)>>3)<<6)|((((c>>16)&255)>>3)<<1)|1;
    if(pixels[(y+8)*384+x]!=expected){fprintf(stderr,"RDP DIFFERENCE case=%u x=%d y=%d got=%04x expected=%04x\n",test,x,y,pixels[(y+8)*384+x],expected);return 1;}
   }
  }
 }
 puts("RDP PASS 80 overlapping sprite/priority/flip/rowscroll/palette cases match reference pixels in both paths, including clipped edges");
}



