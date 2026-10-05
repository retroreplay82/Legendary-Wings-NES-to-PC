#include "burst-animation.h"
#include <string.h>
bool burst_pose_find(const uint8_t oam[256],HeroPoseMatch *out) {
    static const int tiles[9][4]={{125},{126,126,126,126},{127,112,113,114},
        {115,116,117,118},{114,113,112,127},{118,117,116,115},
        {119,120,121,122},{123},{124}};
    memset(out,0,sizeof(*out));
    for(int seed=0;seed<64;seed++)for(int pose=0;pose<9;pose++) {
        const uint8_t *s=oam+seed*4;bool small=pose==0||pose>=7;
        int parts=small?1:4;
        if(s[0]>=207||(s[2]&3))continue;
        for(int part=0;part<parts;part++) {
            int attr=pose==1?(part%2)*64+(part/2)*128:pose==4||pose==5?192:0;
            if(s[1]!=tiles[pose][part]||(s[2]&0xe3)!=attr)continue;
            HeroPoseMatch match={.pose=pose,.x=s[3]-(part%2)*8,.y=s[0]+1-(part/2)*8};
            bool valid=true;int count=0;
            for(int k=0;k<parts;k++) {
                int x=match.x+(k%2)*8,y=match.y+(k/2)*8;
                if(x<0||x>=256||y<1||y>=208)continue;
                attr=pose==1?(k%2)*64+(k/2)*128:pose==4||pose==5?192:0;
                int found=-1;
                for(int slot=0;slot<64;slot++) {
                    const uint8_t *q=oam+slot*4;
                    if(q[0]<207&&q[3]==x&&q[0]+1==y&&q[1]==tiles[pose][k]&&(q[2]&0xe3)==attr){found=slot;break;}
                }
                if(found<0){valid=false;break;}match.slots[found]=true;count++;
            }
            if(valid&&count>=(small?1:2)){*out=match;return true;}
        }
    }
    return false;
}
