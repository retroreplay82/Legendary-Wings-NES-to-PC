#include "hero-animation.h"
#include <string.h>
typedef struct { int x,y; uint8_t tile,attr; } Part;
static const int parts[8]={12,12,12,10,12,10,10,10};
static const Part poses[8][12]={
    {{0,0,0,2},{0,8,2,2},{8,0,1,2},{8,8,3,2},{8,16,4,0},{8,24,5,0},
     {16,0,1,66},{16,8,3,66},{16,16,4,64},{16,24,5,64},{24,0,0,66},{24,8,2,66}},
    {{0,8,7,2},{0,16,9,2},{8,0,6,2},{8,8,8,2},{8,16,10,0},{8,24,11,0},
     {16,0,6,66},{16,8,8,66},{16,16,10,64},{16,24,11,64},{24,8,7,66},{24,16,9,66}},
    {{0,0,12,2},{0,8,14,2},{8,0,13,2},{8,8,15,2},{8,16,4,0},{8,24,5,0},
     {16,0,13,66},{16,8,15,66},{16,16,4,64},{16,24,5,64},{24,0,12,66},{24,8,14,66}},
    {{0,0,27,2},{0,8,29,2},{8,0,28,2},{8,8,30,2},{8,16,31,2},
     {16,0,28,66},{16,8,30,66},{16,16,31,66},{24,0,27,66},{24,8,29,66}},
    {{0,0,27,2},{0,8,29,2},{8,0,28,2},{8,8,30,2},{8,16,31,2},{8,24,5,0},
     {16,0,28,66},{16,8,30,66},{16,16,31,66},{16,24,5,64},{24,0,27,66},{24,8,29,66}},
    {{0,0,27,2},{0,8,29,2},{8,0,28,2},{8,8,30,2},{8,16,4,0},
     {16,0,28,66},{16,8,30,66},{16,16,4,64},{24,0,27,66},{24,8,29,66}},
    {{0,0,27,2},{0,8,2,2},{8,0,28,2},{8,8,3,2},{8,16,31,2},
     {16,0,28,66},{16,8,3,66},{16,16,31,66},{24,0,27,66},{24,8,2,66}},
    {{0,0,0,2},{0,8,29,2},{8,0,1,2},{8,8,30,2},{8,16,31,2},
     {16,0,1,66},{16,8,30,66},{16,16,31,66},{24,0,0,66},{24,8,29,66}}
};
static bool phoenix_pose_find(const uint8_t oam[256],HeroPoseMatch *out) {
    static const Part base[12]={{0,8,35,0},{0,24,41,0},{8,0,32,0},{8,8,36,0},{8,16,39,0},{8,24,42,0},{16,0,33,0},{16,8,37,0},{16,16,40,0},{16,24,43,0},{24,0,34,0},{24,8,38,0}};
    static const uint8_t right[3][6]={{33,37,40,43,34,38},{44,46,48,49,45,47},{50,52,54,55,51,53}};
    for(int seed=0;seed<64;seed++) {
        const uint8_t *s=oam+seed*4;if(s[0]>=239||s[1]!=32||(s[2]&0xc0))continue;
        for(int pose=0;pose<3;pose++) {
            HeroPoseMatch m={.pose=pose,.x=s[3]-8,.y=s[0]+1,.flash=8,.palette=s[2]&3};
            int count=0;bool valid=true;
            for(int k=0;k<12;k++) {
                int x=m.x+base[k].x,y=m.y+base[k].y,t=k<6?base[k].tile:right[pose][k-6];
                if(x<0||x>=256||y<1||y>=240)continue;
                int found=-1;
                for(int j=0;j<64;j++){const uint8_t *q=oam+j*4;if(q[0]<239&&q[3]==x&&q[0]+1==y&&q[1]==t&&(q[2]&0xc3)==(s[2]&3)){found=j;break;}}
                /* Native OAM rotation occasionally omits the outer tail feather. */
                if(found<0){if(k==1)continue;valid=false;break;}m.slots[found]=true;count++;
            }
            /* At the bottom-right corner only the unique powered torso may remain. */
            if(valid&&count>=1){*out=m;return true;}
        }
    }
    return false;
}
bool hero_pose_find(const uint8_t oam[256],HeroPoseMatch *out) {
    if(phoenix_pose_find(oam,out))return true;
    int best=0;memset(out,0,sizeof(*out));
    for(int palette=0;palette<2;palette++)for(int pose=0;pose<8;pose++)for(int seed=0;seed<64;seed++) {
        const uint8_t *s=oam+seed*4;if(s[0]>=239||s[1]>31)continue;
        for(int part=0;part<parts[pose];part++) {
            const Part *p=&poses[pose][part];
            int attr=p->attr|((p->attr&3)?0:palette);
            if(s[1]!=p->tile||(s[2]&0xc3)!=attr)continue;
            HeroPoseMatch candidate={.pose=pose<3?pose:0,.x=s[3]-p->x,.y=s[0]+1-p->y,
                                     .palette=palette,.flash=pose<3?0:pose-2};
            int count=0;bool valid=true;
            for(int k=0;k<parts[pose];k++) {
                p=&poses[pose][k];int x=candidate.x+p->x,y=candidate.y+p->y;
                if(x<0||x>=256||y<1||y>=240)continue;
                int found=-1;
                for(int slot=0;slot<64;slot++) {
                    const uint8_t *q=oam+slot*4;
                    attr=p->attr|((p->attr&3)?0:palette);
                    if(q[0]<239&&q[0]+1==y&&q[3]==x&&q[1]==p->tile&&(q[2]&0xc3)==attr){found=slot;break;}
                }
                if(found<0){valid=false;break;}
                candidate.slots[found]=true;count++;
            }
            if(valid&&count>=4&&count>best){*out=candidate;best=count;if(count==12)return true;}
        }
    }
    return best>=4;
}
