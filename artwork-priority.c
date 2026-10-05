#include "artwork-priority.h"
#include "cyc_render.h"
#include <string.h>
void artwork_priority_prepare(uint8_t owner[512*480],const bool removed[64]) {
    memset(owner,64,512*480);
    const uint8_t *oam=cyc_render_oam(),*opaque=cyc_frame_bg_opaque();
    for(int slot=0;slot<64;slot++) {
        const uint8_t *s=oam+slot*4;if(removed[slot]||s[0]>=239)continue;
        int y0=s[0]+1;
        for(int row=0;row<16;row++) {
            int y=y0+row;if(y>=240)continue;
            uint8_t mask=cyc_render_line_mask(y);if(!(mask&16))continue;
            bool tall=cyc_render_line_sprite16(y);int height=tall?16:8;
            if(row>=height)continue;
            int sy=(s[2]&128)?height-1-row:row;
            uint16_t pattern=tall?(s[1]&1)*4096+(s[1]&254)*16+(sy/8)*16:
                cyc_render_line_sprite_table(y)+s[1]*16;
            uint8_t lo=cyc_render_chr(pattern+(sy&7)),hi=cyc_render_chr(pattern+(sy&7)+8);
            for(int col=0;col<8;col++) {
                int x=s[3]+col;if(x>=256||(x<8&&!(mask&4)))continue;
                int bit=(s[2]&64)?col:7-col;
                if(!(((lo>>bit)|(hi>>bit))&1))continue;
                if((s[2]&32)&&opaque[y*256+x])continue;
                int at=y*1024+x*2;
                for(int dy=0;dy<2;dy++)for(int dx=0;dx<2;dx++)
                    if(owner[at+dy*512+dx]>slot)owner[at+dy*512+dx]=slot;
            }
        }
    }
}
bool artwork_priority_claim(uint8_t owner[512*480],int slot,int x,int y) {
    if(slot<0||slot>=64||x<0||x>=512||y<0||y>=480)return false;
    uint8_t mask=cyc_render_line_mask(y/2);
    if(!(mask&16)||(x<16&&!(mask&4)))return false;
    uint8_t *pixel=owner+y*512+x;
    if(*pixel<slot)return false;
    *pixel=slot;return true;
}
