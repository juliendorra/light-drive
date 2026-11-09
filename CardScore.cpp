// Import the old distance-only journal. New saves include preferences in
// the shared per-game SD format; this file never writes the old journal.
#include <CHGame.h>
#include <SdSpi.h>
#include <Fat.h>
#include "CardScore.h"
namespace cardscore {
static uint32_t get(const uint8_t *p) {return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
static uint32_t crc(const uint8_t *p,unsigned n) {uint32_t c=~0u;while(n--) {c^=*p++;for(unsigned i=0;i<8;++i)c=(c>>1)^(0xedb88320u&-(c&1));}return ~c;}
bool load(uint32_t &best) {
    gfx_wait();auto *b=gfx_chunkScratch();
    if(!sd::init() || fat::mount(b))return false;
    fat::File file;if(fat::find("LIGHTDRVSAV",file,b) || file.size!=1024)return false;
    fat::Run runs[2];int n=fat::runs(file,runs,2,b);if(n<1)return false;
    bool have=false;uint32_t generation=0;
    for(unsigned slot=0;slot<2;++slot) {
        if(!fat::read(runs,n,slot,b))return false;
        if(get(b)!=0x3244474c || get(b+20)!=crc(b,20))continue;
        if(!have || int32_t(get(b+4)-generation)>0) {generation=get(b+4);best=get(b+8);have=true;}
    }
    return have;
}
}
