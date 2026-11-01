#pragma once
#include <stdint.h>
namespace cardscore {
enum Status : uint8_t { Unavailable, Ready, Saved, Failed };
extern Status status;
// Uses the idle LCD chunk buffer; call only after gfx_wait(). Creates a
// two-sector journal in LIGHTDRV.SAV on a FAT16/FAT32 card if needed.
bool load(uint32_t &best);
bool store(uint32_t best,uint16_t wakeups,uint16_t avoided);
}
