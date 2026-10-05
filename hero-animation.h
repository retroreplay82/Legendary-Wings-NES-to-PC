#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct { int pose,x,y,palette,flash; bool flip,slots[64]; } HeroPoseMatch;
/* Pose 0 raised, 1 lowered, 2 level. Native OAM drives timing and position. */
bool hero_pose_find(const uint8_t oam[256],HeroPoseMatch *out);
