#include <CHGame.h>
#include "View.h"
#include "Art.h"
#include "SignArt.h"
#include "RoadArt.h"
#include "HazeArt.h"
#include "Scenery.h"
#include "Neon.h"
#include "CardScore.h"
#include <string.h>
#ifdef NIGHT_PROFILE
volatile uint32_t *nightProfileData();
#define nightProfile (nightProfileData())
#define ART_STAGE(i,call) do {uint32_t t=micros();call;uint32_t elapsed=micros()-t;nightProfile[1+i]+=elapsed;if(elapsed>nightProfile[11+i])nightProfile[11+i]=elapsed;} while(0)
#else
#define ART_STAGE(i,call) call
#endif

namespace view {
namespace {
enum Ink { Black, Shade, Dim, Edge, Pale, Cyan, Pink, Lime, White, Gold, Glow, PinkGlow, LimeGlow, CyanFog, PinkFog, LimeFog };
int16_t frameBend=0;
uint16_t frameNear=4*night::Q;
neon::Flicker frameFlicker;
#if defined(NIGHT_DEVICE_PROFILE) || defined(NIGHT_CARD_CHECK)
constexpr unsigned SCRATCH_BYTES=3840,PROFILE_OFFSET=3712;
bool profileGlow=true;
#else
constexpr unsigned SCRATCH_BYTES=4096,PROFILE_OFFSET=3936;
#endif
// Shared by the mutually exclusive eyelid/glow and rollover passes.
alignas(4) uint8_t sceneScratch[SCRATCH_BYTES];
// Screen-clipped, leaf raster loops in SRAM. This renderer uses the whole
// framebuffer; avoiding a flash call for every outline pixel fits 60 Hz.
inline __attribute__((always_inline)) void pixel(int x,int y,unsigned color) {
    if(unsigned(x)>=128 || unsigned(y)>=128)return;
    uint8_t &b=gfx_fb[y*64+x/2];
    if(x&1)b=(b&15)|(color<<4);else b=(b&240)|color;
}
RAMFUNC(nightLine) void line(int x,int y,int endX,int endY,unsigned color) {
    int dx=endX>x?endX-x:x-endX,dy=endY>y?endY-y:y-endY;
    int sx=x<endX?1:-1,sy=y<endY?1:-1,err=dx-dy;
    for(;;) {
        pixel(x,y,color);if(x==endX && y==endY)break;
        int twice=err*2;if(twice>-dy) {err-=dy;x+=sx;}if(twice<dx) {err+=dx;y+=sy;}
    }
}
RAMFUNC(nightCircle) void circle(int cx,int cy,int radius,unsigned color) {
    int x=0,y=radius,d=3-2*radius;
    while(x<=y) {
        for(unsigned i=0;i<8;++i) {
            int dx=i&1?x:y,dy=i&1?y:x;
            pixel(cx+(i&2?-dx:dx),cy+(i&4?-dy:dy),color);
        }
        if(d<0)d+=4*x+6;else d+=4*(x-y--)+10;++x;
    }
}
void palette(unsigned p) {
    static unsigned applied=99;unsigned key=p;if(applied==key)return;applied=key;
    static const uint16_t tones[4][5]={
        {0x000,0x200,0x710,0xE43,0xFBA},
        {0x000,0x210,0x750,0xDB5,0xFDC},
        {0x000,0x012,0x135,0x48B,0xACF},
        {0x000,0x102,0x526,0xB6C,0xECD}
    };
    uint16_t colors[16]={0,0,0,0,0,0x4FD,0xF5A,0xCF6,0xFFF,0xFD7,0x154,0x512,0x351,0x021,0x201,0x120};
    for(unsigned i=0;i<5;++i)colors[i]=tones[p%4][i];
    pal::init(colors);pal::setCycling(false);
}
void center(int y,const char *s,unsigned c) {text35((128-text35Width(s))/2,y,s,c);}
void big(int y,const char *s,unsigned c,unsigned scale=2) {
    gfx_textScaled((128-gfx_textWidthScaled(s,scale))/2,y,s,c,scale);
}
int half(int y) {return 2+(y-44)*76/54;}
int middle(const night::Game &g,int y) {
    int far=98-y;return 64-g.position*half(y)/1024+frameBend*far*far/(54*54);
}
int projectQ(int z) {return 44+scenery::GROUND_SCALE*night::Q/(z+frameNear);}
int project(int z) {return projectQ(z*night::Q);}
void sky(const night::Game &g) {
    static uint32_t ridgeSeed=~uint32_t(0);
    static uint8_t ridge[43];
    if(ridgeSeed!=g.seed) {
        for(int i=0;i<43;++i) {
            int x=i*3;ridge[i]=40+night::wave(x*2+g.seed)/90+night::wave(x*5+67)/140;
        }
        ridgeSeed=g.seed;
    }
    for(int i=0;i<18;++i) {
        int x=(i*43+19)%124+2,y=(i*i*17+7)%34+5;
        pixel(x,y,i%6?Shade:Dim);
    }
    // A small moon, kept secondary to the vanishing point.
    circle(103,19,4,Dim);gfx_fillCircle(105,17,4,Black);
    int last=42;
    for(int x=0;x<128;x+=3) {
        int y=ridge[x/3];
        line(x-3,last,x,y,Shade);last=y;
    }
    gfx_hline(0,45,128,Shade);
}
void road(const night::Game &g) {
    int lastL=0,lastR=0,lastY=44;
    for(int y=44;y<=98;++y) {
        int c=middle(g,y),w=half(y),l=c-w,r=c+w;
        sceneScratch[y]=uint8_t(l<0?0:l>128?128:l);
        sceneScratch[128+y]=uint8_t(r<0?0:r>128?128:r);
        if(y>44) {
            line(lastL,lastY,l,y,y>57?Edge:Dim);
            line(lastR,lastY,r,y,y>57?Edge:Dim);
        }
        lastL=l;lastR=r;lastY=y;
    }
    int stripePhase=g.distance%(12*night::Q);
    for(int i=18;i>=0;--i) {
        int z=(i*12+12)*night::Q-stripePhase;
        int y=projectQ(z+3*night::Q),bottom=projectQ(z);
        if(y>97)continue;if(bottom>98)bottom=98;
        int length=bottom-y+1,c=middle(g,(y+bottom)/2),w=(bottom-44)/22+1;
        gfx_fillRect(c-w/2,y,w,length,y<53?Dim:Pale);
        // The wet asphalt catches short, broken glints, never a filled road.
        if(i%3==1)gfx_hline(c+half(y)/2,y+1,1+(y-44)/12,Shade);
    }
    // Sparse reflectors and broken grass replace the wireframe fence.
    for(int side=-1;side<=1;side+=2)for(int i=5;i>=0;--i) {
        int z=(i*43+43)*night::Q-int(g.distance%(43*night::Q)),y=projectQ(z);if(y>97)continue;
        int h=1+(y-44)/7,x=middle(g,y)+side*(half(y)+5);
        gfx_vline(x,y-h,h,Dim);pixel(x,y-h,Pale);
    }
    // Irregular low-contrast grit/grass patches are planted in the world,
    // so the near verge streams past instead of remaining fixed on glass.
    unsigned cell=g.distance/(6*night::Q);
    for(int i=12;i>=0;--i)for(int side=-1;side<=1;side+=2) {
        unsigned id=(cell+i)*173u+(side+1)*79u;
        int z=(i*6+3+int(id%4))*night::Q-int(g.distance%(6*night::Q));
        if(z<0)continue;int y=projectQ(z);if(y<49 || y>97)continue;
        int w=half(y),x=middle(g,y)+side*(w*(1110+id%270)/1024+1);
        pixel(x,y,Shade);pixel(x+side,y-1,Dim);
        if(y>69) {pixel(x-side,y,Dim);pixel(x+side,y-2,Shade);}
    }
}
void forest(const night::Game &g) {
    int cell=g.distance/(31*night::Q);
    const scenery::Road projection{g.position,frameBend,frameNear};
    // The previous cell remains alive until its actual tree has passed.
    // Dropping it at each 31 m boundary made nearby trunks pop away.
    for(int i=7;i>=-1;--i) {
        for(int side=-1;side<=1;side+=2) {
            if(g.screen==night::Title && side<0)continue; // quiet sky behind the left-aligned logo
            auto t=scenery::tree(g.distance,g.tick,projection,cell+i,side);
            if(!t.visible)continue;
            // A real roadside lot has room around it. Its clearing moves
            // through exactly the same world coordinates as the building.
            if(side==g.sign.side && night::absolute(t.depth-(int(g.sign.at)-int(g.distance)))<42*night::Q)continue;
            art::GroundClip ground{sceneScratch+(side>0?128:0),224,side>0};
            art::projected(art::Sprite(art::Pine+t.kind),t.variant,t.x,t.foot,t.height,t.wind,ground,t.mirror,t.height<13*night::Q);
            if(i<4 && t.id%3==0) {
                int lane=1550+t.id%160;
                int x=projection.center(t.foot)+side*projection.half(t.foot)*lane/1024;
                ground.from=0;
                art::projected(art::Shrub,0,x,t.foot,(t.foot-44*night::Q)/2+3*night::Q,0,ground,t.mirror,t.height<13*night::Q);
            }
        }
    }
}
struct Point {int x,y;};
Point world(const night::Game &g,int lane,int z,int height=0) {
    if(z<1)z=1;int y=project(z),w=half(y);
    return {middle(g,y)+lane*w/1024,y-height*w/(3*256)};
}
Point blend(Point a,Point b,int n,int denominator) {return {a.x+(b.x-a.x)*n/denominator,a.y+(b.y-a.y)*n/denominator};}
unsigned lightKind(const night::Game &g) {return g.sign.kind==1?1:g.sign.kind==2?2:0;}
unsigned lightInk(const night::Game &g) {return Cyan+lightKind(g);}
constexpr int SIGN_WIDTHS[]={1550,1800,1100,1650},SIGN_DEPTHS[]={6,5,3,7};
inline __attribute__((always_inline)) void hazePixel(int x,int y,unsigned kind,unsigned level) {
    if(unsigned(x)>=128 || unsigned(y)>=99 || !level)return;
    unsigned edge=sceneScratch[y];if(x>=int(edge) && x<128-int(edge))return;
    pixel(x,y,(level==1?Glow:CyanFog)+kind);
}
RAMFUNC(nightHaze) void hazeColumn(const HazeTexture &texture,int x,int top,int bottom,int col,unsigned kind) {
    int nativeH=texture.h-2,height=bottom-top,margin=(height+nativeH-1)/nativeH;
    int from=top-margin;if(from<0)from=0;
    int to=bottom+margin;if(to>99)to=99;
    for(int y=from;y<to;++y) {
        int edge=sceneScratch[y];if(x>=edge && x<128-edge)continue;
        int n=(y-top)*nativeH,row=(n>=0?n/height:-((-n+height-1)/height))+1;
        if(row<0 || row>=texture.h)continue;
        unsigned at=row*texture.w+col;
        unsigned level=(texture.pixels[at/4]>>((at&3)*2))&3;
        if(frameFlicker.contains(col-1,row-1))level=level==1?2:0;
        if(level)pixel(x,y,(level==1?Glow:CyanFog)+kind);
    }
}
void projectedHaze(const HazeTexture &texture,Point a,Point b,Point c,Point d,unsigned kind) {
    int width=b.x-a.x;if(width<=0 || b.x<0 || a.x>=128)return;
    int nativeW=texture.w-2,margin=(width+nativeW-1)/nativeW;
    int first=a.x-margin;if(first<0)first=0;
    int last=b.x+margin;if(last>128)last=128;
    for(int x=first;x<last;++x) {
        int n=(x-a.x)*nativeW,col=(n>=0?n/width:-((-n+width-1)/width))+1;
        int u=(x-a.x)*1024/width;
        int top=blend(a,b,u,1024).y,bottom=blend(d,c,u,1024).y;
        if(bottom<=top || col>=texture.w)continue;
        if(col<0)continue;
        hazeColumn(texture,x,top,bottom,col,kind);
    }
}
void roadLights(const night::Game &g,bool haze=false) {
    const int depth=haze?7:6;
    const unsigned arrows=night::arrowCount(g.sign.kind);const int first=g.arrowZ(0);
    for(unsigned i=0;i<arrows;++i) {
        int z=first+int(i)*night::ARROW_GAP;
        if((g.sign.collected&(1u<<i)) || z<0 || z>150)continue;
        int top=project(z+depth),bottom=project(z>depth?z-depth:1);
        if(top<46)top=46;if(bottom>98)bottom=98;
        for(int y=top;y<=bottom;++y) {
            int groundZ=scenery::GROUND_SCALE*night::Q/(y-44)-frameNear;
            int row=((z+6)*night::Q-groundZ)*13/(12*night::Q)+(haze?1:0);
            if(row<0 || row>=(haze?15:13))continue;
            uint32_t bits=haze?ARROW_HAZE_ROWS[row]:ROAD_ARROW_ROWS[row];
            int w=half(y),c=middle(g,y)+g.sign.side*night::ARROW_LANE*w/1024;
            int width=2*night::ARROW_HALF_WIDTH*w/1024;
            if(width<1)width=1;
            int left=c-width/2,edge=sceneScratch[y];
            if(haze && left>=edge && left+width<=128-edge)continue;
            int margin=haze?(width+8)/9:0;
            for(int x=-margin;x<width+margin;++x) {
                if(haze) {
                    int n=x*9,col=(n>=0?n/width:-((-n+width-1)/width))+1;
                    if(col>=0 && col<11)hazePixel(left+x,y,lightKind(g),(bits>>(col*2))&3);
                }
                else if(bits&(1u<<(x*9/width)))pixel(left+x,y,lightInk(g));
            }
        }
    }
}
void signCorners(const night::Game &g,Point &a,Point &b,Point &c,Point &d) {
    int side=g.sign.side,kind=g.sign.kind%4,z=g.signZ();
    int top=kind==2?64*256/10:kind==0?48*256/10:5*256;
    int bottom=kind==0?24*256/10:kind==1?25*256/10:22*256/10;
    int near=1200,far=near+SIGN_WIDTHS[kind],farZ=z+SIGN_DEPTHS[kind];
    a=world(g,side*near,z,top);b=world(g,side*far,farZ,top);
    d=world(g,side*near,z,bottom);c=world(g,side*far,farZ,bottom);
}
void neonHaze(const night::Game &g) {
    if(g.signZ()<0 || g.signZ()>235)return;
    Point a,b,c,d;signCorners(g,a,b,c,d);
    if(a.x>b.x) {Point p=a;a=b;b=p;p=d;d=c;c=p;}
    projectedHaze(HAZE_SIGNS[g.sign.kind%4],a,b,c,d,lightKind(g));
}
void quad(Point a,Point b,Point c,Point d,unsigned ink,bool fill=true) {
    if(fill) {
        Point points[]={a,b,c,d};int low=a.y,high=a.y;
        for(auto p:points) {if(p.y<low)low=p.y;if(p.y>high)high=p.y;}
        if(low<0)low=0;if(high>127)high=127;
        for(int y=low;y<=high;++y) {
            int xs[4],count=0;
            for(int i=0;i<4;++i) {
                Point p=points[i],q=points[(i+1)%4];
                if((p.y<=y && q.y>y)||(q.y<=y && p.y>y))xs[count++]=p.x+(q.x-p.x)*(y-p.y)/(q.y-p.y);
            }
            if(count>=2) {int l=xs[0],r=xs[0];for(int i=1;i<count;++i) {if(xs[i]<l)l=xs[i];if(xs[i]>r)r=xs[i];}gfx_hline(l,y,r-l+1,ink);}
        }
    } else {line(a.x,a.y,b.x,b.y,ink);line(b.x,b.y,c.x,c.y,ink);line(c.x,c.y,d.x,d.y,ink);line(d.x,d.y,a.x,a.y,ink);}
}
RAMFUNC(nightSign) void projectedSign(const SignTexture &texture,Point a,Point b,Point c,Point d,int shiftX,int shiftY,bool back,bool taken) {
    int width=b.x-a.x;if(width<=0 || b.x+shiftX<0 || a.x+shiftX>=128)return;
    int start=a.x;if(start+shiftX<0)start=-shiftX;
    int end=b.x;if(end+shiftX>128)end=128-shiftX;
    int xstep=texture.w*65536/width,sx=xstep/2+(start-a.x)*xstep;
    for(int x=start;x<end;++x,sx+=xstep) {
        int col=sx>>16,u=(x-a.x)*1024/width;
        int top=blend(a,b,u,1024).y,bottom=blend(d,c,u,1024).y;
        if(bottom<=top || col>=texture.w)continue;
        int startY=top;if(startY+shiftY<0)startY=-shiftY;
        int endY=bottom;if(endY+shiftY>99)endY=99-shiftY;
        int ystep=texture.h*65536/(bottom-top),sy=ystep/2+(startY-top)*ystep;
        for(int y=startY;y<endY;++y,sy+=ystep) {
            int row=sy>>16;if(row>=texture.h)continue;
            unsigned pos=row*texture.w+col,ink=(texture.pixels[pos/2]>>((pos&1)*4))&15;
            if(ink==15)continue;
            if(back)ink=ink?Dim:Shade;
            else if(ink>=5 && ink<=8 && frameFlicker.contains(col,row))ink=Glow+(texture.w==22?2:texture.h==28?1:0);
            else if(taken && ink>=5 && ink<=8)ink=Pale;
            pixel(x+shiftX,y+shiftY,ink);
        }
    }
}
void neon(const night::Game &g) {
    int z=g.signZ();if(z<-15 || z>235)return;
    int side=g.sign.side,kind=g.sign.kind%4;
    // Gravel turnout, access drive and a building all share the sign's
    // actual world position, parallax, depth scale and forest exclusion.
    int near=z-9;if(near<2)near=2;
    Point lotA=world(g,side*1050,near),lotB=world(g,side*3700,near+14);
    Point lotC=world(g,side*3700,z+48),lotD=world(g,side*1100,z+28);
    quad(lotA,lotB,lotC,lotD,Shade);
    line(lotA.x,lotA.y,lotB.x,lotB.y,Dim);
    if(z<120)for(int i=0;i<18;++i) {
        unsigned n=i*197+kind*73;Point p=world(g,side*(1200+n%2200),near+n%30);
        pixel(p.x,p.y,i%4?Shade:Dim);
    }
    int bz=z+26,by=project(bz),h=half(by)*4/3;
    Point building=world(g,side*2350,bz);
    art::draw(art::Sprite(art::Diner+kind),0,building.x,building.y,h,side<0);
    Point drive=world(g,side*1700,z+20);
    line(lotA.x,lotA.y,drive.x,drive.y,Dim);
    if(z<0)return;
    unsigned glow=Glow+lightKind(g);
    // World dimensions differ: oval diner, freestanding motel lettering,
    // a tall service-station blade, and a broad 24H shop sign.
    Point a,b,c,d;signCorners(g,a,b,c,d);
    for(int n=2;n<=8;n+=6) {
        Point top=blend(d,c,n,10),foot=world(g,side*(1200+SIGN_WIDTHS[kind]*n/10),z+SIGN_DEPTHS[kind]*n/10);
        line(top.x,top.y,foot.x,foot.y,Dim);
        if(z<65)line(top.x+side,top.y,foot.x+side,foot.y,Shade);
    }
    int thickness=1+half(project(z))/15;
    if(a.x>b.x) {Point p=a;a=b;b=p;p=d;d=c;c=p;}
    projectedSign(SIGN_TEXTURES[kind],a,b,c,d,side*thickness,1,true,false);
    projectedSign(SIGN_TEXTURES[kind],a,b,c,d,0,0,false,false);
    // Soft pool and broken road reflections are attached to the lit site.
    if(z<120)for(int i=0;i<4;++i) {
        Point p=world(g,side*(900+i*45),z/2+i*2);
        if(p.y<97)gfx_hline(p.x-2,p.y,3+half(p.y)/7,glow);
    }
}
void hazard(const night::Game &g) {
    int z=g.hazardZ();if(z<0 || z>190 || g.hazard.resolved)return;
    int y=project(z),depth=y-44,x=middle(g,y)+g.hazardX()*half(y)/1024;
    unsigned frame=g.tick/7;bool mirror=g.hazard.side>0;
    auto height=[](int h) {return h>105?105:h;};
    switch(g.hazard.kind) {
    case night::Deer: art::draw(art::Deer,frame,x,y,height(depth*3/4+3),mirror);break;
    case night::Fox: art::draw(art::Fox,frame,x,y,height(depth*2/5+3),mirror);break;
    case night::Hare: art::draw(art::Hare,frame,x,y,height(depth*2/5+2),mirror);break;
    case night::Owl: art::draw(art::Owl,g.tick/9,x,y-depth/3,height(depth/3+3),mirror);break;
    case night::Birds:
        for(int i=0;i<3;++i)art::draw(art::Crow,g.tick/6+i,x+(i-1)*depth/8,y-depth/4+(i%2)*depth/10,height(depth/5+2),mirror);
        break;
    default:art::draw(art::Branch,0,x,y,height(depth/3+3),mirror);break;
    }
}
void rain(const night::Game &g) {
    for(unsigned i=0;i<31;++i) {
        int y=9+(i*31+g.tick*(i%3+1)/3)%87,x=(i*47+g.tick/11)%128;
        if(i%3 || y<45)line(x,y,x-1,y+2+(i%3),i%7?Shade:Dim);
    }
}
unsigned root(unsigned n) {
    unsigned r=0,b=1u<<14;
    while(b>n)b>>=2;
    while(b) {if(n>=r+b) {n-=r+b;r=(r>>1)+b;}else r>>=1;b>>=2;}
    return r;
}
void eyelids(const night::Game &g) {
    unsigned t=g.tunnel();if(!t)return;
    int rx=84-int(t)*62/100,ry=64-int(t)*49/100;
    int cy=59+night::wave(g.tick/9)/160;
    for(int y=0;y<99;++y) {
        int dy=night::absolute(y-cy)*256/ry;
        int dx=dy>=256?0:rx*int(root(65536-dy*dy))/256;
        sceneScratch[y]=dx>=64?0:64-dx;
        if(dx>=65)continue;
        int l=64-dx,r=64+dx;
        gfx_hline(0,y,l,Black);gfx_hline(r,y,128-r,Black);
        if(dx>1 && y%2==0) {pixel(l,y,Shade);pixel(r-1,y,Shade);}
    }
}
int dashboardEdge(int x) {return x<=44?99-x*3/44:x<=84?96+(x-44)*3/40:99;}
void blinkShutter(const night::Game &g) {
    unsigned amount=g.blinkShutter();if(!amount)return;
    int cy=59+night::wave(g.tick/9)/160;
    int opening=(64-int(g.tunnel())*49/100)*int(256-amount)/256;
    for(int x=0;x<128;++x) {
        int edge=dashboardEdge(x),dx=x-64;
        // Curved upper and lower lids meet across the road. Clip them at
        // the cockpit's physical edge: instruments remain readable.
        int curve=dx*dx/256;
        int top=cy-opening+curve*int(amount)/256;
        int bottom=cy+opening-curve*int(amount)/256;
        if(bottom<top)bottom=top;
        if(top>edge)top=edge;
        gfx_vline(x,0,top,Black);
        gfx_vline(x,bottom,edge-bottom,Black);
    }
}
void arrowReceiver(const night::Game &g) {
    if(g.screen!=night::Drive && g.screen!=night::Paused)return;
    int nearest=10000;
    const unsigned count=night::arrowCount(g.sign.kind);
    const int first=g.arrowZ(0);if(first>48)return;
    for(unsigned i=0;i<count;++i) {
        int z=first+int(i)*night::ARROW_GAP;
        if(!(g.sign.collected&(1u<<i)) && z>=0 && z<nearest)nearest=z;
    }
    if(nearest>48)return;
    // The tail is three of the arrow's nine source columns. A whole tail
    // fitting between the ends corresponds to the actual +/-250 lane
    // window; the extra sixth of the full arrow width accounts for it.
    const int plane=94,w=half(plane);
    int tolerance=(night::ARROW_HALF_WIDTH*w+1023)/1024;
    int tailHalf=(2*night::ARROW_HALF_WIDTH*w/1024+5)/6;
    int radius=tolerance+tailHalf;
    unsigned ink=nearest>32?Dim:Edge;
    for(int x=64-radius;x<=64+radius;++x) {
        // Follow the cockpit's top edge, leaving a three-pixel air gap.
        int edge=dashboardEdge(x);
        for(int y=edge-10;y<=edge-3;++y) {
            if((x+y)&1)continue;
            unsigned existing=(gfx_fb[y*64+x/2]>>((x&1)*4))&15;
            if(existing<3)pixel(x,y,ink); // preserve the tested transparency
        }
    }
}
void smallMeter(int x,int y,unsigned value,unsigned ink) {
    if(value>100)value=100;
    circle(x,y,6,Dim);
    pixel(x-3,y+3,ink);pixel(x-4,y-1,ink);pixel(x,y-5,ink);
    pixel(x+4,y-1,ink);pixel(x+3,y+3,ink);
    unsigned a=96+value*192/100;
    line(x,y,x+night::wave(a+64)*4/256,y+night::wave(a)*4/256,ink);pixel(x,y,Pale);
}
void dashboard(const night::Game &g) {
    gfx_fillRect(0,99,128,29,Black);
    for(int x=0;x<128;++x) {
        int edge=dashboardEdge(x);
        for(int y=edge;y<99;++y)pixel(x,y,Black);
        pixel(x,edge,Edge);
    }
    // Cropped wheel and an analogue dial: cockpit, rather than HUD chrome.
    int turn=g.steering/12;
    circle(25,134,35,Edge);circle(25,134,32,Dim);
    // Top stitch plus two at +/-45 degrees on the cropped leather rim.
    // Shared centre/radii keep them clear of the gauge.
    for(int i=0;i<3;++i) {
        unsigned a=192+(i-1)*32+turn;int dx=night::wave(a+64),dy=night::wave(a);
        line(25+dx*34/256,134+dy*34/256,25+dx*33/256,134+dy*33/256,Pale);
    }
    circle(25,130,24,Dim);
    for(int i=0;i<7;++i) {
        unsigned a=136+i*18;int dx=night::wave(a+64),dy=night::wave(a);
        line(25+dx*19/256,130+dy*19/256,25+dx*22/256,130+dy*22/256,Edge);
    }
    unsigned angle=139+g.speed*96/120;
    line(25,130,25+night::wave(angle+64)*17/256,130+night::wave(angle)*17/256,Pale);
    char s[24];snprintf(s,sizeof s,"%02u",g.speed);text35(25-text35Width(s)/2,120,s,g.speed>100?Edge:Pale);
    text35(55,104,"KM/H",Dim);
    if(g.muted)text35(76,104,"M",Dim);
    unsigned sleep=g.sleep/100;
    smallMeter(88,106,sleep,sleep>=70?Edge:Pale);
    text35(104,101,"SLEEP",Dim);snprintf(s,sizeof s,"%u%%",sleep);text35(104,108,s,sleep>=70?Edge:Pale);
    smallMeter(88,121,g.damage,g.damage?Edge:Pale);
    text35(104,116,"DRIFT",g.damage?Edge:Dim);
    snprintf(s,sizeof s,"+%u%%",g.damage);text35(104,123,s,g.damage?Edge:Dim);
}
void trip(const night::Game &g) {
    unsigned clock=g.tick/60;
    char time[16];snprintf(time,sizeof time,"%u:%02u",clock/60,clock%60);
    gfx_fillRect(7,3,22,7,Black);text35(8,4,time,Dim);
    unsigned metres=g.distance/night::Q;
    char s[24];snprintf(s,sizeof s,"%u.%02u KM",metres/1000,(metres%1000)/10);
    gfx_fillRect(94,3,30,7,Black);text35(124-text35Width(s),4,s,Pale);
}
void top(const night::Game &g) {
    if(g.shock && g.screen==night::Drive) {
        char hit[24];snprintf(hit,sizeof hit,"AWAKE. DRIFT +%u%%",g.damage);
        gfx_fillRect(25,15,78,9,Black);center(17,hit,Pale);
    } else if(g.recovery && g.screen==night::Drive) {
        gfx_fillRect(29,15,70,9,Black);center(17,"LIGHT + 8% AWAKE",lightInk(g));
    } else if(g.offroad && g.screen==night::Drive) {
        gfx_fillRect(31,15,66,9,Black);center(17,"EASY. FIND THE ROAD.",Pale);
    } else if(g.speed>100 && g.screen==night::Drive) {
        gfx_fillRect(22,15,84,9,Black);center(17,"HEAVY FOOT. HOLD B.",Pale);
    } else if(g.tick<420 && g.screen==night::Drive) {
        gfx_fillRect(13,15,102,9,Black);center(17,"FOOT GETS HEAVY. HOLD B.",Dim);
    }
    if(g.hazard.warned && g.hazardZ()>0 && !g.hazard.resolved && g.screen==night::Drive) {
        char cue[28];
        if(g.hazard.kind==night::Branch)snprintf(cue,sizeof cue,"BRANCH: PASS WIDE");
        else {
            const char *name=g.hazard.kind==night::Deer?"DEER":g.hazard.kind==night::Fox?"FOX":g.hazard.kind==night::Hare?"HARE":g.hazard.kind==night::Owl?"OWL":"BIRDS";
            snprintf(cue,sizeof cue,"%s FROM %s",name,g.hazard.side>0?"RIGHT":"LEFT");
        }
        gfx_fillRect(28,86,72,8,Black);center(87,cue,Pale);
    }
}
void impactJolt(const night::Game &g) {
    if(!g.shock || g.screen!=night::Drive)return;
    unsigned elapsed=72-g.shock;
    static const int8_t offsets[][2]={{-3,2},{2,-1},{-2,1},{1,0},{-1,0},{0,0}};
    if(elapsed>=18)return;
    int dx=offsets[elapsed/3][0],dy=offsets[elapsed/3][1];
    int weight=night::obstacle(g.hazard.kind).damage+8;
    dx=(dx<0?-1:1)*(night::absolute(dx)*weight+18)/38;
    dy=(dy<0?-1:1)*(night::absolute(dy)*weight+18)/38;
    if(g.position>g.hazard.lane)dx=-dx;
    int first=dx>0?dx:0,last=dx<0?64+dx:64;
    for(int i=0;i<128;++i) {
        int y=dy>0?127-i:i,source=y-dy;uint8_t *row=gfx_fb+y*64;
        if(unsigned(source)>=128)memset(row,0,64);
        else {
            memmove(row+first,gfx_fb+source*64+first-dx,last-first);
            memset(row,0,first);memset(row+last,0,64-last);
        }
    }
}
void lightHaze(const night::Game &g) {
#ifdef NIGHT_DEVICE_PROFILE
    if(!profileGlow)return;
#endif
#ifdef NIGHT_PROFILE
    uint32_t began=micros();++nightProfile[23];
#endif
    neonHaze(g);roadLights(g,true);
#ifdef NIGHT_PROFILE
    uint32_t elapsed=micros()-began;nightProfile[26]+=elapsed;if(elapsed>nightProfile[27])nightProfile[27]=elapsed;
#endif
}
void scene(const night::Game &g,bool mask=true) {
    frameBend=g.bend();frameNear=scenery::nearDepth(g.speed);
    frameFlicker=neon::flicker(g.sign.at,g.seed,g.tick,g.sign.kind);
#ifdef NIGHT_PROFILE
    ++nightProfile[0];
#endif
    ART_STAGE(0,gfx_clear(Black));ART_STAGE(1,sky(g));ART_STAGE(2,road(g);roadLights(g));
    ART_STAGE(3,forest(g));ART_STAGE(4,neon(g));ART_STAGE(5,rain(g));ART_STAGE(6,hazard(g));
    ART_STAGE(7,if(mask) {eyelids(g);if(g.tunnel()) {ART_STAGE(9,lightHaze(g));}});
    ART_STAGE(8,arrowReceiver(g);dashboard(g));impactJolt(g);
}
inline __attribute__((always_inline)) unsigned rollPixel(const uint8_t *source,int x,int y) {
    x>>=8;y>>=8;if(unsigned(x)>=128 || unsigned(y)>=128)return Black;
    unsigned ink=(source[y*32+x/4]>>((x&3)*2))&3;
    return (0x4320u>>(ink*4))&15; // black, dim, edge, pale
}
RAMFUNC(nightRoll) void roll(const night::RolloverPose &pose,bool capture) {
    // Preserve every source pixel's position. Four dominant night inks
    // pack the 128x128 frame into the Rev0's available 4 KB scratch space.
    uint8_t *snapshot=sceneScratch;
    constexpr uint32_t inks=0x005BFF90;
    if(capture)for(unsigned i=0;i<sizeof sceneScratch;++i) {
        unsigned a=gfx_fb[i*2],b=gfx_fb[i*2+1];
        snapshot[i]=((inks>>((a&15)*2))&3)|(((inks>>((a>>4)*2))&3)<<2)
            |(((inks>>((b&15)*2))&3)<<4)|(((inks>>((b>>4)*2))&3)<<6);
    }
    if(pose.angle==0 && pose.zoom==256)return;
    int c=night::wave(pose.angle+64)*256/pose.zoom,s=night::wave(pose.angle)*256/pose.zoom;
    for(int y=0;y<128;++y) {
        int sx=(64+pose.x)*256-64*c+(y-64)*s,sy=(64+pose.y)*256+64*s+(y-64)*c;
        for(int x=0;x<64;++x) {
            unsigned lo=rollPixel(snapshot,sx,sy);sx+=c;sy-=s;
            unsigned hi=rollPixel(snapshot,sx,sy);sx+=c;sy-=s;
            gfx_fb[y*64+x]=lo|(hi<<4);
        }
    }
}
void panel(int y,int h) {gfx_fillRect(8,y,112,h,Black);gfx_rect(8,y,112,h,Dim);gfx_hline(14,y,22,Edge);}
void title(const night::Game &g) {
    // Parked composition: rain and foliage move, the road and its stop
    // stay together. A sign must not float beside scrolling scenery.
    night::Game demo=g;demo.distance=0;demo.position=0;
    demo.sign.at=demo.distance+65*night::Q;demo.sign.side=1;demo.sign.kind=0;
    demo.hazard.at=demo.distance+1000*night::Q;demo.sleep=1100;
    scene(demo,false);
    text35(8,5,"A NIGHT DRIVE",Dim);
    // Letter-shaped shadows preserve scenery and the complete sign to
    // the right. A rectangular title matte would slice the roadside lot.
    for(int offset=-1;offset<=1;++offset) {
        gfx_textScaled(8+offset,15,"LIGHT",Black,2);
        gfx_textScaled(8,15+offset,"LIGHT",Black,2);
        gfx_textScaled(8+offset,33,"DRIVE",Black,2);
        gfx_textScaled(8,33+offset,"DRIVE",Black,2);
    }
    gfx_textScaled(8,15,"LIGHT",Pale,2);gfx_textScaled(8,33,"DRIVE",Edge,2);
    gfx_fillRect(20,68,88,18,Black);center(71,"A / KEEP DRIVING",Pale);center(80,"B / HOW TO STAY AWAKE",Dim);
    gfx_fillRect(0,99,128,29,Black);gfx_hline(8,99,112,Dim);
    center(104,night::paletteName(g.palette),Edge);
    text35(13,104,"<",Dim);text35(111,104,">",Dim);
    center(114,g.muted?"UP: SOUND OFF":"UP: SOFT SOUND ON",Dim);
    center(122,g.haptics?"DOWN: RUMBLE ON":"DOWN: RUMBLE OFF",Dim);
}
void help() {
    gfx_clear(Black);center(8,"KEEP A LITTLE LIGHT",Edge);
    const char *lines[]={"LEFT / RIGHT: STEER","B: EASE OFF THE GAS","","SLEEP CLOSES YOUR VIEW.","IT ALSO MAKES YOU DRIFT.","TOO TIRED? BLACK BLINKS.","FOLLOW THE NEON ARROWS.","TAIL BETWEEN GUIDE ENDS.","EACH ARROW: 8% LESS SLEEP.","STAY BETWEEN THE EDGES.","","WILDLIFE + BRANCHES.","HITS ADD LASTING DRIFT.","LIGHT OR A HIT WAKES YOU.","SELECT: PALETTE","START: PAUSE"};
    for(unsigned i=0;i<16;++i)center(20+i*6,lines[i],i==6||i==7||i==8?Cyan:i<2?Pale:Dim);
    center(115,"UP: SOUND   DOWN: RUMBLE",Dim);
    center(123,"A / B: BACK",Pale);
}
}
void draw(const night::Game &g) {
    static uint16_t sourceFrame=0xFFFF;
    palette(g.palette);
    // A shut windshield needs no scenery pass. Keep the cockpit visible;
    // updates, contacts and pickups still run and can wake us mid-blink.
    if(g.blinkClosed()) {gfx_clear(Black);dashboard(g);trip(g);sourceFrame=0xFFFF;return;}
    if(g.screen==night::Title) {title(g);return;}
    if(g.screen==night::Help) {help();return;}
#if defined(NIGHT_DEVICE_PROFILE) || defined(NIGHT_CARD_CHECK)
    // The temporary USB timing build borrows 256 bytes from the unused
    // rollover snapshot. Its driving renderer and hot raster loops stay
    // unchanged; a loss screen skips affine sampling in this build only.
    if(g.screen==night::Tumble || g.tumble) {scene(g,false);return;}
#else
    if(g.screen==night::Tumble || g.tumble) {
        const auto pose=night::rolloverPose(g.tumble,g.speed,g.position<0?-1:1);
        night::Game camera=g;camera.distance+=pose.travel;camera.tick+=g.tumble;
        camera.position+=pose.lateral;
        // Keep the full-resolution affine motion each frame. The slower
        // moving scenery is refreshed at 30 Hz and stays cached at rest.
        bool capture=sourceFrame==0xFFFF || !g.tumble ||
            (g.screen==night::Tumble && !(g.tumble&1)) ||
            (g.screen==night::Rest && sourceFrame!=g.tumble);
        if(capture) {scene(camera,false);sourceFrame=g.tumble;}
        roll(pose,capture);
    } else
#endif
    {scene(g);sourceFrame=0xFFFF;}
    if(g.screen==night::Tumble)return;
    top(g);
    blinkShutter(g);
    trip(g);
    if(g.screen==night::Paused) {
        panel(29,55);center(37,"PARKED IN THE MOMENT",Pale);
        center(49,"START: KEEP DRIVING",Edge);center(59,"B: END DRIVE",Dim);
        center(67,"SELECT: COLOR   UP: SOUND",Dim);
        center(75,g.haptics?"DOWN: RUMBLE ON":"DOWN: RUMBLE OFF",Dim);
    } else if(g.screen==night::Rest) {
        panel(17,83);center(23,g.tumble?"TAKE A BREATH":"REST A WHILE",Pale);
        center(34,"THAT WOKE YOU UP.",Dim);
        char s[32];unsigned m=g.distance/night::Q;
        snprintf(s,sizeof s,"DISTANCE %u.%02u KM",m/1000,(m%1000)/10);center(45,s,Edge);
        snprintf(s,sizeof s,"WAKE-UPS %u",g.lights);center(55,s,Pale);
        snprintf(s,sizeof s,"ANIMALS AVOIDED %u",g.avoided);center(65,s,Pale);
        m=g.best/night::Q;snprintf(s,sizeof s,"BEST %u.%02u KM",m/1000,(m%1000)/10);center(75,s,Dim);
        center(84,cardscore::status==cardscore::Saved || cardscore::status==cardscore::Ready?"BEST KEPT ON SD":"BEST KEPT ON CONSOLE",Dim);
        center(94,"A: DRIVE AGAIN   B: HOME",Pale);
    }
}
}

#ifdef NIGHT_PROFILE
// QA counters occupy unused glow scratch space. Rollover reuses the whole
// buffer, so these counters are only for staged driving performance runs.
volatile uint32_t *nightProfileData() {return reinterpret_cast<volatile uint32_t *>(view::sceneScratch+view::PROFILE_OFFSET);}
#ifdef NIGHT_DEVICE_PROFILE
void nightProfileReset(bool glow) {
    view::profileGlow=glow;
    memset(view::sceneScratch+view::PROFILE_OFFSET,0,30*sizeof(uint32_t));
}
#endif
#endif
