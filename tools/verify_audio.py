#!/usr/bin/env python3
"""Exercise the actual game's audio routing through the pinned SDK timer model."""
from pathlib import Path
import os, subprocess
ROOT=Path(__file__).resolve().parents[1]
SDK=Path(os.environ.get('CHGAME_ROOT', ROOT/'.deps/CHGame'))
MODEL=SDK/'tools/audio/host'
LIB=SDK/'platform/board/arduino/CHGame/libraries/CHGame/src'
if not (MODEL/'harness.cpp').exists():raise SystemExit('Set CHGAME_ROOT to the pinned CHGame SDK checkout.')
OUT=ROOT/'out/audio-check';OUT.mkdir(parents=True,exist_ok=True)
(OUT/'timer_model.h').write_text((MODEL/'harness.cpp').read_text().split('int main(',1)[0])
light=OUT/'light_audio.cpp'
light.write_text('''#include "timer_model.h"
#include "Sounds.h"
#include <cassert>
int main() {
    audio::begin(SOUNDS, unsigned(Sfx::COUNT));
    night::Game game; game.screen=night::Drive; game.events=night::Chime;
    soundscape(game); osSystickHandler();
    assert(cen && 1000000u/(arr+1)==1760 && ccr==(arr+1)/8);
    // Hazard cues still take precedence over a coincident ambient chime.
    game.events=night::Chime|night::Impact; soundscape(game); osSystickHandler();
    for (unsigned us=0;us<1000;++us) stepMicrosecond();
    assert(cen && 1000000u/(arr+1)==160);
    game.screen=night::Paused; game.events=0; soundscape(game);
    assert(!cen && !ccr);
    puts("PASS: Light Drive higher-register soft chime, hazard precedence and pause silence");
}
''')

exe=OUT/"audio_test"
subprocess.run(["clang++","-std=c++17","-O1","-g","-fsanitize=undefined","-fno-sanitize-recover=all",f"-I{MODEL}",f"-I{LIB}",f"-I{OUT}",f"-I{ROOT}",str(light),str(ROOT/'Sounds.cpp'),str(LIB/"chgame/Audio.cpp"),"-o",str(exe)],check=True)
subprocess.run([str(exe)],check=True)
