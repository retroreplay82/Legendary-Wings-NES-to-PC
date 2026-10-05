#include "halo-animation.h"
#include <string.h>
bool halo_pose_find(const uint8_t oam[256],HeroPoseMatch *out) {
    memset(out,0,sizeof(*out));
    for(int seed=0;seed<64;seed++) {
        const uint8_t *s=oam+seed*4;
        if(s[0]>=207||s[1]!=94||((s[2]&227)!=2&&(s[2]&227)!=66))continue;
        int left=s[3]-((s[2]&64)?8:0),y=s[0]+1,count=0;
        HeroPoseMatch m={.x=left,.y=y,.palette=2};bool valid=true;
        for(int k=0;k<2;k++) {
            int x=left+k*8;if(x<0||x>=256)continue;
            int found=-1;
            for(int slot=0;slot<64;slot++){const uint8_t *q=oam+slot*4;
                if(q[0]<207&&q[0]+1==y&&q[3]==x&&q[1]==94&&(q[2]&227)==2+k*64){found=slot;break;}}
            if(found<0){valid=false;break;}m.slots[found]=true;count++;
        }
        if(valid&&count){*out=m;return true;}
    }
    return false;
}
