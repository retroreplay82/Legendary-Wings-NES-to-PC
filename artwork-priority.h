#ifndef ARTWORK_PRIORITY_H
#define ARTWORK_PRIORITY_H
#include <stdint.h>
#include <stdbool.h>
void artwork_priority_prepare(uint8_t owner[512*480],const bool removed[64]);
bool artwork_priority_claim(uint8_t owner[512*480],int slot,int x,int y);
#endif
