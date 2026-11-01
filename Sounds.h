#pragma once
#include <chgame/Audio.h>
#include "Game.h"
enum class Sfx : uint8_t { Neon, Gravel, Wildlife, Bump, COUNT };
extern const audio::Effect SOUNDS[];
void soundscape(const night::Game &game);
