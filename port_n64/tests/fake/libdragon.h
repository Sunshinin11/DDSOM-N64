#pragma once
#include <stdint.h>
#include <stddef.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>
#define assertf(c,...) do{if(!(c)){fprintf(stderr,__VA_ARGS__);abort();}}while(0)
#define debugf(...) ((void)0)
#define PhysicalAddr(p) reinterpret_cast<uintptr_t>(p)
enum{FMT_CI4,FMT_RGBA16,TILE0=0,TILE1=1,TLUT_RGBA16,FILTER_POINT};
struct surface_t{void*buffer;int width,height;};
struct rdpq_tileparms_t{uint8_t palette=0;};
namespace fake{
inline std::vector<uint8_t> gfx;
inline surface_t*target=nullptr;inline uint16_t tmem[2048],tlut[256],fill;
inline const uint8_t*image;inline int cx0=0,cy0=0,cx1=384,cy1=240;
struct Tile{int at=0,stride=0,palette=0;} ;inline Tile tiles[8];
}
inline uint32_t get_ticks_us(){static uint32_t t;return ++t;}
inline void data_cache_hit_writeback(const void*,size_t){}
inline void data_cache_hit_invalidate(const void*,size_t){}
inline void dma_read(void*p,uint32_t at,size_t n){assertf(at&&at-1+n<=fake::gfx.size(),"DMA range");memcpy(p,fake::gfx.data()+at-1,n);}
inline void rdpq_attach(surface_t*p,void*){fake::target=p;fake::cx0=fake::cy0=0;fake::cx1=p->width;fake::cy1=p->height;}
inline void rdpq_detach_wait(){}
inline void rdpq_set_scissor(int x0,int y0,int x1,int y1){fake::cx0=x0;fake::cy0=y0;fake::cx1=x1;fake::cy1=y1;}
inline uint16_t color_from_packed16(uint16_t c){return c;}
inline void rdpq_set_mode_fill(uint16_t c){fake::fill=c;}
inline void rdpq_fill_rectangle(int x0,int y0,int x1,int y1){for(int y=std::max(y0,fake::cy0);y<std::min(y1,fake::cy1);y++)for(int x=std::max(x0,fake::cx0);x<std::min(x1,fake::cx1);x++)((uint16_t*)fake::target->buffer)[y*fake::target->width+x]=fake::fill;}
inline void rdpq_set_mode_standard(){}inline void rdpq_mode_tlut(int){}inline void rdpq_mode_alphacompare(int){}inline void rdpq_mode_filter(int){}
inline void rdpq_tex_upload_tlut(const uint16_t*p,int at,int n){memcpy(fake::tlut+at,p,n*2);}
inline void rdpq_set_texture_image_raw(int,uintptr_t at,int,int,int){fake::image=(const uint8_t*)at;}
inline void rdpq_set_tile(int tile,int,int at,int stride,const rdpq_tileparms_t*p){fake::tiles[tile]={at,stride,p?p->palette:0};}
inline void rdpq_load_block(int tile,int,int,int n,int){memcpy((uint8_t*)fake::tmem+fake::tiles[tile].at,fake::image,n*2);}
inline void rdpq_set_tile_size(int,int,int,int,int){}
inline void rdpq_texture_rectangle_scaled(int tile,int x0,int y0,int x1,int y1,float s0,float t0,float s1,float t1){
 auto&t=fake::tiles[tile];const uint8_t*tex=(uint8_t*)fake::tmem+t.at;
 for(int y=std::max(y0,fake::cy0);y<std::min(y1,fake::cy1);y++)for(int x=std::max(x0,fake::cx0);x<std::min(x1,fake::cx1);x++){
 int sx=int(floorf(s0+(s1-s0)*(x-x0+0.5f)/(x1-x0))),sy=int(floorf(t0+(t1-t0)*(y-y0+0.5f)/(y1-y0)));
 unsigned pen=(tex[sy*t.stride+sx/2]>>((sx&1)?0:4))&15;uint16_t c=fake::tlut[t.palette*16+pen];
 if(c&1)((uint16_t*)fake::target->buffer)[y*fake::target->width+x]=c;
 }
}
