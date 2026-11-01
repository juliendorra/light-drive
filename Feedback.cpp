#include <Arduino.h>
#include "Feedback.h"
#include "src/rumble/Rumble.h"

namespace feedback {
namespace {
struct Step {uint16_t ms;uint8_t strength;};
const Step LOSS[]={{120,255},{45,0},{160,235},{55,0},{180,210},{75,0},{200,165},{85,0},{240,110},{85,0},{200,80}};
const Step NEON[]={{35,110}};
const Step *steps=nullptr;
uint8_t index=0,count=0;
uint32_t deadline=0,lastShoulder=0;
night::Screen previous=night::Title;
bool wasShoulder=false;
uint16_t lastRollFrame=0xFFFF;
void cancel() {steps=nullptr;count=index=0;rumble::stop();}
void output(uint32_t now) {
    const auto &step=steps[index];
    if(step.strength)rumble::pulse(step.strength,uint16_t(deadline-now),4);
    else rumble::stop();
}
void start(const Step *pattern,uint8_t n,uint32_t now) {
    cancel();steps=pattern;count=n;deadline=now+steps[0].ms;output(now);
}
void advance(uint32_t now) {
    if(!steps)return;
    bool changed=false;
    while(int32_t(now-deadline)>=0) {
        if(++index>=count) {cancel();return;}
        deadline+=steps[index].ms;changed=true;
    }
    if(changed)output(now);
}
}
void begin() {rumble::begin();cancel();previous=night::Title;lastShoulder=0;wasShoulder=false;lastRollFrame=0xFFFF;}
void update(const night::Game &g) {
    rumble::update();uint32_t now=millis();
    bool allowed=g.haptics && (g.screen==night::Drive || g.screen==night::Tumble || g.screen==night::Rest);
    rumble::setEnabled(allowed);
    if(!allowed) {cancel();previous=g.screen;wasShoulder=false;lastRollFrame=0xFFFF;return;}
    if(g.screen!=previous) {
        cancel();wasShoulder=false;
        if(g.screen==night::Tumble)lastRollFrame=0xFFFF;
        else if(g.screen==night::Rest && previous!=night::Tumble)start(LOSS,sizeof LOSS/sizeof LOSS[0],now);
    }
    previous=g.screen;
    if(g.screen==night::Tumble) {
        unsigned first=lastRollFrame==0xFFFF?0:lastRollFrame+1;
        for(unsigned frame=first;frame<=g.tumble;++frame) {
            const auto contact=night::rolloverContact(frame);
            if(contact.milliseconds)rumble::pulse(contact.strength,contact.milliseconds,4);
        }
        lastRollFrame=g.tumble;return;
    }
    advance(now);
    if(g.screen!=night::Drive)return;
    if(g.events&night::Impact) {
        cancel();const auto &hit=night::obstacle(g.hazard.kind);rumble::pulse(hit.rumble,hit.rumbleMs,3);
    }
    else if((g.events&night::Neon) && !rumble::remaining())start(NEON,sizeof NEON/sizeof NEON[0],now);
    bool shoulder=night::absolute(g.position)>night::ROAD_EDGE;
    if(shoulder && !steps && (!wasShoulder || g.tick-lastShoulder>=14)) {
        rumble::pulse(g.offroad>100?140:120,32,1);lastShoulder=g.tick;
    }
    wasShoulder=shoulder;
}
}
