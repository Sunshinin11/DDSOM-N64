#pragma once
#include <stdint.h>
#include <string.h>
namespace ddsom {
inline uint32_t SaveCrc(const uint8_t*p,unsigned n){uint32_t c=~0u;for(unsigned i=0;i<n;i++){c^=p[i];for(int k=0;k<8;k++)c=(c>>1)^(0xedb88320u&-(c&1));}return ~c;}
inline uint32_t Be32(const uint8_t*p){return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3];}
inline void Put32(uint8_t*p,uint32_t v){for(int i=3;i>=0;i--){p[i]=v;v>>=8;}}
inline uint32_t SaveChecksum(const uint8_t*r){uint8_t bytes[140];memcpy(bytes,r,8);memcpy(bytes+8,r+12,132);return SaveCrc(bytes,140);}
inline bool SaveValid(const uint8_t*r){return Be32(r)==0x44444e31&&Be32(r+12)==1&&Be32(r+8)==SaveChecksum(r);}
inline void SaveEncode(uint8_t*r,const uint16_t*w,uint32_t seq){Put32(r,0x44444e31);Put32(r+4,seq);Put32(r+12,1);for(int i=0;i<64;i++){r[16+i*2]=w[i]>>8;r[17+i*2]=w[i];}Put32(r+8,SaveChecksum(r));}
inline void SaveDecode(const uint8_t*r,uint16_t*w){for(int i=0;i<64;i++)w[i]=uint16_t(r[16+i*2]<<8|r[17+i*2]);}
}
