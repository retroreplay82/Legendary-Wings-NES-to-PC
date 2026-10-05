#include "turret-animation.h"
#include <string.h>
bool turret_pose_find(const uint8_t oam[256],HeroPoseMatch *out) {
    static const int px[5]={0,8,16,4,12},py[5]={0,0,0,8,8};
    static const int attrs[5]={1,1,65,2,66};
    memset(out,0,sizeof(*out));
    for(int pose=0;pose<2;pose++)for(int seed=0;seed<64;seed++)for(int part=0;part<5;part++) {
        const uint8_t *s=oam+seed*4;int tile=part==1?199:part<3?198:200+pose;
        if(s[0]>=207||s[1]!=tile||(s[2]&227)!=attrs[part])continue;
        HeroPoseMatch m={.pose=pose,.x=s[3]-px[part],.y=s[0]+1-py[part]};
        int count=0;bool valid=true;
        for(int k=0;k<5;k++) {
            int x=m.x+px[k],y=m.y+py[k];if(x<0||x>=256||y<1||y>=208)continue;
            int found=-1;
            for(int slot=0;slot<64;slot++){const uint8_t *q=oam+slot*4;
                if(q[0]<207&&q[0]+1==y&&q[3]==x&&q[1]==(k==1?199:k<3?198:200+pose)&&
                   (q[2]&227)==attrs[k]){found=slot;break;}}
            if(found<0){valid=false;break;}m.slots[found]=true;count++;
        }
        if(valid&&count>=2){*out=m;return true;}
    }
    return false;
}
