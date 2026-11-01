#include "Scenery.h"
#include "Game.h"

namespace scenery {
int nearDepth(unsigned speed) {
    if(speed<=62)return 4*night::Q;
    if(speed>118)speed=118;
    return 4*night::Q-int(speed-62)*2*night::Q/56;
}
int Road::half(int y) const {return 2*night::Q+(y-44*night::Q)*76/54;}
int Road::center(int y) const {
    int far=98*night::Q-y,curve=far*far/(54*54);
    return 64*night::Q-position*half(y)/1024+bend*curve/night::Q;
}
Tree tree(uint32_t distance,uint32_t tick,const Road &road,int cell,int side) {
    Tree t;t.id=uint32_t(cell)*73u+23u+(side+1)*137u;
    t.depth=(cell*31+31+int(t.id%13)-6)*night::Q-int(distance);
    if(t.depth<-4*night::Q || t.depth>275*night::Q || t.depth+road.near<2*night::Q)return t;
    unsigned shape=t.id;shape^=shape<<13;shape^=shape>>17;shape^=shape<<5;
    t.kind=shape%7<3?0:shape%7<5?1:2;
    t.variant=(shape>>8)%TREE_VARIANTS;t.mirror=(shape>>6)&1;
    t.foot=44*night::Q+(GROUND_SCALE*night::Q*night::Q)/(t.depth+road.near);
    t.height=(t.foot-44*night::Q)*(t.kind==2?3:4);
    // Stable stature and drawing belong to this world cell, not the
    // screen frame. No silhouette swaps or stretched animated trunks.
    t.height=t.height*int(224+(shape>>16)%65)/256;
    // Keep crown, trunk and ground in the same perspective. A capped
    // height lets the approaching root drop while the tree stops growing,
    // making its crown fall back into view during an off-road rollover.
    if(t.foot-t.height>=99*night::Q)return t;
    t.visible=true;
    int lane=1650+t.id%900;
    t.x=road.center(t.foot)+side*road.half(t.foot)*lane/1024;
    // Sub-pixel flutter on tiny distant crowns reads as shimmer. Let the
    // wind emerge gently only as the drawing becomes large enough.
    if(t.height>48*night::Q)t.wind=night::wave(tick/2+t.id*19)*(t.height-48*night::Q)/(256*96);
    return t;
}
}
