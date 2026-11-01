// Bounded FAT16/FAT32 save-file allocation. Existing files are never
// resized or relocated. Only this game's 8.3 file and its own FAT entries
// are written. Two CRC journal slots preserve the previous score if a
// later sector write is interrupted. No persistent sector buffer/cache.
#include <CHGame.h>
#include <SdSpi.h>
#include "CardScore.h"
#include <string.h>
#ifdef CHSIM
#include <stdio.h>
#include <stdlib.h>
#endif
namespace cardscore {
Status status=Unavailable;
namespace {
uint32_t get(const uint8_t *p,unsigned n=4) {uint32_t v=0;for(unsigned i=0;i<n;++i)v|=uint32_t(p[i])<<(8*i);return v;}
void put(uint8_t *p,uint32_t v,unsigned n=4) {for(unsigned i=0;i<n;++i)p[i]=v>>(8*i);}
uint32_t crc(const uint8_t *p,unsigned n) {uint32_t c=~0u;while(n--) {c^=*p++;for(unsigned i=0;i<8;++i)c=(c>>1)^(0xedb88320u&-(c&1));}return ~c;}
#ifdef CHSIM
bool write(uint32_t lba,const uint8_t *b) {
    const char *path=getenv("CHSD_CARD");FILE *f=path?fopen(path,"r+b"):nullptr;if(!f)return false;
    bool ok=!fseek(f,uint64_t(lba)*512,SEEK_SET) && fwrite(b,1,512,f)==512;
    ok=!fclose(f)&&ok;return ok;
}
#else
// Same bounded CMD24 and bus restoration as Pinball/CardWrite.cpp.
uint8_t transfer(uint8_t b) {SPI1->DATAR=b;while(!(SPI1->STATR&1)){}return SPI1->DATAR;}
uint8_t wait(bool token) {uint32_t start=millis();uint8_t r;do r=transfer(255);while((r==255)==token && millis()-start<1000);return r;}
uint8_t cmd(uint8_t n,uint32_t arg) {transfer(255);transfer(64|n);for(int s=24;s>=0;s-=8)transfer(arg>>s);transfer(0x95);uint8_t r;unsigned tries=9;do r=transfer(255);while((r&128)&&--tries);return r;}
bool write(uint32_t lba,const uint8_t *b) {
    GPIOA->BSHR=1u<<4;while(SPI1->STATR&(1u<<7)){}
    uint16_t old=SPI1->CTLR1;SPI1->CTLR1=old&~(1u<<6);
    SPI1->CTLR1=(1u<<2)|(1u<<8)|(1u<<9)|(1u<<6)|(1u<<3);
    (void)SPI1->DATAR;(void)SPI1->STATR;GPIOB->BCR=1u<<11;
    bool ok=wait(false)==255 && cmd(58,0)==0;uint32_t ocr=0;
    if(ok)for(unsigned i=0;i<4;++i)ocr=(ocr<<8)|transfer(255);
    if(ok)ok=cmd(24,(ocr&(1u<<30))?lba:lba<<9)==0;
    if(ok) {transfer(255);transfer(0xfe);for(unsigned i=0;i<512;++i)transfer(b[i]);transfer(255);transfer(255);ok=(transfer(255)&31)==5;if(wait(false)!=255)ok=false;}
    GPIOB->BSHR=1u<<11;transfer(255);while(SPI1->STATR&(1u<<7)){}
    SPI1->CTLR1&=~(1u<<6);SPI1->CTLR1=old;return ok;
}
#endif
struct Volume {
    uint32_t base,fat,fatSize,data,root,rootSecs,end,spc,copies;
    bool f32() const {return rootSecs==0;}
    uint32_t lba(uint32_t c) const {return data+(c-2)*spc;}
    uint32_t next(uint32_t c,uint8_t *b) const {
        unsigned stride=f32()?4:2,off=c*stride;
        if(c<2 || c>=end || !sd::read(fat+off/512,b))return 1;
        return get(b+off%512,stride)&(f32()?0x0fffffffu:0xffffu);
    }
    bool set(uint32_t c,uint32_t value,uint8_t *b) const {
        unsigned stride=f32()?4:2,off=c*stride;
        for(unsigned i=0;i<copies;++i) {
            uint32_t sector=fat+i*fatSize+off/512;
            if(!sd::read(sector,b))return false;
            uint32_t v=value;if(f32())v|=get(b+off%512)&0xf0000000u;
            put(b+off%512,v,stride);if(!write(sector,b))return false;
        }
        return true;
    }
};
bool mount(Volume &v,uint8_t *b) {
    if(!sd::init() || !sd::read(0,b))return false;v.base=0;
    if(get(b+510,2)!=0xaa55)return false;
    if(b[0]!=0xeb && b[0]!=0xe9) {
        for(unsigned i=446;i<510;i+=16)if(b[i+4]==4 || b[i+4]==6 || b[i+4]==11 || b[i+4]==12 || b[i+4]==14) {v.base=get(b+i+8);break;}
        if(!v.base || !sd::read(v.base,b) || get(b+510,2)!=0xaa55)return false;
    }
    v.spc=b[13];v.copies=b[16];v.rootSecs=(get(b+17,2)+15)/16;
    uint32_t reserved=get(b+14,2),total=get(b+19,2);if(!total)total=get(b+32);
    v.fatSize=get(b+22,2);if(!v.fatSize)v.fatSize=get(b+36);
    uint32_t meta=reserved+v.copies*v.fatSize+v.rootSecs;
    if(get(b+11,2)!=512 || !v.spc || v.spc>128 || (v.spc&(v.spc-1)) || !reserved || !v.copies || v.copies>2 || !v.fatSize || v.fatSize>0x100000 || meta>=total)return false;
    v.end=(total-meta)/v.spc+2;
    if(v.end<4087 || (v.rootSecs && v.end>=65527) || (!v.rootSecs && (v.end<65527 || (get(b+40,2)&128))))return false;
    if(v.fatSize*(v.f32()?128:256)<v.end)return false;
    v.fat=v.base+reserved;v.data=v.base+meta;
    v.root=v.f32()?get(b+44):v.fat+v.copies*v.fatSize;
    return !v.f32() || (v.root>=2 && v.root<v.end);
}
// Find/allocate exactly two data sectors. Never grow a full root directory.
bool locate(uint32_t slots[2],uint8_t *b,bool create) {
    Volume v;if(!mount(v,b))return false;
    uint32_t cluster=v.root,freeSector=0,nextEnd=0;unsigned freeOffset=0;
    for(unsigned guard=0;guard<4096;++guard) {
        unsigned sectors=v.f32()?v.spc:v.rootSecs;
        for(unsigned k=0;k<sectors;++k) {
            uint32_t sector=v.f32()?v.lba(cluster)+k:v.root+k;
            if(!sd::read(sector,b))return false;
            for(unsigned off=0;off<512;off+=32) {
                auto p=b+off;
                if((p[0]==0 || p[0]==0xe5) && !freeSector) {freeSector=sector;freeOffset=off;}
                if(p[0] && p[0]!=0xe5 && !memcmp(p,"LIGHTDRVSAV",11)) {
                    if((p[11]&0x1f) || get(p+28)!=1024)return false;
                    uint32_t c=get(p+26,2)+(v.f32()?(get(p+20,2)<<16):0);
                    if(c<2 || c>=v.end)return false;
                    slots[0]=v.lba(c);
                    uint32_t next=v.next(c,b),eoc=v.f32()?0x0ffffff8u:0xfff8u;
                    if(v.spc>=2) {if(next<eoc)return false;slots[1]=slots[0]+1;}
                    else {if(next<2 || next>=v.end || v.next(next,b)<eoc)return false;slots[1]=v.lba(next);}
                    return true;
                }
                if(!p[0]) {
                    if(freeSector==sector && freeOffset==off && off==480) {
                        if(k+1<sectors)nextEnd=sector+1;
                        else if(v.f32()) {uint32_t n=v.next(cluster,b);if(n>=2 && n<v.end)nextEnd=v.lba(n);}
                    }
                    goto foundEnd;
                }
            }
        }
        if(!v.f32())break;
        cluster=v.next(cluster,b);if(cluster<2 || cluster>=v.end)break;
    }
foundEnd:
    if(!create || !freeSector)return false;
    uint32_t first=0,second=0,cached=~0u;
    unsigned stride=v.f32()?4:2;
    for(uint32_t c=2;c<v.end;++c) {
        uint32_t sector=v.fat+c*stride/512;
        if(sector!=cached) {if(!sd::read(sector,b))return false;cached=sector;}
        if(!(get(b+c*stride%512,stride)&(v.f32()?0x0fffffffu:0xffffu))) {
            if(!first) {first=c;if(v.spc>=2)break;}
            else {second=c;break;}
        }
    }
    if(!first || (v.spc==1 && !second))return false;
    slots[0]=v.lba(first);slots[1]=v.spc>=2?slots[0]+1:v.lba(second);
    memset(b,0,512);if(!write(slots[0],b) || !write(slots[1],b))return false;
    if(!v.set(first,second?second:0x0fffffffu,b) || (second && !v.set(second,0x0fffffffu,b)))return false;
    // The old end marker may have been the sector's last entry. Publish
    // the following marker first, before making the new file visible.
    if(nextEnd) {if(!sd::read(nextEnd,b))return false;b[0]=0;if(!write(nextEnd,b))return false;}
    if(!sd::read(freeSector,b))return false;
    auto p=b+freeOffset;bool wasEnd=!p[0];memset(p,0,32);memcpy(p,"LIGHTDRVSAV",11);p[11]=0x20;
    if(wasEnd && freeOffset<480)p[32]=0;
    put(p+26,first,2);if(v.f32())put(p+20,first>>16,2);put(p+28,1024);
    if(!write(freeSector,b))return false;
    // Invalidate the optional FAT32 allocation hints after allocating.
    if(v.f32() && sd::read(v.base,b)) {
        uint32_t info=get(b+48,2),reserved=get(b+14,2);
        if(info && info<reserved && sd::read(v.base+info,b) && get(b)==0x41615252u && get(b+484)==0x61417272u) {
            put(b+488,~0u);put(b+492,~0u);if(!write(v.base+info,b))return false;
        }
    }
    return true;
}
struct Score {uint32_t sequence,best;uint16_t wakeups,avoided;};
bool readScore(uint32_t lba,Score &s,uint8_t *b) {
    if(!sd::read(lba,b) || get(b)!=0x3244474cu || get(b+20)!=crc(b,20))return false;
    s={get(b+4),get(b+8),uint16_t(get(b+12,2)),uint16_t(get(b+14,2))};return true;
}
bool latest(const uint32_t slots[2],Score &s,unsigned &index,uint8_t *b) {
    Score a{},c{};bool va=readScore(slots[0],a,b),vc=readScore(slots[1],c,b);
    index=vc && (!va || int32_t(c.sequence-a.sequence)>0)?1:0;s=index?c:a;return va||vc;
}
}
bool load(uint32_t &best) {
    auto b=gfx_chunkScratch();uint32_t slots[2];Score s{};unsigned index;
    if(!locate(slots,b,false)) {status=Unavailable;return false;}
    status=Ready;if(!latest(slots,s,index,b))return false;
    best=s.best;return true;
}
bool store(uint32_t best,uint16_t wakeups,uint16_t avoided) {
    auto b=gfx_chunkScratch();uint32_t slots[2];Score old{};unsigned index=0;
    if(!locate(slots,b,true)) {status=Failed;return false;}
    bool had=latest(slots,old,index,b);
    if(had && old.best>=best) {status=Saved;return true;}
    memset(b,0,512);put(b,0x3244474cu);put(b+4,old.sequence+1);put(b+8,best);
    put(b+12,wakeups,2);put(b+14,avoided,2);put(b+20,crc(b,20));
    uint32_t sector=slots[had?1-index:0];
    if(!write(sector,b)) {status=Failed;return false;}
    Score checked{};bool ok=readScore(sector,checked,b) && checked.best==best;
    status=ok?Saved:Failed;return ok;
}
}
