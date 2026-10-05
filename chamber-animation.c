#include "chamber-animation.h"
#include <string.h>
bool chamber_pose_find(const uint8_t oam[256],HeroPoseMatch *out) {
    static const int top[5]={251,253,255,193,195},bottom[5]={252,254,192,194,196};
    memset(out,0,sizeof(*out));
    for(int pose=0;pose<5;pose++)for(int seed=0;seed<64;seed++)for(int part=0;part<4;part++) {
        const uint8_t *s=oam+seed*4;int tile=part<2?top[pose]:bottom[pose],attr=(part%2)*64;
        if(s[0]>=207||s[1]!=tile||(s[2]&227)!=attr)continue;
        HeroPoseMatch m={.pose=pose,.x=s[3]-(part%2)*8,.y=s[0]+1-(part/2)*8};
        int count=0;bool valid=true;
        for(int k=0;k<4;k++) {
            int x=m.x+(k%2)*8,y=m.y+(k/2)*8;if(x<0||x>=256||y<1||y>=208)continue;
            int found=-1;
            for(int slot=0;slot<64;slot++){const uint8_t *q=oam+slot*4;
                if(q[0]<207&&q[0]+1==y&&q[3]==x&&q[1]==(k<2?top[pose]:bottom[pose])&&
                   (q[2]&227)==(k%2)*64){found=slot;break;}}
            if(found<0){valid=false;break;}m.slots[found]=true;count++;
        }
        if(valid&&count>=2){*out=m;return true;}
    }
    return false;
}
