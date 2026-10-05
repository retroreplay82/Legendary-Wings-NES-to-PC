#include "green-animation.h"
#include <string.h>
bool green_pose_find(const uint8_t oam[256],HeroPoseMatch *out) {
 static const int tiles[6][16]={{132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147},
 {148,149,134,150,151,152,153,154,155,156,157,158,159,167,168,169},
 {213,214,215,216},{217,218,219,220},{213,215,214,216},{217,219,218,220}};
 memset(out,0,sizeof(*out));
 for(int pose=0;pose<6;pose++)for(int seed=0;seed<64;seed++) {
  const uint8_t *s=oam+seed*4;int columns=pose<2?4:2,parts=columns*columns;
  if(s[0]>=207||(s[2]&3)!=3)continue;
  bool flipx=(s[2]&64)!=0,flipy=(s[2]&128)!=0;
  for(int part=0;part<parts;part++) {
   if(s[1]!=tiles[pose][part])continue;
   HeroPoseMatch match={.pose=pose,.x=s[3]-((part%columns)^(flipx?columns-1:0))*8,.y=s[0]+1-((part/columns)^(flipy?columns-1:0))*8,.flip=flipx,.palette=s[2]};
   bool valid=true;int count=0,visible=0;
   for(int k=0;k<parts;k++) {
    int x=match.x+((k%columns)^(flipx?columns-1:0))*8,y=match.y+((k/columns)^(flipy?columns-1:0))*8;
    if(x<0||x>=256||y<1||y>=208)continue;visible++;
    int found=-1;
    for(int slot=0;slot<64;slot++) {
     const uint8_t *q=oam+slot*4;
     if(q[0]<207&&q[3]==x&&q[0]+1==y&&q[1]==tiles[pose][k]&&q[2]==s[2]){found=slot;break;}
    }
    /* Exact native tile IDs remain sufficient when OAM allocation or
     * screen clipping leaves only part of this known enemy visible. */
    if(found<0)continue;
    if(found<0){valid=false;break;}match.slots[found]=true;count++;
   }
   if(valid&&count>=1){*out=match;return true;}
  }
 }
 return false;
}
