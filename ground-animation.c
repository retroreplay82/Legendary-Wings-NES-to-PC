#include "ground-animation.h"
#include <string.h>
typedef struct { int x,y; uint8_t tile,attr; } Part;
/* All roots use the same body anchor. Extended wing tips are at x=-8,
 * rather than shifting the character when the wing silhouette grows. */
static const int counts[18]={10,11,12,10,4,9,9,9,10,7,9,9,10,6,10,8,10,9};
static const Part poses[18][12]={
    {{0,0,3,66},{0,8,6,64},{0,8,7,66},{0,16,22,64},
     {8,0,2,66},{8,8,5,64},{8,16,21,64},{8,24,24,64},{16,8,4,66},{16,24,23,64}},
    {{-8,5,27,66},{0,0,25,66},{0,8,6,64},{0,8,26,66},{0,16,22,64},
     {8,0,2,66},{8,8,5,64},{8,16,21,64},{8,24,24,64},{16,8,4,66},{16,24,23,64}},
    {{-8,0,30,66},{-8,8,29,2},{0,0,28,66},{0,8,6,64},{0,8,29,66},{0,16,22,64},
     {8,0,2,66},{8,8,5,64},{8,16,21,64},{8,24,24,64},{16,8,4,66},{16,24,23,64}},
    {{0,0,3,66},{0,8,6,64},{0,8,7,66},{0,16,22,64},
     {8,0,31,66},{8,8,5,64},{8,16,21,64},{8,24,24,64},{16,8,4,66},{16,24,23,64}},
    {{0,8,6,64},{0,8,7,64},{8,8,5,64},{16,8,4,66}},
    {{8,8,67,66},{8,16,69,66},{8,0,64,66},{8,24,72,66},
     {0,0,65,66},{16,8,66,66},{0,8,68,66},{0,16,70,66},{16,24,71,66}},
    {{8,8,67,66},{8,16,69,66},{8,0,64,66},{8,24,24,64},
     {0,0,65,66},{16,8,66,66},{0,8,68,66},{0,16,70,66},{16,24,23,64}},
    {{8,8,67,66},{8,16,21,64},{8,0,64,66},{8,24,72,66},
     {0,0,65,66},{16,8,66,66},{0,8,68,66},{0,16,70,64},{16,24,23,66}},
    {{0,0,65,66},{0,8,6,64},{0,8,7,64},{0,16,70,66},
     {8,0,64,66},{8,8,5,64},{8,16,69,66},{8,24,72,66},{16,8,4,66},{16,24,71,66}},
    {{0,0,13,66},{0,7,7,66},{0,8,15,64},{8,0,12,66},{8,8,14,64},{16,7,4,66},{8,24,10,64}},
    {{0,0,3,66},{0,8,6,64},{0,8,7,66},{0,16,9,64},{8,0,2,66},{8,8,5,64},{8,24,10,64},{16,8,4,66},{-8,21,11,64}},
    {{0,0,13,66},{0,7,7,66},{0,8,15,64},{0,16,17,64},{1,24,18,64},{8,0,12,66},{8,8,14,64},{8,16,16,64},{16,7,4,66}},
    {{0,0,3,66},{0,8,6,64},{0,8,7,66},{0,16,20,64},{8,0,2,66},{8,8,5,64},{8,16,19,64},{10,24,10,64},{16,8,4,66},{-8,21,11,64}},
    {{0,0,3,66},{0,8,6,64},{0,8,7,66},{8,0,2,66},{8,8,5,64},{16,8,4,66}},
    {{0,0,13,66},{0,7,7,66},{0,8,15,64},{8,0,12,66},{8,8,14,64},{16,7,4,66},
     {0,16,75,64},{8,16,74,64},{0,24,76,64},{8,24,10,64}},
    {{0,0,3,66},{0,8,6,64},{0,8,7,66},{8,0,2,66},{8,8,5,64},{16,8,4,66},{0,16,78,64},{8,16,77,64}},
    /* The extra front thigh appears for five frames after tucked legs. */
    {{0,0,3,66},{0,8,6,64},{0,8,7,66},{0,16,9,64},{8,0,2,66},{8,8,5,64},{8,16,79,64},{8,24,10,64},{16,8,4,66},{-8,21,11,64}},
    /* Final flash restores the head while the body remains white. */
    {{8,8,67,66},{8,16,69,66},{8,0,31,66},{8,24,72,66},
     {0,0,3,66},{16,8,66,66},{0,8,68,66},{0,16,70,66},{16,24,71,66}}
};
static const Part powered[3][12]={{{0,0,34,64},{0,8,38,64},{8,0,33,64},{8,8,37,64},{8,16,40,64},{8,16,43,64},{16,0,32,64},{16,8,36,64},{16,16,39,64},{16,24,42,64},{24,8,35,64},{24,24,41,64}},{{0,0,45,64},{0,8,47,64},{8,0,44,64},{8,8,46,64},{8,16,48,64},{8,16,49,64},{16,0,32,64},{16,8,36,64},{16,16,39,64},{16,24,42,64},{24,8,35,64},{24,24,41,64}},{{0,0,51,64},{0,8,53,64},{8,0,50,64},{8,8,52,64},{8,16,54,64},{8,16,55,64},{16,0,32,64},{16,8,36,64},{16,16,39,64},{16,24,42,64},{24,8,35,64},{24,24,41,64}}};
static bool powered_ground_find(const uint8_t oam[256],HeroPoseMatch *out) {
    for(int pose=0;pose<3;pose++)for(int flip=0;flip<2;flip++)for(int seed=0;seed<64;seed++) {
        const uint8_t *s=oam+seed*4;if(s[0]>=239||s[1]!=32)continue;
        int x=s[3]-(flip?8:16),y=s[0]+1,count=0;bool valid=true;
        HeroPoseMatch m={.pose=pose,.flash=8,.palette=s[2]&3,.flip=flip,.x=x+8,.y=y};
        for(int k=0;k<12;k++) {
            const Part *p=&powered[pose][k];int px=x+(flip?24-p->x:p->x),py=y+p->y,found=-1;
            if(px<0||px>=256||py<1||py>=240)continue;
            for(int j=0;j<64;j++){const uint8_t *q=oam+j*4;if(q[0]<239&&q[3]==px&&q[0]+1==py&&q[1]==p->tile&&(q[2]&0xc3)==((p->attr^(flip?64:0))|(s[2]&3))){found=j;break;}}
            if(found<0){valid=false;break;}m.slots[found]=true;count++;
        }
        if(valid&&count>=4){*out=m;return true;}
    }
    return false;
}
bool ground_pose_find(const uint8_t oam[256],HeroPoseMatch *out) {
    if(powered_ground_find(oam,out))return true;
    int best=0;memset(out,0,sizeof(*out));
    for(int palette=0;palette<2;palette++)for(int flip=0;flip<2;flip++)
    for(int pose=0;pose<18;pose++)for(int seed=0;seed<64;seed++) {
        const uint8_t *s=oam+seed*4;if(s[0]>=239||s[1]>79)continue;
        for(int part=0;part<counts[pose];part++) {
            const Part *p=&poses[pose][part];
            int attr=(p->attr|((p->attr&3)?0:palette))^(flip?64:0);
            if(s[1]!=p->tile||(s[2]&0xe3)!=attr)continue;
            HeroPoseMatch candidate={.pose=pose==17?0:pose==16?5:pose>=14?pose-6:pose>=9?(pose==13?4:pose==9?3:pose-5):pose<3?pose:0,.flash=pose==17?7:pose>=9||pose<3?0:pose-2,.palette=palette,.flip=flip,
                .x=s[3]-(flip?16-p->x:p->x),.y=s[0]+1-p->y};
            int count=0;bool valid=true;
            for(int k=0;k<counts[pose];k++) {
                p=&poses[pose][k];int x=candidate.x+(flip?16-p->x:p->x),y=candidate.y+p->y;
                if(x<0||x>=256||y<1||y>=240)continue;
                attr=(p->attr|((p->attr&3)?0:palette))^(flip?64:0);
                int found=-1;
                for(int slot=0;slot<64;slot++) {
                    const uint8_t *q=oam+slot*4;
                    bool attributes=(q[2]&0xe3)==attr;
                    /* The white pose alternates its left leg between the
                     * shared white palette and the normal body palette. */
                    if(pose>=5&&p->tile==70)attributes=(q[2]&0xe1)==(attr&0xe1);
                    if(q[0]<239&&q[0]+1==y&&q[3]==x&&q[1]==p->tile&&attributes){found=slot;break;}
                }
                if(found<0){valid=false;break;}candidate.slots[found]=true;count++;
            }
            if(valid&&count>=4&&count>best) {
                /* A short arm fragment can also be contained in a larger
                 * white flash pose. Prefer the most complete composite. */
                *out=candidate;best=count;
            }
        }
    }
    return best>=4;
}
