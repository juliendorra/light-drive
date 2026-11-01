#include "Game.h"

namespace night {
namespace {
unsigned ease(unsigned t,unsigned duration) {
    if(t>=duration)return 1024;
    unsigned p=t*1024/duration;
    return p*p*(3072-2*p)/(1024*1024);
}
int bump(unsigned frame,unsigned at,int strength) {
    if(frame<at || frame>=at+12)return 0;
    unsigned t=frame-at;
    return t<3?int(t)*strength/3:int(12-t)*strength/9;
}
}
// Keep curve evaluation in flash rather than expanding it into the SRAM
// raster loop. The full-resolution four-ink snapshot costs 4 KB of RAM.
__attribute__((noinline)) RolloverPose rolloverPose(unsigned frame,unsigned speed,int side) {
    if(frame>ROLLOVER_FRAMES)frame=ROLLOVER_FRAMES;
    if(!frame)return {0,256,0,0,0,0};
    int angle;
    // Nonzero motion on the first frame, accelerating into the first side
    // contact. Zoom and lateral departure share this same time origin.
    if(frame<=44)angle=(frame*(frame+44)*64+1936)/(44*88);
    else if(frame<=75)angle=64+int(frame-44)*64/31;
    else if(frame<=106)angle=128+int(frame-75)*64/31;
    else if(frame<=140)angle=192+int(frame-106)*64/34;
    else if(frame<=168)angle=256+22*ease(frame-140,28)/1024;
    else if(frame<=190)angle=278-8*ease(frame-168,22)/1024;
    else angle=270+4*ease(frame-190,20)/1024;
    int x=frame<168?2*wave(frame*128/168)/256:0;
    int y=bump(frame,0,2)+bump(frame,44,4)+bump(frame,75,3)+bump(frame,106,3)
        +bump(frame,140,4)+bump(frame,168,2)+bump(frame,190,1);
    unsigned zoom=frame<48?256+160*frame*(96-frame)/(48*48):
        frame<120?416:416-61*ease(frame-120,90)/1024;
    unsigned cover=absolute(wave(angle))+absolute(wave(angle+64))+7*(absolute(x)+absolute(y))+2;
    if(zoom<cover)zoom=cover;
    // Preserve entry-speed momentum through the first body contact, then
    // coast forward with a smooth deceleration through the rolling phase.
    // The last small rebound settles in place, rather than sliding forever.
    unsigned step=speed*Q/216,travelTime=frame<190?frame:190;
    unsigned coast=travelTime>44?travelTime-44:0;
    unsigned travel=travelTime<=44?step*travelTime:
        step*44+step*coast*(292-coast)/292;
    int lateral=frame<=42?5*frame+frame*frame/2:
        1092+int((frame<90?frame-42:48))*378/48;
    return {int16_t(side<0?-angle:angle),uint16_t(zoom),uint16_t(travel),int16_t(side<0?-lateral:lateral),int8_t(side<0?-x:x),int8_t(y)};
}
RolloverContact rolloverContact(unsigned frame) {
    switch(frame) {
    case 0:return {45,130};     // tyre/gravel, momentum continues
    case 18:return {55,170};    // suspension loads up before the roll
    case 44:return {130,240};   // first quarter-turn contact
    case 75:return {160,220};   // roof
    case 106:return {140,180};   // second side
    case 140:return {180,215};   // wheels/ground
    case 168:return {100,120};   // settle and rebound
    case 190:return {120,80};
    default:return {0,0};
    }
}
}
