#include "../../Game.h"
#include <assert.h>
#include <stdio.h>
using namespace night;
int main() {
    const auto initial=rolloverPose(0,80);
    assert(initial.angle==0 && initial.zoom==256 && initial.travel==0 && initial.lateral==0);
    // No separate stopped/zoom-only stage: all three movements begin on
    // frame one, with lateral velocity increasing toward the first hit.
    const auto first=rolloverPose(1,80);
    assert(first.angle>0 && first.zoom>256 && first.lateral>0 && first.travel>0);
    assert(rolloverPose(24,80).lateral-rolloverPose(23,80).lateral>
        rolloverPose(6,80).lateral-rolloverPose(5,80).lateral);
    unsigned contacts=0,largestTurn=0,largestZoom=0;
    auto previous=initial;
    for(unsigned frame=0;frame<=ROLLOVER_FRAMES;++frame) {
        auto p=rolloverPose(frame,80),left=rolloverPose(frame,80,-1);
        assert(left.angle==-p.angle && left.lateral==-p.lateral && left.x==-p.x && left.y==p.y && left.zoom==p.zoom && left.travel==p.travel);
        assert(p.zoom>=256 && p.zoom<=448 && p.travel>=previous.travel);
        if(frame<=168)assert(p.angle>=previous.angle);
        if(frame<=44)assert(p.travel==80*Q/216*frame);
        if(frame>=190)assert(p.travel==rolloverPose(190,80).travel);
        unsigned turn=absolute(p.angle-previous.angle),zoom=absolute(int(p.zoom)-previous.zoom);
        if(turn>largestTurn)largestTurn=turn;if(zoom>largestZoom)largestZoom=zoom;
        if(frame) {assert(turn<=4);assert(zoom<=24);}
        auto impact=rolloverContact(frame);
        if(impact.milliseconds) {++contacts;assert(impact.milliseconds<=500);}
        previous=p;
    }
    assert(contacts==8);
    assert(rolloverPose(180,80).travel>rolloverPose(150,80).travel);
    // Forward displacement keeps the incoming speed before contact and
    // tapers to rest afterwards, for the full range of driving speeds.
    for(unsigned speed=MIN_SPEED;speed<=MAX_SPEED;++speed) {
        unsigned previousStep=speed*Q/216;
        for(unsigned frame=45;frame<=190;++frame) {
            unsigned movement=rolloverPose(frame,speed).travel-rolloverPose(frame-1,speed).travel;
            assert(movement<=previousStep+1 && movement<=speed*Q/216);
            previousStep=movement;
        }
        assert(rolloverPose(190,speed).travel==(speed*Q/216)*117);
    }
    assert(rolloverPose(12,80).angle>=8 && rolloverPose(12,80).angle<16);
    assert(rolloverPose(24,80).angle<40); // accelerating rather than snapping to 90 degrees
    assert(rolloverPose(168,80).angle>rolloverPose(190,80).angle); // damped landing rebound
    assert(rolloverPose(ROLLOVER_FRAMES,80).angle==274);
    auto settled=rolloverPose(999,80);assert(settled.angle==274 && settled.travel==previous.travel && settled.zoom==previous.zoom);
    const unsigned times[]={44,75,106,140},angles[]={64,128,192,256};
    for(unsigned i=0;i<4;++i) {
        assert(rolloverPose(times[i]-1,80).angle<int(angles[i]));
        assert(rolloverPose(times[i],80).angle>=int(angles[i]));
        assert(rolloverContact(times[i]).milliseconds>0);
    }
    printf("PASS: progressive 3.5 s roll, max per-frame angle step %.2f degrees, zoom step %.3fx, eight synchronized contacts, mirrored sides and stable final pose\n",largestTurn*360.0/256,largestZoom/256.0);
}
