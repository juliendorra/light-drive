#include "../../Neon.h"
#include <cassert>
#include <set>
#include <cstdio>
int main() {
    for(unsigned kind=0;kind<4;++kind) {
        unsigned dim=0,longest=0,run=0;std::set<unsigned> intervals,zones;int previous=-1;
        for(unsigned tick=0;tick<36000;++tick) {
            auto f=neon::flicker(650*256,937,tick,kind);
            if(f.left!=255) {
                ++dim;++run;zones.insert(f.left+f.top*64);
                assert(f.right>f.left && f.bottom>f.top && f.right-f.left<38);
                assert(f.contains(f.left,f.top) && !f.contains(f.right,f.bottom));
                if(run==1) {if(previous>=0)intervals.insert(tick-previous);previous=tick;}
            } else {if(run>longest)longest=run;run=0;}
        }
        assert(dim>50 && dim<1800 && longest<=6 && intervals.size()>10 && zones.size()>=3);
    }
    puts("PASS: sparse irregular letter/zone flicker, bounded short bursts, varied intervals and local source-space bounds");
}
