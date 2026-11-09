#!/usr/bin/env python3
"""Check the actual C++ span renderer, including opaque black animal interiors."""
from pathlib import Path
import shutil
import subprocess
ROOT=Path(__file__).resolve().parents[1]
out=ROOT/'out/art-test';out.mkdir(parents=True,exist_ok=True)
(out/'CHGame.h').write_text('#pragma once\n#include <stdint.h>\nextern uint8_t gfx_fb[8192];\n#define RAMFUNC(name)\n')
(out/'test.cpp').write_text(r'''
#include <cassert>
#include <cstring>
#include "Art.h"
#include "ArtData.h"
uint8_t gfx_fb[8192];
unsigned at(int x,int y){return (gfx_fb[y*64+x/2]>>((x&1)*4))&15;}
static uint8_t expected[128*128];
int main(){
    using namespace art;
    for(unsigned a=0;a<sizeof ANIMATIONS/sizeof ANIMATIONS[0];++a){
        auto &animation=ANIMATIONS[a];
        for(unsigned n=0;n<animation.count;++n){
            auto &f=animation.frames[n];
            for(bool mirror:{false,true}){
                memset(gfx_fb,0x55,sizeof gfx_fb);draw(Sprite(a),n,64,90,f.h,mirror);
                for(int y=0;y<f.h;++y){
                    const uint8_t *p=f.data+f.rows[y];unsigned runs=*p++;int x=0;
                    while(runs--){unsigned b=*p++,count=(b>>4)+1,ink=b&15;
                        for(unsigned k=0;k<count;++k,++x){int dx=64-f.w/2+(mirror?f.w-1-x:x);
                            assert(at(dx,90-f.h+y)==(ink==15?5:ink));
                        }
                    }
                }
            }
        }
    }
    // Preserve the original raster convention at fractional scales,
    // including mirrored spans and clipping above the viewport.
    for(unsigned id=0;id<sizeof ANIMATIONS/sizeof ANIMATIONS[0];++id)for(int height:{7,19,37,73,105})for(bool mirror:{false,true}) {
        auto &f=ANIMATIONS[id].frames[0];int scale=height*256/f.h,width=f.w*scale/256,left=64-width/2,top=100-height;
        memset(expected,5,sizeof expected);memset(gfx_fb,0x55,sizeof gfx_fb);
        for(int sy=0;sy<f.h;++sy) {
            int y0=top+sy*scale/256,y1=top+(sy+1)*scale/256;
            const uint8_t *p=f.data+f.rows[sy];unsigned runs=*p++;int sx=0;
            while(runs--) {unsigned r=*p++,n=(r>>4)+1,c=r&15;int a=sx*scale/256,b=(sx+n)*scale/256;sx+=n;
                if(c==15)continue;if(mirror) {int t=a;a=width-b;b=width-t;}
                for(int y=y0;y<y1;++y)for(int x=left+a;x<left+b;++x)if(unsigned(x)<128 && unsigned(y)<128)expected[y*128+x]=c;
            }
        }
        draw(Sprite(id),0,64,100,height,mirror);
        for(int y=0;y<128;++y)for(int x=0;x<128;++x)assert(at(x,y)==expected[y*128+x]);
    }
    // Crown motion must leave the entire ground band unchanged. Every
    // root pixel, including opaque black, stays outside a curved road cut.
    uint8_t edge[99];for(int y=0;y<99;++y)edge[y]=54+(y%13);
    for(Sprite id:{Pine,Birch,Oak,Shrub})for(unsigned frame=0;frame<ANIMATIONS[id].count;++frame)for(bool right:{false,true})for(bool mirror:{false,true})for(int height:{0,256*256,768*256}) {
        auto &f=ANIMATIONS[id].frames[frame];if(!height)height=f.h*256;GroundClip clip{edge,uint8_t(id==Shrub?0:224),right};
        memset(gfx_fb,0x55,sizeof gfx_fb);projected(id,frame,64*256+93,92*256+117,height,0,clip,mirror);
        uint8_t still[8192];memcpy(still,gfx_fb,sizeof still);
        memset(gfx_fb,0x55,sizeof gfx_fb);
        projected(id,frame,64*256+93,92*256+117,height,512,clip,mirror);
        int scale=height/f.h;int rootTop=(92*256+117-f.anchorY*scale+(f.h*clip.from/256)*scale)>>8;if(rootTop<0)rootTop=0;
        int fixedTop=(92*256+117-f.anchorY*scale+(f.h*3/4)*scale)>>8;
        for(int y=rootTop;y<93 && y<99;++y)for(int x=0;x<128;++x) {
            if(y>=fixedTop)assert(at(x,y)==((still[y*64+x/2]>>((x&1)*4))&15));
            if(right?x<edge[y]:x>=edge[y])assert(at(x,y)==5);
        }
    }
}
'''.replace('#include <cstring>','#include <cstring>\n#include <initializer_list>'))
subprocess.run([shutil.which('clang++') or 'c++','-std=c++17','-O1','-fsanitize=undefined,bounds','-fno-sanitize-recover=all',
    '-I'+str(out),'-I'+str(ROOT),str(out/'test.cpp'),str(ROOT/'Art.cpp'),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],check=True)
print('PASS: every frame/mirror, legacy fractional scales, opaque bodies, fixed roots and curved road clipping')
