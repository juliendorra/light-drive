#include "../../Game.h"
#include <assert.h>
#include <stdio.h>
#include <initializer_list>
using namespace night;
static void tap(Game &g,uint8_t k) {g.update(k);g.update(0);}
static void contact(Game &g) {g.hazard.warned=true;g.hazard.age=g.hazard.kind==Branch?0:1600*60/obstacle(g.hazard.kind).crossingSpeed;}
static void safe(Game &g) {g.hazard.at=g.distance+1000*Q;g.position=0;}
int main() {
    assert(wave(0)==0 && wave(64)==256 && wave(128)==0 && wave(192)==-256);
    Game g;tap(g,Right);assert(g.palette==1);tap(g,Brake);assert(g.screen==Help);
    tap(g,Confirm);assert(g.screen==Title);tap(g,Sound);assert(g.muted);
    tap(g,Confirm);assert(g.screen==Drive && g.palette==1 && g.muted);
    for(int i=0;i<90;++i)g.update(0);
    assert(g.sleep>1100 && g.distance>20*Q);
    tap(g,Start);assert(g.screen==Paused);auto t=g.tick,dist=g.distance;auto sleep=g.sleep;
    for(int i=0;i<600;++i)g.update(0);
    assert(g.tick==t && g.distance==dist && g.sleep==sleep);
    tap(g,Start);assert(g.screen==Drive);
    assert(g.speed>=START_SPEED);
    for(int i=0;i<160;++i)g.update(Brake);assert(g.speed==MIN_SPEED);
    int pos=g.position;for(int i=0;i<30;++i)g.update(Right);assert(g.position>pos+150);
    Game awake,tired;awake.begin();tired.begin();tired.sleep=8000;
    for(int i=0;i<120;++i) {awake.position=tired.position=0;awake.update(0);tired.update(0);}
    assert(awake.speed>START_SPEED && tired.speed>awake.speed+2);
    int beforeBrake=tired.speed;for(int i=0;i<40;++i) {tired.position=0;tired.update(Brake);}
    assert(tired.speed<beforeBrake-8);beforeBrake=tired.speed;tired.update(0);assert(tired.speed==beforeBrake);
    // An unattended pedal reaches 130 km/h, stays capped there, and still
    // responds to B. Isolate this from collisions and road departures.
    Game capped;capped.begin();assert(capped.speed==62 && MAX_SPEED==130);
    for(unsigned frame=0;frame<10800;++frame) {
        safe(capped);capped.sign.at=capped.distance+1000*Q;capped.update(0);
        assert(capped.screen==Drive && capped.speed<=130);
        if(frame>=7200)assert(capped.speed==130);
    }
    for(unsigned frame=0;frame<8;++frame) {safe(capped);capped.update(Brake);}
    assert(capped.speed==128);
    // Every world-space marker restores sleep without A, once, in its lane.
    for(unsigned kind=0;kind<4;++kind)for(int side : {-1,1}) {
        g.begin();g.sign.at=120*Q;g.sign.kind=kind;g.sign.side=side;g.sleep=8500;
        for(unsigned arrow=0;arrow<arrowCount(kind);++arrow) {
            g.distance=g.sign.at+arrowOffset(kind,arrow)*Q-1;
            g.position=side*ARROW_LANE;sleep=g.sleep;g.update(0);
            assert((g.sign.collected&(1u<<arrow)) && (g.events&Neon));
            assert(g.sleep<=sleep-ARROW_RELIEF+3 && g.lights==arrow+1);
            sleep=g.sleep;g.update(Confirm);assert(!(g.events&Neon) && g.sleep>=sleep);
        }
        assert(g.sign.collected==(1u<<arrowCount(kind))-1);
        for(int position : {0,-side*ARROW_LANE,side*950}) {
            g.begin();g.sign.at=120*Q;g.sign.side=side;g.sleep=6000;
            g.distance=g.sign.at+arrowOffset(g.sign.kind,0)*Q-1;g.position=position;
            g.update(Confirm);assert(!g.sign.collected && !(g.events&Neon));
        }
    }
    g.begin();g.sign.at=120*Q;g.sign.kind=1;g.distance=g.sign.at+45*Q;g.update(0);
    assert(g.sign.at==120*Q); // retain the set until its arrows after the sign pass
    g.distance=g.sign.at+74*Q;g.update(0);assert(g.sign.at>120*Q && !g.sign.collected);
    // Continuous shoulder exposure has a forgiving three-second window.
    g.begin();for(int i=0;i<179;++i) {g.position=1300;g.update(0);}assert(g.screen==Drive);
    g.position=1300;g.update(0);assert(g.screen==Tumble && g.reason==Road && g.tunnel()==0);
    for(unsigned i=0;i<ROLLOVER_FRAMES-1;++i)g.update(0);assert(g.screen==Tumble);
    g.update(0);assert(g.screen==Rest && g.tumble==ROLLOVER_FRAMES);
    g.begin();g.offroad=80;g.update(0);assert(g.offroad==77);
    g.sleep=9999;g.update(0);assert(g.screen==Drive && g.sleep==10000 && g.tunnel()==100);
    // Hazards resolve once at the same distance for rendering and collision.
    g.begin();g.hazard.at=10*Q;g.hazard.lane=0;g.distance=g.hazard.at-1;contact(g);g.update(0);
    assert(g.incidents==1 && g.hazard.resolved && (g.events&Impact));
    assert(g.speed==MIN_SPEED && g.shock==72 && g.sleep==0 && g.damage==obstacle(Deer).damage && g.tunnel()==0);
    for(unsigned i=0;i<72;++i)g.update(0);assert(!g.shock && g.incidents==1);
    g.update(0);assert(g.incidents==1);
    g.begin();g.hazard.at=10*Q;g.hazard.lane=600;g.distance=g.hazard.at-1;contact(g);g.update(0);
    assert(g.incidents==0 && (g.events&Pass));
    for(unsigned kind=0;kind<CREATURE_COUNT;++kind) {
        g.begin();g.hazard.kind=kind;g.hazard.at=10*Q;g.distance=g.hazard.at-1;g.hazard.lane=0;
        contact(g);g.sleep=8000;g.speed=100;g.update(0);assert(g.incidents==1 && (g.events&Impact));
        assert(g.sleep==0 && g.tunnel()==0 && g.speed==100-obstacle(kind).speedLoss && g.damage==obstacle(kind).damage);
        g.begin();g.hazard.kind=kind;g.hazard.at=10*Q;g.distance=g.hazard.at-1;g.hazard.lane=-600;
        contact(g);g.update(0);assert(!g.incidents && (g.events&Pass));
    }
    // Repeated impacts accumulate handling damage, with no hit-count loss.
    g.begin();g.sleep=8000;
    for(unsigned hit=0;hit<6;++hit) {
        g.position=0;g.hazard.kind=Deer;g.hazard.lane=0;
        g.hazard.at=g.distance+1;g.hazard.resolved=false;contact(g);g.update(0);
        assert(g.screen==Drive && g.incidents==hit+1 && (g.events&Impact));
        assert(!g.sleep && g.damage==(hit<3?(hit+1)*30:100));
    }
    // The diagnostic encounter count and handling effect saturate safely
    // during an endless run; neither wraps nor becomes a hidden life limit.
    g.incidents=255;g.position=0;g.hazard.at=g.distance+1;
    g.hazard.resolved=false;contact(g);g.update(0);
    assert(g.screen==Drive && g.incidents==255 && g.damage==100);
    // Handling damage amplifies both curve force and the pull away from centre.
    g.begin();g.speed=80;int original=g.curveForce();g.damage=30;
    assert(g.curveForce()==original+original*30/100);
    g.begin();for(unsigned m=0;g.bend()!=0 && m<4000;++m)g.distance=(m+1)*Q;
    assert(g.bend()==0);
    for(int side:{-1,1}) {Game clean=g,damaged=g;clean.position=damaged.position=side*700;damaged.damage=100;for(unsigned frame=0;frame<4;++frame) {clean.update(0);damaged.update(0);}assert(side*damaged.position>side*clean.position);}
    // Animals cross on their own clock even when the car is braking.
    Game fast,slow;fast.begin();slow.begin();fast.hazard.warned=slow.hazard.warned=true;
    fast.hazard.age=slow.hazard.age=30;slow.speed=MIN_SPEED;fast.speed=MAX_SPEED;
    for(unsigned frame=0;frame<30;++frame) {fast.position=slow.position=0;fast.update(0);slow.update(Brake);}
    assert(fast.hazard.age==slow.hazard.age && fast.hazardX()==slow.hazardX() && fast.distance>slow.distance);
    g.begin();safe(g);g.distance=4000*Q-1;g.sleep=1000;g.update(0);assert(g.screen==Drive && g.distance>=4000*Q);
    g.distance=12000*Q;g.sleep=9999;g.position=1300;g.offroad=179;g.update(0);
    assert(g.screen==Tumble && g.best>12000*Q);
    auto best=g.best;g.begin();assert(g.best==best && !g.lights && !g.avoided && !g.damage);
    // Bounded, delayed-observation steering across multiple full routes.
    unsigned successes=0,lights=0;
    for(unsigned seed=1;seed<=32;++seed) {
        Game p;p.begin(seed*937);uint8_t input=0;bool ease=false;
        for(unsigned f=0;f<20000 && p.screen==Drive && p.distance<4000*Q;++f) {
            if(f%8==0) {
                int target=p.sign.side*380;
                if(p.hazardZ()<65 && p.hazardZ()>-5)target=p.hazard.lane>0?-650:650;
                input=0;
                if(p.speed>88)ease=true;if(p.speed<73)ease=false;
                if(ease)input|=Brake;
                if(p.position<target-55)input|=Right;
                else if(p.position>target+55)input|=Left;
            }
            p.update(input);
            assert(p.position>=-1600 && p.position<=1600 && p.sleep<=10000);
            assert(p.speed>=MIN_SPEED && p.speed<=MAX_SPEED && p.sign.collected<=63);
        }
        if(p.distance>=4000*Q)++successes;
        lights+=p.lights;
    }
    printf("Full routes: %u/32 survive the 4 km benchmark (game continues); %u neon recoveries (automated pilots, not human playtests).\n",successes,lights);
    assert(successes>=8);
    // Doing nothing eventually loses the road; sleep alone never ends it.
    g.begin();for(int i=0;i<15000 && g.screen==Drive;++i)g.update(0);
    assert(g.screen==Rest || g.screen==Tumble);
    while(g.screen==Tumble)g.update(0);assert(g.screen==Rest);
    puts("PASS: sleep/recovery, site-sized automatic lane arrows, pause, controls, shoulder grace, hazards, endless distance and bounded full-route pilots");
}
