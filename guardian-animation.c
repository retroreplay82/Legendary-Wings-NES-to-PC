#include "guardian-animation.h"
#include <string.h>
bool guardian_pose_find(const uint8_t oam[256],HeroPoseMatch *out) {
    memset(out,0,sizeof(*out));
    for(int seed=0;seed<64;seed++) {
        const uint8_t *s=oam+seed*4;if(s[0]>=239||(s[2]&3)!=3)continue;
        for(int pose=0;pose<4;pose++)for(int flip=0;flip<(pose>=2?1:2);flip++)for(int part=0;part<4;part++) {
            int tile=pose==3?160+part:pose==2?188:part<2?(pose?186:182):183;
            int attr=pose==3?3:35+(part%2)*64+(pose==2?(part/2)*128:flip*128);
            if(s[1]!=tile||(s[2]&0xe3)!=attr)continue;
            int px=(part%2)*8,py=pose>=2?(part/2)*8:flip?8-(part/2)*8:(part/2)*8;
            HeroPoseMatch match={.pose=pose,.flip=pose<2&&flip,.x=s[3]-px,.y=s[0]+1-py};
            bool valid=true;int count=0;
            for(int k=0;k<4;k++) {
                int x=match.x+(k%2)*8,y=match.y+(pose>=2?(k/2)*8:flip?8-(k/2)*8:(k/2)*8);
                if(x<0||x>=256||y<1||y>=240)continue;
                tile=pose==3?160+k:pose==2?188:k<2?(pose?186:182):183;
                attr=pose==3?3:35+(k%2)*64+(pose==2?(k/2)*128:flip*128);
                int found=-1;
                for(int slot=0;slot<64;slot++) {
                    const uint8_t *q=oam+slot*4;
                    if(q[0]<239&&q[3]==x&&q[0]+1==y&&q[1]==tile&&(q[2]&0xe3)==attr){found=slot;break;}
                }
                if(found<0){valid=false;break;}match.slots[found]=true;count++;
            }
            if(valid&&count>=2){*out=match;return true;}
        }
    }
    return false;
}


