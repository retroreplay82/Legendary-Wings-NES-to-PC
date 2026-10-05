#include "swirl-animation.h"
#include <string.h>
/* Shrinking vortices retain complete top rows while removing bottom rows.
 * Prefer complete composites over a shorter matching prefix. */
bool swirl_pose_find(const uint8_t oam[256],HeroPoseMatch *out) {
    int best=0;memset(out,0,sizeof(*out));
    for(int pose=0;pose<4;pose++)for(int flip=0;flip<2;flip++)for(int seed=0;seed<64;seed++) {
        int columns=pose==0?1:pose==1?2:3,base=pose==0?32:pose==1?33:pose==2?37:46;
        const uint8_t *s=oam+seed*4;int attr=flip?195:3;
        if(s[0]>=207||(s[2]&227)!=attr||s[1]<base||s[1]>=base+columns*columns)continue;
        int part=s[1]-base,px=part%columns,py=part/columns;
        HeroPoseMatch m={.pose=pose,.flip=flip,.x=s[3]-(flip?columns-1-px:px)*8,
            .y=s[0]+1-(flip?columns-1-py:py)*8};
        int count=0;bool valid=true,ended=false;
        for(int row=0;row<columns;row++) {
            int row_count=0,expected=0;
            for(int col=0;col<columns;col++) {
                int x=m.x+col*8,y=m.y+row*8;
                if(x<0||x>=256||y<1||y>=208)continue;expected++;
                int tile=base+(flip?columns-1-row:row)*columns+(flip?columns-1-col:col),found=-1;
                for(int slot=0;slot<64;slot++){const uint8_t *q=oam+slot*4;
                    if(q[0]<207&&q[3]==x&&q[0]+1==y&&q[1]==tile&&(q[2]&227)==attr){found=slot;break;}}
                if(found>=0){m.slots[found]=true;row_count++;count++;}
            }
            if(row_count&& (ended||row_count!=expected)){valid=false;break;}
            if(expected&&!row_count)ended=true;
            if(row==0&&expected&&!row_count){valid=false;break;}
        }
        if(valid&&count>=columns&&count>best){*out=m;best=count;}
    }
    return best>0;
}
