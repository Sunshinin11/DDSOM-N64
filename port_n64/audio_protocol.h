#pragma once
#include <stdint.h>
#include <algorithm>
namespace ddsom {
struct AudioLevels {
 static constexpr int full=65536;
 int music=full,target=full,step=0,all=full,allStep=0;
 unsigned unknown=0;
 int Command(const uint8_t*r){
  unsigned rate=(unsigned(r[3])<<8)|r[4];int speed=std::max(1,int(rate));
  switch(r[1]){
  case 0x00:music=target=all=full;step=allStep=0;return 2;
  case 0x01:return 3;
  case 0x05:return 1;
  case 0x06:case 0x07:{bool down=r[1]==7;if(!step)music=down?full:0;target=int(r[2]&254)<<8;if(target>=0xfe00)target=full;step=down?-speed:speed;break;}
  case 0x0c:case 0x0d:{bool out=r[1]==0x0d;if(!allStep)all=out?full:0;allStep=out?-speed:speed;break;}
  default:unknown++;break;
  }return 0;
 }
 void Tick(){
  if(step){int next=music+step;if((step<0&&next<=target)||(step>0&&next>=target)){music=target;step=0;}else music=std::clamp(next,0,full);}
  if(allStep){all=std::clamp(all+allStep,0,full);if(all==full||all==0)allStep=0;}
 }
 void NewMusic(){music=target=full;step=0;}
};
}
