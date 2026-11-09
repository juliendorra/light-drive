#include "../../Game.h"
#include <cassert>
#include <cstdio>
using namespace night;
static void tired(Game &g) {
    g.begin();g.sleep=10000;g.sign.at=g.hazard.at=1000000u*Q;
}
int main() {
    Game g;tired(g);unsigned starts=0,previousStart=0,previousGap=1000,run=0,maxRun=0;
    bool closed=false;
    for(unsigned f=0;f<3000;++f) {
        g.position=0;auto before=g.distance;g.update(0);
        assert(g.screen==Drive && g.distance>before && g.sleep==10000 && g.tunnel()==100);
        assert(g.drowsy<=BLINK_RAMP && g.blinkPhase<g.blinkPeriod());
        bool now=g.blinkClosed();
        if(now && !closed) {
            if(starts) {unsigned gap=f-previousStart;assert(gap<=previousGap && gap>=72);previousGap=gap;}
            previousStart=f;++starts;
        }
        if(now)++run;else {if(run>maxRun)maxRun=run;run=0;}
        closed=now;
    }
    assert(starts>25 && previousGap==72 && maxRun==18);
    assert(g.blinkPeriod()==72 && g.blinkDuration()==18);
    // Exact 60 Hz envelope: three closing frames, eighteen closed, four
    // reopening; no extra state and no discontinuity at the cycle wrap.
    tired(g);g.drowsy=BLINK_RAMP;
    unsigned transition=0,fullyClosed=0;
    const unsigned expected[]={64,128,192,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,204,153,102,51};
    for(unsigned phase=0;phase<72;++phase) {
        g.blinkPhase=phase;unsigned amount=g.blinkShutter();
        assert(amount==(phase<47?0:expected[phase-47]));
        assert(g.blinkClosed()==(amount==256));
        if(amount && amount<256)++transition;
        if(amount==256)++fullyClosed;
    }
    assert(transition==7 && fullyClosed==18);
    // Steering, braking and distance all remain live while eyes are shut.
    tired(g);g.drowsy=BLINK_RAMP;g.blinkPhase=55;g.speed=90;
    assert(g.blinkClosed());auto before=g.distance;
    for(unsigned f=0;f<8;++f) {assert(g.blinkClosed());g.update(Right|Brake);}
    assert(g.distance>before && g.position>10 && g.speed<=88 && g.screen==Drive);
    // Any arrow immediately cancels a blink, even if its 8% relief still
    // leaves the tunnel at its smallest size. It restarts the open window.
    tired(g);g.sign.at=120*Q;g.sign.side=1;
    g.distance=g.sign.at+arrowOffset(g.sign.kind,0)*Q-1;g.position=ARROW_LANE;
    g.drowsy=BLINK_RAMP;g.blinkPhase=60;assert(g.blinkClosed());g.update(0);
    assert((g.events&Neon) && g.sleep>=BLINK_SLEEP && !g.blinkClosed() && !g.drowsy && !g.blinkPhase);
    for(unsigned f=0;f<120;++f) {g.position=ARROW_LANE;g.update(0);assert(!g.blinkClosed());}
    // An animal hit can wake the driver during total black, including the
    // third and later hits. Damage remains the consequence.
    for(unsigned prior=0;prior<6;++prior) {
        tired(g);g.incidents=prior;g.hazard.at=1;g.hazard.warned=true;g.hazard.lane=0;
        g.hazard.age=1600*60/obstacle(Deer).crossingSpeed;
        g.drowsy=BLINK_RAMP;g.blinkPhase=60;assert(g.blinkClosed());g.update(0);
        assert((g.events&Impact) && g.incidents==prior+1 && g.screen==Drive);
        assert(!g.sleep && !g.drowsy && !g.blinkPhase && !g.tunnel() && !g.blinkClosed());
    }
    // Pause freezes both clocks and makes the pause menu visible. Resume
    // returns to the same blink, with no wall-clock jump.
    tired(g);g.drowsy=BLINK_RAMP;g.blinkPhase=60;g.update(Start);
    assert(g.screen==Paused && !g.blinkClosed());auto tick=g.tick;auto phase=g.blinkPhase;auto age=g.drowsy;
    for(unsigned f=0;f<600;++f)g.update(0);
    assert(g.tick==tick && g.blinkPhase==phase && g.drowsy==age);
    g.update(Start);assert(g.screen==Drive && g.blinkClosed() && g.blinkPhase==phase);
    // A road crash still ends the run and snaps the eyes open immediately.
    tired(g);g.drowsy=BLINK_RAMP;g.blinkPhase=60;g.position=1300;g.offroad=179;g.update(0);
    assert(g.screen==Tumble && g.reason==Road && !g.blinkClosed() && !g.tunnel() && !g.sleep);
    printf("PASS: %u progressively closer blinks; 1.2 s final period / 0.3 s closed, no sleep stop, live controls, neon/animal wake-up, frozen pause and crash wake-up\n",starts);
}
