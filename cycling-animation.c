#include "cycling-animation.h"
#include <string.h>
/* Each quarter can change palette independently during the native cycle. */
bool cycling_pose_find(const uint8_t oam[256],HeroPoseMatch *out) {
    static const int tiles[4]={86,90,90,90},flags[4]={0,0,192,128};
    memset(out,0,sizeof(*out));
    for(int seed=0;seed<64;seed++)for(int part=0;part<4;part++) {
        const uint8_t *s=oam+seed*4;
        if(s[0]>=207||s[1]!=tiles[part]||(s[2]&224)!=flags[part]||(s[2]&3)>2)continue;
        HeroPoseMatch m={.x=s[3]-(part%2)*8,.y=s[0]+1-(part/2)*8,.palette=s[2]&3};
        int count=0;bool valid=true;
        for(int k=0;k<4;k++) {
            int x=m.x+(k%2)*8,y=m.y+(k/2)*8;
            if(x<0||x>=256||y<1||y>=208)continue;
            int found=-1;
            for(int slot=0;slot<64;slot++) {
                const uint8_t *q=oam+slot*4;
                if(q[0]<207&&q[3]==x&&q[0]+1==y&&q[1]==tiles[k]&&
                   (q[2]&224)==flags[k]&&(q[2]&3)<=2){found=slot;break;}
            }
            if(found<0)continue;m.slots[found]=true;count++;
        }
        if(valid&&count>=2){
            for(int slot=0;slot<64;slot++){const uint8_t *q=oam+slot*4;
                if(q[0]<207&&q[1]==91&&(q[2]&224)==0&&(q[2]&3)<=2&&q[3]==m.x+4&&q[0]+1==m.y+4){
                    m.pose=1;m.slots[slot]=true;break;
                }}
            *out=m;return true;
        }
    }
    return false;
}

