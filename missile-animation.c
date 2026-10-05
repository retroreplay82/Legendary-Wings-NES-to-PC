#include "missile-animation.h"
#include <string.h>
/* Palette-alternating native missile segments share one vertical column. */
static bool segment(const uint8_t *s) {
    return s[0]<207&&s[1]==83&&(s[2]&224)==0&&((s[2]&3)==0||(s[2]&3)==2);
}
bool missile_pose_find(const uint8_t oam[256],HeroPoseMatch *out) {
    memset(out,0,sizeof(*out));
    for(int seed=0;seed<64;seed++) {
        const uint8_t *s=oam+seed*4;if(!segment(s))continue;
        int x=s[3],top=s[0]+1;bool changed=true;
        while(changed){changed=false;for(int i=0;i<64;i++){const uint8_t *q=oam+i*4;
            if(segment(q)&&q[3]==x&&q[0]+9==top){top-=8;changed=true;break;}}}
        HeroPoseMatch m={.x=x,.y=top};
        for(int y=top;y<208;y+=8){int found=-1;
            for(int i=0;i<64;i++){const uint8_t *q=oam+i*4;
                if(segment(q)&&q[3]==x&&q[0]+1==y){found=i;break;}}
            if(found<0)break;m.slots[found]=true;m.pose++;
        }
        if(m.pose){*out=m;return true;}
    }
    return false;
}
