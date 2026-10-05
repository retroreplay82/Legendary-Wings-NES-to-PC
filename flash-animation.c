#include "flash-animation.h"
#include <string.h>
bool flash_pose_find(const uint8_t oam[256],HeroPoseMatch *out) {
    memset(out,0,sizeof(*out));
    for(int seed=0;seed<64;seed++) {
        const uint8_t *s=oam+seed*4;
        if(s[0]>=207||s[1]!=95||((s[2]&227)!=66&&(s[2]&227)!=2))continue;
        int top=s[0]+1,x=s[3],attr=s[2]&227;
        bool changed=true;
        while(changed){changed=false;for(int i=0;i<64;i++) {
            const uint8_t *q=oam+i*4;
            if(q[0]<207&&q[1]==95&&(q[2]&227)==attr&&q[3]==x&&q[0]+9==top){top-=8;changed=true;break;}
        }}
        HeroPoseMatch m={.x=x,.y=top,.palette=2};
        for(int y=top;y<208;y+=8) {
            int found=-1;
            for(int i=0;i<64;i++){const uint8_t *q=oam+i*4;
                if(q[0]<207&&q[1]==95&&(q[2]&227)==attr&&q[3]==x&&q[0]+1==y){found=i;break;}}
            if(found<0)break;m.slots[found]=true;m.pose++;
        }
        if(m.pose){*out=m;return true;}
    }
    return false;
}
