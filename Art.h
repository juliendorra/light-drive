#pragma once
#include <stdint.h>
namespace art {
enum Sprite : uint8_t { Deer, Fox, Hare, Owl, Crow, Pine, Birch, Oak, Branch, Shrub, Diner, Motel, Gas, Shop, RoadArrow };
void draw(Sprite sprite,unsigned frame,int center,int foot,int height,bool mirror=false,bool distant=false);
struct GroundClip {const uint8_t *edge;uint8_t from=224;bool right=false;};
// Screen coordinates and height are Q8. Wind bends the crown in Q8 screen
// pixels; the trunk/root anchor stays fixed. Ground bounds have 99 rows.
void projected(Sprite sprite,unsigned frame,int x,int foot,int height,int wind,
               const GroundClip &ground,bool mirror=false,bool distant=false);
}
