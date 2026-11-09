// Feasibility sweep: an instructed, awake-to-tired driver brakes, waits
// 400 ms, then uses 133 ms sampled steering with a noisy position estimate.
// This is a path/collider check; the separate human model tests visibility.
#include "../../Game.h"
#include <cstdio>
#include <cassert>
#include <initializer_list>
using namespace night;
int main() {
    unsigned counts[6]={},passes[6]={},managed[6]={},managedPass[6]={};
    for(unsigned kind=0;kind<CREATURE_COUNT;++kind)for(unsigned seed=1;seed<=8;++seed)
    for(int side:{-1,1})for(int speed:{62,80,100,118})for(int start:{-700,0,700})
    for(int lane:{-600,0,600})for(int fatigue:{1800,8500}) {
        ++counts[kind];bool feasible=false;bool ordinary=speed<=100 && fatigue==1800;
        if(ordinary)++managed[kind];
        // Primary path is behind the animal's tail (the entry road half).
        // Braking can also allow a complete crossing while holding centre.
        for(int path=0;path<4 && !feasible;++path) {
            Game g;g.begin(seed*937);g.distance=1200*Q;g.position=start;g.sleep=fatigue;g.speed=speed;
            g.sign.at=g.distance+1500*Q;g.hazard.kind=kind;g.hazard.side=side;g.hazard.lane=lane;
            g.hazard.at=g.distance+(unsigned(speed)*Q/216)*obstacle(kind).leadFrames;
            unsigned keys=0;
            for(unsigned f=0;f<900 && g.screen==Drive && !g.hazard.resolved;++f) {
                if(f>=24 && f%8==0) {
                    int target=kind==Branch?(lane>0?-650:lane<0?650:start>=0?650:-650):path==0?side*600:path==1?0:path==2?-side*600:start;
                    int noise=int((seed*13+f*7)%81)-40;
                    int estimate=g.position+noise+g.steering*14/16;
                    keys=Brake;
                    if(estimate<target-45)keys|=Right;else if(estimate>target+45)keys|=Left;
                }
                g.update(keys);
            }
            feasible=g.hazard.resolved && !g.incidents && g.screen==Drive;
        }
        if(feasible) {++passes[kind];if(ordinary)++managedPass[kind];}
    }
    printf("{\"reaction_ms\":400,\"decision_ms\":133,\"position_noise\":40,\"damage\":0,\"species\":[");
    for(unsigned k=0;k<CREATURE_COUNT;++k) {
        printf("%s{\"kind\":%u,\"cases\":%u,\"feasible\":%u,\"managed_awake_cases\":%u,\"managed_awake_feasible\":%u}",k?",":"",k,counts[k],passes[k],managed[k],managedPass[k]);
        assert(managedPass[k]==managed[k] && passes[k]*100>=counts[k]*97);
    }
    puts("]}");
}
