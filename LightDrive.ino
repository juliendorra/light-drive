// LIGHT DRIVE: an original, quiet first-person night drive for CHGame Rev0.
#include <CHGame.h>
#include "Game.h"
#include "View.h"
#include "Sounds.h"
#include "Feedback.h"
#include "CardScore.h"
#include "src/rumble/Rumble.h"
#ifdef NIGHT_PROFILE
extern volatile uint32_t *nightProfileData();
#endif
#ifdef NIGHT_DEVICE_PROFILE
extern void nightProfileReset(bool glow);
#endif

night::Game game;
struct Record {uint32_t best=0;uint8_t palette=0,muted=0,haptics=1,reserved=0;} record;
constexpr uint32_t SAVE_MAGIC=save::magic("LGDR");
#if CHGAME_DEBUG
static bool debug(char command,const char *args) {
    if(command=='G') {
        gfx_wait();bool ok=cardscore::store(game.best,game.lights,game.avoided);
        uint32_t saved=0;ok=cardscore::load(saved) && saved>=game.best && ok;
        char line[64];snprintf(line,sizeof line,"CARDSCORE %u %u\n",ok,(unsigned)saved);dbg::print(line);return true;
    }
#ifdef NIGHT_DEVICE_PROFILE
    if(command=='Y') {
        unsigned kind=0,z=92,sleep=8800,glow=1;int position=500;
        sscanf(args,"%u %u %u %d %u",&kind,&z,&sleep,&position,&glow);
        if(kind>3 || z>200 || sleep>9500 || position<-1000 || position>1000)return false;
        game.begin();game.distance=420*night::Q;game.tick=1200;game.sleep=sleep;game.position=position;
        game.sign.kind=kind;game.sign.side=1;game.sign.at=game.distance+z*night::Q;
        game.hazard.at=game.distance+1000*night::Q;nightProfileReset(glow);return true;
    }
    if(command=='C') {
        auto metrics=nightProfileData();char line[384];unsigned used=snprintf(line,sizeof line,"CPUMETRICS");
        for(unsigned i=0;i<30;++i)used+=snprintf(line+used,sizeof line-used," %u",(unsigned)metrics[i]);
        line[used++]='\n';line[used]=0;dbg::print(line);return true;
    }
#endif
#if defined(CHSIM) && defined(NIGHT_CAPTURE)
    // Staging only for renderer QA; absent from ordinary builds/firmware.
    if(command=='E') {
        unsigned distance=420*night::Q,tick=1200,kind=0,sleep=8800,speed=night::START_SPEED,base=0;int position=500,side=1,z=65,steering=0;
        sscanf(args,"%u %d %u %u %d %d %u %u %u %d",&distance,&position,&tick,&kind,&side,&z,&sleep,&speed,&base,&steering);
        if(position<-4096 || position>4096 || kind>3 || (side!=1 && side!=-1) || z<0 || z>2000 || sleep>9500 || speed<42 || speed>118 || base>1 || steering<-160 || steering>160)return false;
        game.begin();game.distance=distance;game.position=position;game.tick=tick;game.sleep=sleep;game.palette=0;
        game.speed=speed;game.steering=steering;
        game.sign.at=distance+z*night::Q;game.sign.kind=kind;game.sign.side=side;game.hazard.at=distance+1000*night::Q;
        // Roll frame zero draws the same scene without the receiver or
        // overlays; no transform is applied. Pixel-layering review only.
        if(base)game.screen=night::Tumble;
        gfx_wait();view::draw(game);gfx_flushAsync();return true;
    }
    if(command=='J') {
        unsigned metres=0,sleep=1100,palette=0;int position=0;
        sscanf(args,"%u %u %d %u",&metres,&sleep,&position,&palette);
        if(metres>16000000 || sleep>10000 || position<-1600 || position>1600 || palette>3)return false;
        game.begin();game.distance=metres*night::Q;game.tick=metres*3;
        game.sleep=sleep;game.position=position;game.palette=palette;
        game.sign.at=game.distance+65*night::Q;game.sign.side=1;
        game.hazard.at=game.distance+50*night::Q;return true;
    }
    if(command=='O') {
        game.begin();game.distance=12345*night::Q;game.lights=17;game.avoided=9;
        game.position=1300;game.offroad=179;
        game.sign.at=game.hazard.at=game.distance+1000*night::Q;
        game.update(0);while(game.screen==night::Tumble)game.update(0);return true;
    }
    if(command=='X') {
        unsigned age=0,phase=0;sscanf(args,"%u %u",&age,&phase);if(age>night::BLINK_RAMP)return false;
        game.begin();game.distance=420*night::Q;game.tick=1200;game.sleep=10000;game.drowsy=age;
        if(phase>=game.blinkPeriod())return false;game.blinkPhase=phase;
        game.sign.at=game.hazard.at=game.distance+100000*night::Q;return true;
    }
    if(command=='V') {
        int side=1;unsigned speed=night::START_SPEED;sscanf(args,"%d %u",&side,&speed);if(side!=-1)side=1;
        if(speed<night::MIN_SPEED || speed>night::MAX_SPEED)return false;
        game.begin();game.speed=speed;game.sleep=8500;game.position=side*1300;game.offroad=165;
        game.sign.at=game.distance+1000*night::Q;game.hazard.at=game.distance+1000*night::Q;return true;
    }
    if(command=='W') {
        unsigned kind=0,z=70,sleep=1100,speed=night::START_SPEED,age=0,phase=0,hits=0,damage=0;int position=550;
        sscanf(args,"%u %u %d %u %u %u %u %u %u",&kind,&z,&position,&sleep,&speed,&age,&phase,&hits,&damage);
        if(kind>=night::CREATURE_COUNT || z>190 || sleep>10000 || speed<42 || speed>118 || position<-1000 || position>1000 || age>night::BLINK_RAMP || hits>255 || damage>100)return false;
        game.begin();game.distance=420*night::Q;game.tick=1200;game.position=position;game.sleep=sleep;game.speed=speed;game.palette=0;
        game.hazard.kind=kind;game.hazard.at=game.distance+z*night::Q;game.hazard.side=-1;game.hazard.lane=0;
        game.drowsy=age;if(phase>=game.blinkPeriod())return false;game.blinkPhase=phase;
        game.incidents=hits;game.damage=damage;
        const auto &ob=night::obstacle(kind);unsigned remaining=z*216/game.speed;
        if(remaining<ob.leadFrames) {game.hazard.warned=true;game.hazard.age=ob.leadFrames-remaining;}
        game.sign.at=game.distance+1000*night::Q;return true;
    }
    if(command=='U') {
        unsigned kind=0,z=32,sleep=1100,palette=2,age=0,phase=0;int side=1;
        sscanf(args,"%u %d %u %u %u %u %u",&kind,&side,&z,&sleep,&palette,&age,&phase);
        if(kind>3 || (side!=1 && side!=-1) || z>180 || sleep>10000 || palette>3 || age>night::BLINK_RAMP)return false;
        game.begin();game.distance=420*night::Q;game.tick=1200;game.position=side*500;game.sleep=sleep;game.palette=palette;
        game.sign.at=game.distance+z*night::Q;game.sign.side=side;game.sign.kind=kind;
        game.drowsy=age;if(phase>=game.blinkPeriod())return false;game.blinkPhase=phase;
        game.hazard.at=game.distance+1000*night::Q;return true;
    }
    if(command=='F') {game.begin(strtoul(args,nullptr,10));return true;}
#endif
    if(command!='H')return false;
    char line[240];
    snprintf(line,sizeof line,"NIGHTSTATE %u %u %u %d %u %u %u %u %u %u %u %d %d %d %u %u %u %u %u %u %u %u %u %u %d %u %u %u %u %u %u %u\n",
        game.screen,(unsigned)game.tick,(unsigned)game.distance,game.position,
        game.sleep,game.speed,game.palette,game.muted,game.offroad,game.lights,game.sign.collected,
        game.signZ(),game.sign.side,game.hazardZ(),game.hazard.kind,game.incidents,game.events,game.shock,
        game.haptics,rumble::level(),rumble::remaining(),game.tumble,game.damage,game.hazard.age,game.hazardX(),game.avoided,(unsigned)game.best,cardscore::status,
        game.drowsy,game.blinkPhase,game.blinkClosed(),game.blinkShutter());
    dbg::print(line);return true;
}
#endif
void setup() {
    chgame.boot();dbg::begin("NIGHT 1.3.6");
#if CHGAME_DEBUG
    dbg::hook=debug;
#endif
    gfx_begin(GFX_DIV2,GFX_12BPP);chgame.setFrameRate(60);
    if(!save::load(SAVE_MAGIC,1,record))record=Record{};
    gfx_wait();uint32_t cardBest=0;
    if(cardscore::load(cardBest) && cardBest>record.best) {record.best=cardBest;save::store(SAVE_MAGIC,1,record);}
    else if(record.best>cardBest)cardscore::store(record.best,0,0);
    game.best=record.best;game.palette=record.palette%4;game.muted=record.muted;game.haptics=record.haptics;
    audio::begin(SOUNDS,(uint8_t)Sfx::COUNT,!game.muted);
    feedback::begin();
}
void loop() {
    dbg::poll();if(!chgame.nextFrame())return;
    chgame.pollButtons();uint8_t keys=0;
    if(chgame.pressed(A_BUTTON))keys|=night::Confirm;
    if(chgame.pressed(B_BUTTON))keys|=night::Brake;
    if(chgame.pressed(LEFT_BUTTON))keys|=night::Left;
    if(chgame.pressed(RIGHT_BUTTON))keys|=night::Right;
    if(chgame.pressed(START_BUTTON))keys|=night::Start;
    if(chgame.pressed(SELECT_BUTTON))keys|=night::Palette;
    if(chgame.pressed(UP_BUTTON))keys|=night::Sound;
    if(chgame.pressed(DOWN_BUTTON))keys|=night::Rumble;
#ifdef NIGHT_PROFILE
    uint32_t cpuBegan=micros();
#endif
    const auto before=game.screen;game.update(keys);soundscape(game);feedback::update(game);
    const bool changed=record.palette!=game.palette || record.muted!=game.muted || record.haptics!=game.haptics ||
        ((game.screen==night::Rest || (game.screen==night::Title && before==night::Paused)) && record.best!=game.best);
#ifdef NIGHT_PROFILE
    uint32_t updateTime=micros()-cpuBegan,waitBegan=micros();
#endif
    gfx_wait();
#ifdef NIGHT_PROFILE
    auto metrics=nightProfileData();uint32_t wait=micros()-waitBegan;
    metrics[28]+=wait;if(wait>metrics[29])metrics[29]=wait;
    cpuBegan=micros();
#endif
    if(changed) {
        if(game.best>record.best)cardscore::store(game.best,game.lights,game.avoided);
        record.best=game.best;record.palette=game.palette;record.muted=game.muted;record.haptics=game.haptics;save::store(SAVE_MAGIC,1,record);
    }
    dbg::markRenderStart();view::draw(game);dbg::markRenderEnd();gfx_flushAsync();
#ifdef NIGHT_PROFILE
    uint32_t cpu=updateTime+micros()-cpuBegan;metrics[21]+=cpu;if(cpu>metrics[22])metrics[22]=cpu;
#endif
}
