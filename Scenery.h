#pragma once
#include <stdint.h>

namespace scenery {
// Wider ground perspective: at the same screen depth the optical flow is
// 2.5x the old 900 px*m lens, without multiplying distance or game speed.
constexpr int GROUND_SCALE=360;
constexpr unsigned TREE_VARIANTS=3;
int nearDepth(unsigned speed); // Q8; stronger foreground flow at high speed
// Screen coordinates retain eight fractional bits through placement and
// sprite scaling, rather than rounding distance to whole metres first.
struct Road {
    int position,bend,near=4*256;
    int half(int y) const;
    int center(int y) const;
};
struct Tree {
    int depth=0,x=0,foot=0,height=0,wind=0,kind=0,variant=0;
    unsigned id=0;
    bool visible=false,mirror=false;
};
Tree tree(uint32_t distance,uint32_t tick,const Road &road,int cell,int side);
}
