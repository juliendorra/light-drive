#pragma once
#include <stdint.h>
#include "Rollover.h"

// Deterministic 60 Hz rules. Distances are metres * 256; lateral road edges
// are -1024 and +1024. No allocation, floating point, or platform clock.
namespace night {
constexpr int Q=256, ROAD_EDGE=1024;
constexpr unsigned ARROW_COUNT=6;
constexpr int ARROW_GAP=18, ARROW_LANE=480, ARROW_HALF_WIDTH=250;
constexpr int ARROW_RELIEF=800;
unsigned arrowCount(unsigned kind);
int arrowOffset(unsigned kind,unsigned arrow);
constexpr int START_SPEED=62, MAX_SPEED=130, MIN_SPEED=42;
constexpr unsigned BLINK_SLEEP=9000, BLINK_RAMP=1800;
enum Screen : uint8_t { Title, Help, Drive, Paused, Rest, Tumble=6 };
enum Key : uint8_t { Confirm=1, Brake=2, Left=4, Right=8, Start=16, Palette=32, Sound=64, Rumble=128 };
enum Event : uint16_t { Neon=1, Impact=2, Shoulder=4, Wildlife=8, Chime=32, Rain=64, Pass=128 };
enum Reason : uint8_t { Road=1 };
enum Creature : uint8_t { Deer, Branch, Fox, Hare, Owl, Birds, CREATURE_COUNT };
struct ObstacleSpec {
    uint16_t radius,leadFrames,crossingSpeed;
    uint8_t speedLoss,damage,slowFrames,rumble,rumbleMs;
};
const ObstacleSpec &obstacle(unsigned kind);
struct Sign {
    uint32_t at=650*Q;
    int8_t side=1;
    uint8_t kind=0, collected=0;
    bool taken=false;
};
struct Hazard {
    uint32_t at=480*Q;
    int16_t lane=0;
    int8_t side=-1;
    uint8_t kind=Deer;
    bool resolved=false, warned=false;
    uint16_t age=0;
};
struct Game {
    Screen screen=Title;
    Reason reason=Road;
    Sign sign;
    Hazard hazard;
    uint32_t tick=0, distance=0, best=0, rng=0x1751, seed=0x1751;
    int32_t driftRemainder=0;
    int16_t position=0, steering=0;
    uint16_t sleep=1800, offroad=0, recovery=0, slow=0, events=0, tumble=0, pedal=0, lights=0, avoided=0;
    uint16_t drowsy=0,blinkPhase=0,blinkCycle=0;
    uint8_t speed=START_SPEED, palette=0, previous=0, incidents=0, shock=0, damage=0;
    bool muted=false, braking=false, haptics=true;
    void begin(uint32_t routeSeed=0x1751);
    void update(uint8_t keys);
    int bend() const;
    int signZ() const;
    int hazardZ() const;
    int hazardX() const;
    int curveForce() const;
    int arrowZ(unsigned arrow) const;
    bool inLightLane() const;
    unsigned tunnel() const;
    unsigned blinkPeriod() const;
    unsigned blinkDuration() const;
    unsigned blinkShutter() const; // 0=open, 256=closed; fast eyelid transitions
    bool blinkClosed() const;
    unsigned random(unsigned n);
private:
    void stop(Reason why);
};
int wave(unsigned angle); // signed sine, amplitude 256, period 256
int absolute(int n);
const char *paletteName(unsigned p);
const char *hazardName(unsigned kind);
}
