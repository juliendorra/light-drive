#pragma once
#include <stdint.h>
namespace cardscore {
// Read-only migration from releases that saved only distance in LIGHTDRV.SAV.
bool load(uint32_t &best);
}
