#include "pod-animation.h"
#include <string.h>
bool pod_pose_find(const uint8_t oam[256],HeroPoseMatch *out) {
    memset(out,0,sizeof(*out));
    for(int seed=0;seed<64;seed++) {
        const uint8_t *s=oam+seed*4;
        if(s[0]>=239||(s[1]!=131&&s[1]!=132)||(s[2]&0xa3))continue;
        HeroPoseMatch match={.pose=2,.x=s[3]-((s[2]&64)?8:0),.y=s[0]+1-(s[1]-131)*8};
        int count=0;bool valid=true;
        for(int k=0;k<4;k++) {
            int px=match.x+(k%2)*8,py=match.y+(k/2)*8;
            if(px<0||px>=256||py<1||py>=240)continue;
            int found=-1;
            for(int slot=0;slot<64;slot++) {
                const uint8_t *q=oam+slot*4;
                if(q[0]<239&&q[3]==px&&q[0]+1==py&&q[1]==131+k/2&&
                   (q[2]&0xe3)==(k%2)*64){found=slot;break;}
            }
            if(found<0){
                /* Native OAM rotation can omit individual quadrants. A
                 * conflicting occupied position is not an omitted part. */
                for(int slot=0;slot<64;slot++){const uint8_t *q=oam+slot*4;
                    if(q[0]<239&&q[3]==px&&q[0]+1==py){valid=false;break;}}
                if(!valid)break;continue;
            }match.slots[found]=true;count++;
        }
        if(valid&&count>=1){*out=match;return true;}
    }
    for(int seed=0;seed<64;seed++) {
        const uint8_t *s=oam+seed*4;
        if(s[0]>=239||s[1]<133||s[1]>140||(s[2]&0xa3))continue;
        int pose=(s[1]-133)/4,part=(s[1]-133)%4;
        bool flip=(s[2]&64)!=0;
        int x=s[3]-(flip?8-(part%2)*8:(part%2)*8);
        int y=s[0]+1-(part/2)*8,count=0;
        HeroPoseMatch match={.x=x,.y=y,.pose=pose,.flip=flip};
        bool valid=true;
        for(int k=0;k<4;k++) {
            int px=x+(flip?8-(k%2)*8:(k%2)*8),py=y+(k/2)*8;
            if(px<0||px>=256||py<1||py>=240)continue;
            int found=-1;
            for(int slot=0;slot<64;slot++) {
                const uint8_t *q=oam+slot*4;
                if(q[0]<239&&q[3]==px&&q[0]+1==py&&q[1]==133+pose*4+k&&
                   (q[2]&0xe3)==(flip?64:0)){found=slot;break;}
            }
            if(found<0){
                for(int slot=0;slot<64;slot++){const uint8_t *q=oam+slot*4;
                    if(q[0]<239&&q[3]==px&&q[0]+1==py){valid=false;break;}}
                if(!valid)break;continue;
            }
            match.slots[found]=true;count++;
        }
        if(valid&&count>=1){*out=match;return true;}
    }
    return false;
}

