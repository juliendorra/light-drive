#pragma once
#include <stdint.h>

namespace night {
constexpr unsigned ROLLOVER_FRAMES=210;
struct RolloverPose {
    int16_t angle;             // unwrapped, 256 units per turn
    uint16_t zoom,travel;       // Q8 zoom and visual travel in metres * 256
    int16_t lateral;           // accelerating departure, road units
    int8_t x,y;                // small camera displacement in source pixels
};
struct RolloverContact {uint16_t milliseconds;uint8_t strength;};
RolloverPose rolloverPose(unsigned frame,unsigned speed,int side=1);
RolloverContact rolloverContact(unsigned frame);
}
