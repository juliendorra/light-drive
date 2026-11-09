#!/usr/bin/env python3
"""Test the reused Pinball PWM state machine and Light Drive feedback patterns."""
from pathlib import Path
import os
import shutil
import subprocess

ROOT=Path(__file__).resolve().parents[1]
out=ROOT/'out/rumble';out.mkdir(parents=True,exist_ok=True)
(out/'Arduino.h').write_text('#pragma once\n#include <stdint.h>\nuint32_t millis();\n')
(out/'test.cpp').write_text(r'''
#include <cassert>
#include "Game.h"
#include "Feedback.h"
#include "src/rumble/Rumble.h"
static uint32_t now;
uint32_t millis(){return now;}
int main(){
    night::Game g;feedback::begin();feedback::update(g);assert(rumble::level()==0);
    g.screen=night::Drive;feedback::update(g);
    g.position=1200;g.offroad=1;g.tick=1;feedback::update(g);
    assert(rumble::level()==120 && rumble::remaining()==32);
    now+=32;assert(rumble::level()==0);
    g.tick+=14;feedback::update(g);assert(rumble::level()==120);
    g.events=night::Impact;feedback::update(g);assert(rumble::level()==255 && rumble::remaining()==140);
    g.events=0;g.tick+=14;feedback::update(g);assert(rumble::level()==255); // small pulse cannot interrupt a hit
    now+=140;assert(!rumble::level());
    g.position=0;g.events=night::Neon;feedback::update(g);assert(rumble::level()==110 && rumble::remaining()==35);
    g.events=0;now+=35;feedback::update(g);assert(!rumble::level());
    g.screen=night::Paused;feedback::update(g);assert(!rumble::level());
    g.screen=night::Drive;feedback::update(g);
    g.screen=night::Tumble;feedback::update(g);assert(rumble::level()==130);
    unsigned pulseStarts=1;uint8_t previous=rumble::level();uint32_t start=now;
    for(unsigned frame=1;frame<=night::ROLLOVER_FRAMES;++frame){
        now=start+frame*1000/60;g.tumble=frame;feedback::update(g);auto level=rumble::level();
        auto contact=night::rolloverContact(frame);
        if(contact.milliseconds)assert(level==contact.strength);
        if(level && !previous)++pulseStarts;
        assert(rumble::remaining()<=500);previous=level;
    }
    assert(pulseStarts==8 && !rumble::level());
    g.screen=night::Rest;feedback::update(g);assert(!rumble::level()); // no repeated tumble on result screen
    g.screen=night::Drive;feedback::update(g);g.screen=night::Rest;feedback::update(g);assert(rumble::level()==255);
    g.screen=night::Title;feedback::update(g);assert(!rumble::level());
    for(unsigned kind=0;kind<night::CREATURE_COUNT;++kind) {
        g.screen=night::Drive;g.hazard.kind=kind;g.events=night::Impact;feedback::update(g);
        auto spec=night::obstacle(kind);assert(rumble::level()==spec.rumble && rumble::remaining()==spec.rumbleMs);
        now+=spec.rumbleMs;assert(!rumble::level());
    }
    g.screen=night::Drive;g.haptics=false;g.events=night::Impact;feedback::update(g);assert(!rumble::level());
    rumble::setEnabled(true);rumble::pulse(210,60000);assert(rumble::remaining()==500);
    now+=500;assert(!rumble::level());
    now=0xfffffff0u;rumble::update();rumble::pulse(180,40);now=24;assert(!rumble::level());
}
''')
cxx=os.environ.get('CHSIM_CXX') or shutil.which('clang++') or shutil.which('c++')
subprocess.run([cxx,'-std=c++17','-DCHSIM','-fsanitize=undefined,bounds','-fno-sanitize-recover=all',
    '-I'+str(out),'-I'+str(ROOT),str(out/'test.cpp'),str(ROOT/'Feedback.cpp'),
    str(ROOT/'src/rumble/Rumble.cpp'),str(ROOT/'Game.cpp'),str(ROOT/'Rollover.cpp'),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],check=True)
print('PASS: shoulder taps, priority hit, short arrow pickup tick, eight frame-synchronized rollover pulses, pause/menu/disable, cap and clock wrap')
