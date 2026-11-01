#include "Game.h"

namespace night {
#if defined(__riscv) && !defined(CHSIM)
#define OBSTACLE_FLASH __attribute__((section(".rodata.lightdrive.obstacles")))
#else
#define OBSTACLE_FLASH
#endif
const ObstacleSpec OBSTACLES[] OBSTACLE_FLASH={
    // radius, warning lead (frames), lateral units/sec, speed loss, drift %, coast frames, rumble strength/ms
    {250,240,520,28,30,90,255,140}, // deer
    {310,300,  0,18,20,60,225,110}, // branch
    {210,192,700,12,12,48,200, 90}, // fox
    {170,150,900, 6, 6,30,145, 65}, // hare
    {180,162,800, 5, 5,24,135, 60}, // owl
    {200,132,950, 3, 3,18,110, 45}  // birds
};
const ObstacleSpec &obstacle(unsigned kind) {return OBSTACLES[kind%CREATURE_COUNT];}
unsigned arrowCount(unsigned kind) {return kind%4==0?3:kind%4==1?6:kind%4==2?2:4;}
int arrowOffset(unsigned kind,unsigned arrow) {return -int(arrowCount(kind)-1)*ARROW_GAP/2+int(arrow)*ARROW_GAP;}
int absolute(int n) {return n<0?-n:n;}
int wave(unsigned a) {
    static const int16_t quarter[]={0,25,50,74,98,121,142,162,181,198,213,226,237,245,251,255,256};
    a&=255;unsigned q=a>>6,x=a&63;if(q&1)x=64-x;
    int v=quarter[x>>2];if((x&3) && x<64)v+=(quarter[(x>>2)+1]-v)*int(x&3)/4;
    return q>=2?-v:v;
}
const char *paletteName(unsigned p) {
    static const char *names[]={"NIGHT RED","AMBER","DEEP BLUE","VIOLET"};return names[p%4];
}
const char *hazardName(unsigned kind) {
    static const char *names[]={"DEER CROSSING","FALLEN BRANCH","FOX CROSSING","HARE CROSSING","LOW FLYING OWL","BIRDS AHEAD"};
    return names[kind%CREATURE_COUNT];
}
unsigned Game::random(unsigned n) {rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return n?rng%n:0;}
void Game::begin(uint32_t routeSeed) {
    const auto oldBest=best;const auto oldPalette=palette;const auto oldMute=muted;const auto oldHaptics=haptics;
    *this=Game{};best=oldBest;palette=oldPalette;muted=oldMute;haptics=oldHaptics;
    seed=routeSeed?routeSeed:0x1751;rng=seed;screen=Drive;
    sign.side=random(2)?1:-1;sign.kind=random(4);
    hazard.lane=int(random(1001))-500;hazard.side=random(2)?1:-1;
}
int Game::bend() const {return wave(distance/(7*Q)+seed)/13+wave(distance/(13*Q)+43)/25;}
int Game::signZ() const {return int32_t(sign.at-distance)/Q;}
int Game::hazardZ() const {return int32_t(hazard.at-distance)/Q;}
int Game::hazardX() const {
    if(hazard.kind==Branch)return hazard.lane;
    int travel=hazard.warned?int(obstacle(hazard.kind).crossingSpeed)*hazard.age/60:0;
    if(travel>5000)travel=5000;
    return hazard.lane+hazard.side*(1600-travel);
}
int Game::curveForce() const {
    int force=bend()*int(speed)*int(speed)/(4*START_SPEED);
    return force+force*damage/100;
}
int Game::arrowZ(unsigned arrow) const {return signZ()+arrowOffset(sign.kind,arrow);}
bool Game::inLightLane() const {return absolute(position-sign.side*ARROW_LANE)<ARROW_HALF_WIDTH;}
unsigned Game::tunnel() const {
    if(screen==Tumble || tumble || sleep<=1000)return 0;
    unsigned t=(sleep-1000)*100u/8000;return t>100?100:t;
}
unsigned Game::blinkPeriod() const {return blinkCycle?blinkCycle:300-unsigned(drowsy)*228/BLINK_RAMP;}
unsigned Game::blinkDuration() const {return 18;}
unsigned Game::blinkShutter() const {
    if(screen!=Drive || !drowsy)return 0;
    unsigned remaining=blinkPeriod()-blinkPhase;
    // Three ticks shutting, eighteen shut, four reopening. The transition
    // belongs to the latched cycle, so it cannot jump as fatigue builds.
    if(remaining>blinkDuration()+7)return 0;
    if(remaining>blinkDuration()+4)return (blinkDuration()+8-remaining)*64;
    if(remaining>4)return 256;
    return remaining*256/5;
}
bool Game::blinkClosed() const {
    return blinkShutter()==256;
}
void Game::stop(Reason why) {
    reason=why;screen=Tumble;tumble=0;
    sleep=drowsy=blinkPhase=blinkCycle=0;events|=Impact;
    if(distance>best)best=distance;
}
void Game::update(uint8_t keys) {
    uint8_t pressed=keys&~previous;previous=keys;events=0;
    if(pressed&Palette)palette=(palette+1)%4;
    if((screen==Title || screen==Paused || screen==Help) && (pressed&Sound))muted=!muted;
    if((screen==Title || screen==Paused || screen==Help) && (pressed&Rumble))haptics=!haptics;
    if(screen==Title) {
        ++tick;
        if(pressed&Left)palette=(palette+3)%4;
        if(pressed&Right)palette=(palette+1)%4;
        if(pressed&Brake)screen=Help;
        else if(pressed&(Confirm|Start)) {begin(seed+tick*17);previous=keys;}
        return;
    }
    if(screen==Help) {if(pressed&(Confirm|Brake|Start))screen=Title;return;}
    if(screen==Paused) {
        if(pressed&Start)screen=Drive;
        else if(pressed&Brake) {if(distance>best)best=distance;screen=Title;}
        return;
    }
    if(screen==Rest) {
        if(pressed&(Confirm|Start)) {begin(seed+137);previous=keys;}
        else if(pressed&Brake)screen=Title;
        return;
    }
    if(screen==Tumble) {
        if(++tumble>=ROLLOVER_FRAMES)screen=Rest;
        return;
    }
    if(pressed&Start) {screen=Paused;return;}
    ++tick;
    if(recovery)--recovery;
    if(slow)--slow;
    if(shock)--shock;
    braking=keys&Brake;
    // A heavy foot: no fixed cruise target. Fatigue presses the pedal
    // harder; braking is the player's only direct control over speed.
    if(braking) {pedal=0;if(tick%4==0 && speed>MIN_SPEED)--speed;}
    else if(slow)pedal=0;
    else {
        pedal+=1+sleep/2600;
        if(pedal>=120) {pedal-=120;if(speed<MAX_SPEED)++speed;}
    }
    const uint32_t before=distance;
    distance+=speed*Q/216;
    // Long, low-amplitude road forces; fatigue adds slow wandering rather
    // than random jolts. Input remains responsive all the way to rest.
    int input=((keys&Right)?1:0)-((keys&Left)?1:0);
    steering=(steering*7+input*(braking?112:160))/8;
    int excess=speed>95?speed-95:0;
    driftRemainder+=wave(tick/6+seed)*(sleep+500)/2300-curveForce()
        +wave(tick/3+seed)*excess*excess/80+int(position)*damage/240;
    position+=steering/16+driftRemainder/256;driftRemainder%=256;
    if(position>1600)position=1600;if(position<-1600)position=-1600;
    sleep+=sleep<4000?3:(tick%5?1:2);
    // The size of the roadside site determines its short trail of light.
    const unsigned arrows=arrowCount(sign.kind);
    const uint32_t firstArrow=sign.at-int(arrows-1)*ARROW_GAP*Q/2;
    for(unsigned i=0;i<arrows;++i) {
        uint32_t at=firstArrow+i*ARROW_GAP*Q;
        if(!(sign.collected&(1u<<i)) && before<at && distance>=at && inLightLane()) {
            sign.collected|=1u<<i;
            sign.taken=true;if(lights<65535)++lights;
            recovery=40;events|=Neon;
            sleep=sleep>ARROW_RELIEF?sleep-ARROW_RELIEF:0;
        }
    }
    if(signZ()<-(int(arrows-1)*ARROW_GAP/2+28)) {
        sign.at+=(850+random(301))*Q;sign.side=random(2)?1:-1;
        sign.kind=(sign.kind+1+random(3))%4;sign.collected=0;sign.taken=false;
    }
    const auto &ob=obstacle(hazard.kind);
    if(!hazard.warned && int32_t(hazard.at-distance)<=int(speed*Q/216)*ob.leadFrames) {
        hazard.warned=true;hazard.age=0;events|=Wildlife;
    }
    if(hazard.warned && !hazard.resolved && hazard.age<1200)++hazard.age;
    if(!hazard.resolved && before<hazard.at && distance>=hazard.at) {
        hazard.resolved=true;
        if(absolute(position-hazardX())<ob.radius) {
            if(incidents<255)++incidents;
            sleep=0;recovery=0;slow=ob.slowFrames;shock=72;pedal=0;events|=Impact;
            unsigned total=damage+ob.damage;damage=total>100?100:total;
            speed=speed>MIN_SPEED+ob.speedLoss?speed-ob.speedLoss:MIN_SPEED;
        } else {events|=Pass;if(hazard.kind!=Branch && avoided<65535)++avoided;}
    }
    if(hazardZ()<-28) {
        hazard.at+=(875+random(426))*Q;
        static const uint8_t kinds[]={Deer,Fox,Hare,Owl,Birds,Branch,Branch};
        hazard.kind=kinds[random(sizeof kinds)];
        hazard.lane=int(random(1201))-600;hazard.side=random(2)?1:-1;
        hazard.resolved=hazard.warned=false;hazard.age=0;
    }
    if(absolute(position)>ROAD_EDGE) {
        if(++offroad==1 || offroad%45==0)events|=Shoulder;
        if(tick%12==0 && !(events&Impact))sleep+=8;
    } else offroad=offroad>3?offroad-3:0;
    if(tick%(sleep>7000?530:370)==0)events|=Chime;
    if(tick%127==0)events|=Rain;
    if(sleep>10000)sleep=10000;
    // Stay at the smallest aperture. Microsleeps interrupt sight, never
    // the road clock or controls. Every light/hit cancels an active blink.
    if(sleep<BLINK_SLEEP || (events&(Neon|Impact)))drowsy=blinkPhase=blinkCycle=0;
    else {
        if(drowsy<BLINK_RAMP)++drowsy;
        // Latch each interval: cadence shortens between blinks, rather
        // than moving a modulo boundary while the eyes are already shut.
        if(!blinkCycle)blinkCycle=blinkPeriod();
        if(++blinkPhase>=blinkCycle)blinkPhase=blinkCycle=0;
    }
    if(offroad>=180)stop(Road);
}
}
