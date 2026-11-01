#include <CHGame.h>
#include "Art.h"
#include "ArtData.h"

namespace art {
namespace {
inline __attribute__((always_inline)) void span(int x,int y,int width,unsigned ink) {
    if(unsigned(y)>=128 || width<=0)return;
    int end=x+width;if(x<0)x=0;if(end>128)end=128;if(x>=end)return;
    uint8_t *p=gfx_fb+y*64+x/2;
    if(x&1) {*p=(*p&15)|(ink<<4);++p;++x;}
    unsigned pair=ink*17;
    uint8_t *last=p+(end-x)/2;
    while(p<last)*p++=pair;
    if(end&1)*p=(*p&240)|ink;
}
}
void draw(Sprite id,unsigned frame,int center,int foot,int height,bool mirror,bool distant) {
    const auto &animation=ANIMATIONS[id];const auto &f=animation.frames[frame%animation.count];
    if(height<2 || height>110)return;
    int scale=height*256/f.h,width=f.w*scale/256,left=center-width/2,top=foot-height;
    if(left>=128 || left+width<0 || top>=128 || foot<0)return;
    for(int sy=0;sy<f.h;++sy) {
        int y0=top+sy*scale/256,y1=top+(sy+1)*scale/256;
        if(y1<=0 || y0>=128 || y0==y1)continue;
        const uint8_t *p=f.data+f.rows[sy];unsigned runs=*p++;int sx=0;
        while(runs--) {
            unsigned run=*p++,count=(run>>4)+1,ink=run&15;
            int a=sx*scale/256,b=(sx+count)*scale/256;sx+=count;
            if(ink==15 || a==b)continue;
            if(distant && ink)ink=ink==8?2:1;
            if(mirror) {int old=a;a=width-b;b=width-old;}
            for(int y=y0;y<y1;++y)span(left+a,y,b-a,ink);
        }
    }
}
namespace {
void rootSpan(int a,int b,int y0,int y1,const GroundClip &clip,unsigned ink) {
    for(int y=y0;y<y1;++y) {
        if(unsigned(y)>=99)continue;
        int left=a,right=b,edge=clip.edge[y];
        if(clip.right) {if(left<edge)left=edge;}
        else if(right>edge)right=edge;
        span(left,y,right-left,ink);
    }
}
}
RAMFUNC(nightSprite) void projected(Sprite id,unsigned frame,int x,int foot,int height,int wind,const GroundClip &ground,bool mirror,bool distant) {
    const auto &animation=ANIMATIONS[id];const auto &f=animation.frames[frame%animation.count];
    if(height<512 || height>1024*256)return;
    int scale=height/f.h,width=f.w*scale;
    int anchor=mirror?f.w-f.anchorX:f.anchorX,left=x-anchor*scale,top=foot-f.anchorY*scale;
    if(left>=128*256 || left+width<0 || top>=99*256 || foot<0)return;
    int crown=f.h*3/4,windStep=wind*256/crown,clipFrom=f.h*ground.from/256;
    int screenY=top>>8,remainder=top&255;
    for(int sy=0;sy<f.h;++sy) {
        int y0=screenY;remainder+=scale;screenY+=remainder>>8;remainder&=255;int y1=screenY;
        if(y1<=0 || y0>=99 || y0==y1)continue;
        int sway=windStep && sy<crown?windStep*(crown-sy)/256:0;
        const uint8_t *p=f.data+f.rows[sy];unsigned runs=*p++;int cursor=0;
        while(runs--) {
            unsigned run=*p++,count=(run>>4)+1,ink=run&15;
            int a=cursor;cursor+=count*scale;int b=cursor;
            if(ink==15)continue;
            if(distant && ink)ink=ink==8?2:1;
            if(mirror) {int old=a;a=width-b;b=width-old;}
            a=(left+sway+a)>>8;b=(left+sway+b)>>8;
            if(a==b)continue;
            if(sy<clipFrom)for(int y=y0;y<y1;++y)span(a,y,b-a,ink);
            else rootSpan(a,b,y0,y1,ground,ink);
        }
    }
}
}
