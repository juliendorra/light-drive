#include "Sounds.h"
AUDIO_STEPS(NEON)={AUDIO_STEP(1100,0,45)};
AUDIO_STEPS(GRAVEL)={AUDIO_STEP(180,120,28)};
AUDIO_STEPS(WILDLIFE)={AUDIO_STEP(760,0,26),AUDIO_REST(140),AUDIO_STEP(660,0,30)};
AUDIO_STEPS(BUMP)={AUDIO_STEP(160,80,90),AUDIO_REST(80),AUDIO_STEP(220,160,45)};
const audio::Effect SOUNDS[]={
    AUDIO_EFFECT(NEON,4|audio::SOFT),AUDIO_EFFECT(GRAVEL,2|audio::SOFT|audio::GLIDE),
    AUDIO_EFFECT(WILDLIFE,3|audio::SOFT),AUDIO_EFFECT(BUMP,5|audio::SOFT|audio::GLIDE)
};
void soundscape(const night::Game &g) {
    using namespace night;
    audio::setOn(!g.muted && g.screen!=Paused && g.screen!=Help);
    if(g.events&Impact)audio::sfx(Sfx::Bump);
    else if(g.events&Neon)audio::sfx(Sfx::Neon);
    else if(g.events&Shoulder)audio::sfx(Sfx::Gravel);
    else if(g.events&Wildlife)audio::sfx(Sfx::Wildlife);
    else if(g.screen==Drive && !audio::playing()) {
        // Silence is the bed. One soft note about every six seconds, with
        // longer gaps when tired. No engine loop or polyphonic soundtrack.
        static const uint16_t notes[]={440,330,392,294,440,392,330,0};
        if(g.events&Chime) {unsigned n=notes[(g.tick/370)%8];if(n)audio::blip(n,70,true);}
        else if(g.events&Rain)audio::blip(1600,6,true);
    }
    audio::update();
}
