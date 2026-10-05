#include "wyvern-animation.h"
#include <string.h>
typedef struct { int x,y; uint8_t tile; } Part;
static const int counts[2]={9,7};
static const Part poses[2][9]={
    {{0,8,242},{0,16,245},{0,24,247},{8,0,240},{8,8,243},{8,16,246},{8,24,248},{16,0,241},{16,8,244}},
    {{0,8,242},{0,16,245},{0,24,247},{8,8,249},{8,16,246},{8,24,248},{16,8,250}}
};
bool wyvern_pose_find(const uint8_t oam[256],HeroPoseMatch *out) {
    int best=0;memset(out,0,sizeof(*out));
    for(int flip=0;flip<2;flip++)for(int pose=0;pose<2;pose++)for(int seed=0;seed<64;seed++) {
        const uint8_t *s=oam+seed*4;if(s[0]>=239||s[1]<240||s[1]>250)continue;
        for(int part=0;part<counts[pose];part++) {
            const Part *p=&poses[pose][part];int attr=1+(flip?64:0);
            if(s[1]!=p->tile||(s[2]&0xe3)!=attr)continue;
            HeroPoseMatch candidate={.pose=pose,.flip=flip,.x=s[3]-(flip?16-p->x:p->x),.y=s[0]+1-p->y};
            bool valid=true;int count=0;
            for(int k=0;k<counts[pose];k++) {
                p=&poses[pose][k];int x=candidate.x+(flip?16-p->x:p->x),y=candidate.y+p->y;
                if(x<0||x>=256||y<1||y>=240)continue;
                int found=-1;
                for(int slot=0;slot<64;slot++) {
                    const uint8_t *q=oam+slot*4;
                    if(q[0]<239&&q[0]+1==y&&q[3]==x&&q[1]==p->tile&&(q[2]&0xe3)==attr){found=slot;break;}
                }
                if(found<0){valid=false;break;}candidate.slots[found]=true;count++;
            }
            if(valid&&count>=4&&count>best){*out=candidate;best=count;if(count==counts[pose])return true;}
        }
    }
    return best>=4;
}
