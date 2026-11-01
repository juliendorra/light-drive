#include "Neon.h"
namespace neon {
Flicker flicker(uint32_t sign,uint32_t seed,uint32_t tick,unsigned kind) {
    uint32_t n=sign^seed^(tick/37)*0x9e3779b9u;
    n^=n>>16;n*=0x7feb352du;n^=n>>15;
    Flicker f;unsigned phase=tick%37,start=(n>>9)%22,length=2+(n>>16)%5;
    if(n%9 || !((phase>=start && phase<start+length) || ((n>>21)%3==0 && phase==start+10)))return f;
    unsigned zone=(n>>24);
    switch(kind%4) {
    case 0:f.left=5+(zone%4)*7;f.right=f.left+6;f.top=3;f.bottom=17;break;
    case 1:f.left=8+(zone%5)*5;f.right=f.left+5;f.top=2;f.bottom=16;break;
    case 2:f.left=8;f.right=15;f.top=4+(zone%3)*10;f.bottom=f.top+7;break;
    default:
        if(zone%4==3) {f.left=5;f.right=35;f.top=20;f.bottom=24;}
        else {f.left=12+(zone%3)*6;f.right=f.left+6;f.top=3;f.bottom=17;}
    }
    return f;
}
}
