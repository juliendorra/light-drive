#include "../../Scenery.h"
#include "../../Game.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
using namespace scenery;
int main() {
    assert(nearDepth(62)==4*night::Q && nearDepth(118)==2*night::Q);
    for(unsigned speed=63;speed<=night::MAX_SPEED;++speed) {
        if(speed<=118)assert(nearDepth(speed)<nearDepth(speed-1));
        else assert(nearDepth(speed)==nearDepth(speed-1));
        assert(nearDepth(speed)>=2*night::Q);
        assert(nearDepth(speed-1)-nearDepth(speed)<=10);
        // Sweep the near plane at every speed, including trees just behind
        // the camera. Projection must stay finite and roots stay outside.
        Road fastRoad{900,-25,nearDepth(speed)};
        for(unsigned q=400*night::Q;q<440*night::Q;q+=31)for(int side:{-1,1}) {
            auto t=tree(q,1200,fastRoad,13,side);
            if(t.visible) {
                assert(t.height>0 && t.height<1024*night::Q);
                assert(t.foot<=(44+GROUND_SCALE/2)*night::Q);
                assert(side*(t.x-fastRoad.center(t.foot))>fastRoad.half(t.foot));
            }
        }
    }
    Road road{0,12};unsigned seen[3][TREE_VARIANTS]={};
    for(int side:{-1,1})for(int cell=-1;cell<130;++cell) {
        uint32_t distance=cell<0?0:uint32_t(cell*31)*night::Q;
        auto a=tree(distance,1200,road,cell,side),b=tree(distance+80,1201,road,cell,side);
        if(a.visible) {assert(a.kind>=0 && a.kind<3 && a.variant>=0 && a.variant<int(TREE_VARIANTS));++seen[a.kind][a.variant];}
        if(a.visible && b.visible) {
            assert(a.id==b.id && a.kind==b.kind && a.variant==b.variant && a.mirror==b.mirror);
            assert(a.depth+int(distance)==b.depth+int(distance+80));
            assert(b.foot>a.foot && b.height>=a.height);
            assert(b.height>a.height && b.foot-b.height<=a.foot-a.height);
            assert(side*(a.x-road.center(a.foot))>road.half(a.foot));
        }
    }
    for(auto &species:seen)for(unsigned count:species)assert(count>=8);
    // After leaving the road the camera can align with a nearby trunk.
    // Its crown must keep moving up as its root approaches, rather than
    // turning down once a screen-height cap is reached.
    for(unsigned speed:{42u,62u,118u,unsigned(night::MAX_SPEED)})for(int side:{-1,1})for(int cell=13;cell<42;++cell) {
        Road close{0,0,nearDepth(speed)};
        unsigned start=cell*31*night::Q;auto sample=tree(start,1200,close,cell,side);
        unsigned world=start+sample.depth;Tree previous;
        for(unsigned distance=world-24*night::Q;distance<=world+night::Q;distance+=32) {
            auto t=tree(distance,1200,close,cell,side);
            if(previous.visible && t.visible)assert(t.foot-t.height<=previous.foot-previous.height);
            previous=t;
        }
    }
    // The same absolute cell survives a metre/grid boundary, including
    // its species, root anchor, wind phase and smooth fractional scale.
    for(int side:{-1,1}) {
        int cell=13;auto a=tree(434*night::Q-1,1200,road,cell,side),b=tree(434*night::Q,1200,road,cell,side);
        assert(a.visible==b.visible && a.id==b.id && a.kind==b.kind && a.variant==b.variant && a.mirror==b.mirror);
        if(a.visible)assert(night::absolute(b.foot-a.foot)<night::Q && night::absolute(b.height-a.height)<night::Q);
    }
    puts("PASS: stable world cells, fractional perspective without metre stalls, roadside anchors and continuous boundary placement");
}
