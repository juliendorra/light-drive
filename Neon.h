#pragma once
#include "Game.h"
namespace neon {
struct Flicker {
    uint8_t left=255,right=0,top=0,bottom=0;
    bool contains(int x,int y) const {return x>=left && x<right && y>=top && y<bottom;}
};
// Stateless keyed noise: does not consume the route's random sequence.
Flicker flicker(uint32_t sign,uint32_t seed,uint32_t tick,unsigned kind);
}
