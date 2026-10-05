#include "serpent-animation.h"
#include <string.h>
bool serpent_pose_find(const uint8_t oam[256],HeroPoseMatch *out) {
    memset(out,0,sizeof(*out));
    for(int seed=0;seed<64;seed++)for(int pose=0;pose<3;pose++) {
        const uint8_t *s=oam+seed*4;
        int columns=pose?2:3,parts=columns*2;
        if(s[0]>=207||(s[2]&3))continue;
        for(int part=0;part<parts;part++) {
            int col=part%columns,row=part/columns;
            int tile=pose?179+pose:176+col,attr=pose?col*64+row*128:row*128;
            if(s[1]!=tile||(s[2]&0xe3)!=attr)continue;
            HeroPoseMatch match={.pose=pose,.x=s[3]-col*8,.y=s[0]+1-row*8};
            bool valid=true;int count=0;
            for(int k=0;k<parts;k++) {
                col=k%columns;row=k/columns;
                int x=match.x+col*8,y=match.y+row*8;
                if(x<0||x>=256||y<1||y>=208)continue;
                tile=pose?179+pose:176+col;attr=pose?col*64+row*128:row*128;
                int found=-1;
                for(int slot=0;slot<64;slot++) {
                    const uint8_t *q=oam+slot*4;
                    if(q[0]<207&&q[3]==x&&q[0]+1==y&&q[1]==tile&&(q[2]&0xe3)==attr){found=slot;break;}
                }
                if(found<0){valid=false;break;}match.slots[found]=true;count++;
            }
            if(valid&&count>=2){*out=match;return true;}
        }
    }
    return false;
}
