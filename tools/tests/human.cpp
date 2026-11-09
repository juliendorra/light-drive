// Synthetic human model: delayed, noisy observations, visible objects only,
// intermittent attention, finite button decisions and cautious avoidance.
// Accelerated wall time; game time always advances in 1/60-second steps.
#include "../../Game.h"
#include "../../Scenery.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace night;
struct Random {
    unsigned state;
    unsigned next(unsigned n){state^=state<<13;state^=state>>17;state^=state<<5;return state%n;}
    int noise(int n){return int(next(n*2+1))-n;}
};
struct Profile {
    const char *name;int sample,reaction,noise,lapseEvery,lapseFrames,brakeTtc,brakeTail;
    double anticipation;bool seekLight=true,fullVision=false,noBrake=false,managePedal=true,holdBrake=false,noFatigue=false;
};
struct Observation {
    int position=250,velocity=0,sleep=1800,speed=START_SPEED,damage=0;
    int signSide=0,signZ=999,hazardKind=-1,hazardLane=0,hazardZ=999;
    unsigned signId=0,hazardId=0;
    bool laneVisible=true,shoulder=false;
};
static bool visible(const Game &g,double x,double y,bool full,int bloom=0){
    if(full)return true;
    if(g.blinkClosed())return false;
    unsigned shutter=g.blinkShutter();
    if(shutter) {
        int cy=59+wave(g.tick/9)/160,dx=int(x)-64,curve=dx*dx/256;
        int edge=x<=44?99-int(x)*3/44:x<=84?96+(int(x)-44)*3/40:99;
        int opening=(64-int(g.tunnel())*49/100)*int(256-shutter)/256;
        if(y<cy-opening+curve*int(shutter)/256 || y>=cy+opening-curve*int(shutter)/256)return false;
    }
    double t=g.tunnel(),rx=84-.62*t+bloom,ry=64-.49*t+bloom;
    return (x-64)*(x-64)/(rx*rx)+(y-59)*(y-59)/(ry*ry)<1;
}
static int width(int kind){return obstacle(kind).radius;}
static double center(const Game &g,int y){return 64-g.position*(2+(y-44)*76/54)/1024.0+g.bend()*(98-y)*(98-y)/(54.0*54);}
static Observation observe(const Game &g,const Profile &p,Random &r,const Observation &old,unsigned input){
    Observation o=old;bool blind=g.blinkClosed() && !p.fullVision;
    // The cockpit stays visible while the windshield shutter is closed.
    o.sleep=g.sleep;o.speed=g.speed;o.damage=g.damage;
    o.shoulder=g.offroad>0; // gravel sound / rumble remains available
    // The centre stripe at y=80 is a lateral cue. When the eyelid hides it,
    // keep an imperfect dead-reckoned estimate rather than reading position.
    int reference=80-int(g.tunnel())/9;
    o.laneVisible=visible(g,center(g,reference),reference,p.fullVision);
    int noise=p.noise+g.tunnel()*2;
    if(o.laneVisible)o.position=g.position+r.noise(noise);
    else o.position=old.position+(((input&Right)?7:0)-((input&Left)?7:0))*p.sample+r.noise(15);
    o.velocity=std::clamp((o.position-old.position)/p.sample,-12,12);
    int z=g.signZ();double y=44+scenery::GROUND_SCALE*Q/double(std::max(0,z)*Q+scenery::nearDepth(g.speed));
    double x=center(g,int(y))+g.sign.side*(2+(y-44)*76/54+22);
    if(!blind && z>-48 && z<145 && g.sign.collected!=(1u<<arrowCount(g.sign.kind))-1 && ((x>-5 && x<133) || z<80)){
        o.signId=g.sign.at;o.signSide=g.sign.side;o.signZ=z;
    } else {
        o.signZ-=std::max(1,o.speed*p.sample/216);
        if(z<-48 || g.sign.collected==(1u<<arrowCount(g.sign.kind))-1 || o.signZ<-48)o.signSide=0;
    }
    z=g.hazardZ();y=44+scenery::GROUND_SCALE*Q/double(std::max(0,z)*Q+scenery::nearDepth(g.speed));
    x=center(g,int(y))+g.hazardX()*(2+(y-44)*76/54)/1024.0;
    if(!blind && z>0 && g.hazard.warned && !g.hazard.resolved){
        o.hazardId=g.hazard.at;o.hazardKind=g.hazard.kind;o.hazardZ=z;
        // Estimate the crossing trajectory, with uncertainty. It is only
        // available after the object has entered the visible windshield.
        double ttc=z*3.6/std::max(1,int(g.speed));
        bool seen=visible(g,x,y-5,p.fullVision,2);
        int predicted=g.hazard.kind==Branch?g.hazardX():seen?g.hazardX()-int(g.hazard.side*obstacle(g.hazard.kind).crossingSpeed*ttc):-g.hazard.side*550;
        if(g.hazard.kind!=Branch || seen)o.hazardLane=predicted+r.noise(85+g.tunnel());
        else o.hazardLane=old.hazardId==g.hazard.at?old.hazardLane:0;
    } else {
        o.hazardZ-=std::max(1,o.speed*p.sample/216);
        if(o.hazardZ<-18)o.hazardKind=-1;
    }
    return o;
}
struct Result {
    int won=0,road=0,sleepLoss=0,hitsLoss=0,nearObstacle=0,nearShoulder=0,curveLoss=0,missedLights=0;
    double metres=0,seconds=0,brakeSeconds=0,sleepMean=0,darkSeconds=0,blackSeconds=0,minSpeedSeconds=0;
    double firstLightSeconds=-1,firstNarrowSeconds=-1,firstBlinkSeconds=-1,secondLightSeconds=-1;
    int lights=0,sites=0,hits=0,censored=0,holdViolations=0;
};
static Result drive(unsigned seed,const Profile &profile,bool trace=false){
    Game g;g.begin(seed);Random r{seed^0x8137};
    Observation history[128],latest;
    unsigned input=0,signId=0;bool takeLight=true;
    int lapse=0,brakeUntil=0,nextSpeedCheck=0;bool edge=false,speedCorrection=false;Result result;
    std::vector<unsigned> taken;
    for(int frame=0;frame<120000 && g.screen==Drive && g.distance<20000*Q;++frame){
        if(frame%profile.sample==0)latest=observe(g,profile,r,latest,input);
        history[frame%128]=latest;
        Observation o=frame>=profile.reaction?history[(frame-profile.reaction)%128]:Observation{};
        if(frame%profile.sample==0){
            if(lapse>0)lapse-=profile.sample;
            else if(r.next(profile.lapseEvery)==0)lapse=profile.lapseFrames+int(r.next(15));
            if(lapse<=0){
                int target=profile.seekLight?280:0;input=0;
                int comfortable=profile.brakeTtc>=8?65:82;
                if(o.sleep>6500)comfortable-=6;
                comfortable-=o.damage/5;
                if(o.signId && o.signId!=signId){signId=o.signId;takeLight=r.next(100)<unsigned(o.sleep>6500?97:88);}
                if(profile.seekLight && o.signSide && takeLight)target=o.signSide*ARROW_LANE;
                double ttc=o.hazardZ*3.6/std::max(1,o.speed);
                if(o.hazardKind>=0 && ttc<profile.brakeTtc && o.hazardZ>-5){
                    int margin=width(o.hazardKind)+155;
                    if(std::abs(o.position-o.hazardLane)<margin+120){
                        target=o.position>o.hazardLane?o.hazardLane+margin:o.hazardLane-margin;
                        if(std::abs(target)>760)target=o.position>o.hazardLane?o.hazardLane-margin:o.hazardLane+margin;
                    }
                    target=std::clamp(target,-760,760);
                    brakeUntil=frame+profile.brakeTail;
                }
                if(frame<brakeUntil && !profile.noBrake && (profile.brakeTtc>=8 || o.speed>comfortable-12))input|=Brake;
                if(profile.managePedal && frame>=nextSpeedCheck) {
                    nextSpeedCheck=frame+(profile.reaction>20?240:180);
                    if(o.speed>comfortable+3)speedCorrection=true;
                }
                if(speedCorrection && o.speed<comfortable-8)speedCorrection=false;
                if(speedCorrection && !profile.noBrake)input|=Brake;
                if(o.shoulder)target=0;
                int predicted=o.position+int(o.velocity*(profile.reaction+profile.sample/2)*profile.anticipation);
                if(predicted<target-90)input|=Right;
                else if(predicted>target+90)input|=Left;
            }
        }
        // The experimental driver never releases B, including during
        // attention lapses, from 0.25 seconds after the route starts.
        if(profile.holdBrake && frame>=15)input|=Brake;
        if(profile.holdBrake && frame>=15 && !(input&Brake))++result.holdViolations;
        unsigned oldSign=g.sign.at;bool used=g.sign.taken;
        bool oldResolved=g.hazard.resolved;int kind=g.hazard.kind;
        g.update(input);
        // Counterfactual only: isolate fatigue's contribution without
        // changing the shipped game or giving the ordinary pilot this aid.
        if(profile.noFatigue)g.sleep=g.drowsy=g.blinkPhase=g.blinkCycle=0;
        if(g.events&Neon) {
            if(result.firstLightSeconds<0)result.firstLightSeconds=(frame+1)/60.0;
            if(!used) {++result.sites;if(result.sites==2)result.secondLightSeconds=(frame+1)/60.0;}
        }
        if(g.tunnel()==100 && result.firstNarrowSeconds<0)result.firstNarrowSeconds=(frame+1)/60.0;
        if(g.blinkClosed() && result.firstBlinkSeconds<0)result.firstBlinkSeconds=(frame+1)/60.0;
        if(g.blinkClosed())result.blackSeconds+=1.0/60;
        if(g.speed==MIN_SPEED)result.minSpeedSeconds+=1.0/60;
        if(input&Brake)result.brakeSeconds+=1.0/60;
        result.sleepMean+=g.sleep;
        if(g.tunnel()>=90)result.darkSeconds+=1.0/60;
        if(g.sign.at!=oldSign && !used)++result.missedLights;
        if(!oldResolved && g.hazard.resolved && !(g.events&Impact)){
            int gap=std::abs(g.position-g.hazardX())-width(kind);
            if(gap>=0 && gap<300)++result.nearObstacle;
        }
        bool near=std::abs(g.position)>850;
        if(near && !edge)++result.nearShoulder;
        if(near)edge=true;else if(std::abs(g.position)<700)edge=false;
        if(trace)std::printf("%d,%u,%d,%u,%u,%u,%d,%u,%d,%d\n",frame,g.distance/Q,g.position,g.sleep,g.speed,input,g.bend(),g.tunnel(),o.laneVisible,lapse>0);
    }
    result.lights=g.lights;result.hits=g.incidents;result.censored=g.screen==Drive;
    result.won=g.distance>=4000*Q;result.metres=g.distance/double(Q);result.seconds=g.tick/60.0;
    result.sleepMean=g.tick?result.sleepMean/g.tick/100:0;
    if(g.screen!=Drive){result.road=g.reason==Road;}
    result.curveLoss=result.road && std::abs(g.bend())>=8 && g.tumble==0;
    return result;
}
int main(int argc,char **argv){
    Profile profiles[]={
        {"relaxed_human",8,14,32,160,24,3,22,.70},
        {"slower_reactions",12,24,45,130,36,3,55,.55},
        {"overcautious_braking",8,14,32,160,24,8,240,.70},
        {"never_brakes",8,14,32,160,24,3,22,.70,true,false,true,false},
        {"human_full_vision_control",8,14,32,160,24,3,22,.70,true,true,false},
        {"ignores_neon",8,14,32,160,24,3,22,.70,false,false,false},
        {"only_brakes_for_obstacles",8,14,32,160,24,3,22,.70,true,false,false,false}
    };
    if(argc>1 && !std::strcmp(argv[1],"--hold-brake-study")) {
        unsigned runs=argc>2?unsigned(std::atoi(argv[2])):128;
        Profile study[]={
            profiles[0],
            {"holds_brake",8,14,32,160,24,3,22,.70,true,false,false,false,true},
            {"holds_brake_full_vision",8,14,32,160,24,3,22,.70,true,true,false,false,true},
            {"holds_brake_no_fatigue",8,14,32,160,24,3,22,.70,true,false,false,false,true,true}
        };
        printf("{\"runs_per_profile\":%u,\"brake_from_seconds\":0.25,\"cap_metres\":20000,\"cap_seconds\":2000,\"profiles\":[",runs);
        for(unsigned p=0;p<4;++p) {
            printf("%s{\"name\":\"%s\",\"runs\":[",p?",":"",study[p].name);
            for(unsigned i=0;i<runs;++i) {
                auto a=drive((i+1)*937,study[p]);
                printf("%s{\"seed\":%u,\"metres\":%.3f,\"seconds\":%.3f,\"brake_seconds\":%.3f,\"near_dark_seconds\":%.3f,\"black_seconds\":%.3f,\"min_speed_seconds\":%.3f,\"mean_sleep_percent\":%.3f,\"first_light_seconds\":%.3f,\"wakeups\":%d,\"sites_collected\":%d,\"first_narrow_seconds\":%.3f,\"first_blink_seconds\":%.3f,\"second_light_seconds\":%.3f,\"hits\":%d,\"censored\":%d,\"hold_violations\":%d}",
                    i?",":"",(i+1)*937,a.metres,a.seconds,a.brakeSeconds,a.darkSeconds,a.blackSeconds,a.minSpeedSeconds,a.sleepMean,a.firstLightSeconds,a.lights,a.sites,a.firstNarrowSeconds,a.firstBlinkSeconds,a.secondLightSeconds,a.hits,a.censored,a.holdViolations);
            }
            printf("]}");
        }
        puts("]}");return 0;
    }
    if(argc>1 && !std::strcmp(argv[1],"--trace")){
        puts("frame,metres,position,sleep,speed,buttons,bend,tunnel,lane_visible,lapse");
        drive(argc>2?std::strtoul(argv[2],nullptr,10):937,profiles[0],true);return 0;
    }
    unsigned runs=argc>1?unsigned(std::atoi(argv[1])):64;
    std::printf("{\"runs_per_profile\":%u,\"profiles\":[",runs);
    for(unsigned p=0;p<sizeof profiles/sizeof profiles[0];++p){
        Result total;std::vector<int> nearCounts;
        for(unsigned i=0;i<runs;++i){auto a=drive((i+1)*937,profiles[p]);
            total.won+=a.won;total.road+=a.road;total.sleepLoss+=a.sleepLoss;total.hitsLoss+=a.hitsLoss;
            total.nearObstacle+=a.nearObstacle;total.nearShoulder+=a.nearShoulder;total.curveLoss+=a.curveLoss;
            total.missedLights+=a.missedLights;total.metres+=a.metres;total.seconds+=a.seconds;
            total.darkSeconds+=a.darkSeconds;total.brakeSeconds+=a.brakeSeconds;total.sleepMean+=a.sleepMean;nearCounts.push_back(a.nearObstacle+a.nearShoulder);
        }
        std::sort(nearCounts.begin(),nearCounts.end());
        std::printf("%s{\"name\":\"%s\",\"reaction_ms\":%d,\"decision_ms\":%d,\"survives_4km\":%d,\"road_losses\":%d,\"sleep_losses\":%d,\"collision_losses\":%d,\"curve_losses\":%d,\"mean_metres\":%.1f,\"mean_seconds\":%.1f,\"mean_braking_seconds\":%.1f,\"mean_sleep_percent\":%.1f,\"mean_missed_lights\":%.2f,\"mean_obstacle_near_misses\":%.2f,\"mean_shoulder_near_misses\":%.2f,\"median_near_misses\":%d,\"mean_near_dark_seconds\":%.1f}",
            p?",":"",profiles[p].name,profiles[p].reaction*1000/60,profiles[p].sample*1000/60,total.won,total.road,total.sleepLoss,total.hitsLoss,total.curveLoss,total.metres/runs,total.seconds/runs,total.brakeSeconds/runs,total.sleepMean/runs,total.missedLights/double(runs),total.nearObstacle/double(runs),total.nearShoulder/double(runs),nearCounts[runs/2],total.darkSeconds/runs);
    }
    puts("]}");
}
