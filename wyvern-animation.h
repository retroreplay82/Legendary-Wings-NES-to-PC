#pragma once
#include "hero-animation.h"
/* Canonical root retains the head/tail anchor as the wing opens. */
bool wyvern_pose_find(const uint8_t oam[256],HeroPoseMatch *out);
