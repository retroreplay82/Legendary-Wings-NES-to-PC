/* Bonus-room artwork follows native metatile coordinates, including partial
 * panels at the viewport edges. All sampling is nearest neighbour. */
#include "bonus-bg-signature.inc"
static bool bonus_bank(void) {
    if(!ground_scene)return false;
    uint32_t hash=2166136261u;
    for(int i=0;i<2048;i++)hash=(hash^presentation_chr(0x1000+i))*16777619u;
    return hash==bonus_signature;
}
static HeroPoseMatch bonus_matches[32];
static int bonus_count;
static void find_bonus_treasures(void) {
    bonus_count=0;if(!bonus_treasures.pixels||!bonus_bank())return;
    const uint8_t *o=cyc_render_oam();
    static const int tile[7][4]={{138,139,140,141},{150,151,152,153},{158,159,128,129},{146,147,148,149},{142,143,144,145},{154,155,156,157},{130,131,132,133}};
    for(int seed=0;seed<64&&bonus_count<32;seed++) {
        const uint8_t *s=o+seed*4;if(player_slots[seed]||s[0]>=239)continue;
        for(int pose=0;pose<7;pose++)for(int part=0;part<4;part++)if(s[1]==tile[pose][part]) {
            HeroPoseMatch m={.pose=pose>=4?pose-4:pose,.x=s[3]-(part%2)*8,.y=s[0]+1-(part/2)*8};int count=0;bool valid=true;
            for(int k=0;k<4;k++) {
                int x=m.x+(k%2)*8,y=m.y+(k/2)*8,found=-1;
                if(x<0||x>=256||y<1||y>=240)continue;
                for(int j=0;j<64;j++)if(!player_slots[j]&&o[j*4]<239&&o[j*4+3]==x&&o[j*4]+1==y&&(o[j*4+1]==tile[pose][k]||(pose!=3&&o[j*4+1]==tile[pose>=4?pose-4:pose+4][k]))&&(o[j*4+2]&3)==(s[2]&3)){found=j;break;}
                if(found<0){valid=false;break;}m.slots[found]=true;count++;
            }
            if(valid&&count>=2){bonus_matches[bonus_count++]=m;for(int j=0;j<64;j++)if(m.slots[j])player_slots[j]=true;}
        }
    }
}

static void paint_bonus_treasures(void) {
    if(!bonus_treasures.pixels)return;
    int cell=bonus_treasures.width/4;
    static const int bounds[4][4]={{82,157,481,617},{110,161,481,617},{80,162,508,618},{87,243,454,599}};
    for(int i=0;i<bonus_count;i++) {
        HeroPoseMatch *m=&bonus_matches[i];
        for(int y=0;y<32;y++)for(int x=0;x<32;x++) {
            int dx=m->x*2+x,dy=m->y*2+y;if(dx<0||dx>=512||dy<0||dy>=480)continue;
            const int *b=bounds[m->pose];int sx=m->pose*cell+b[0]+x*(b[2]-b[0])/32,sy=b[1]+y*(b[3]-b[1])/32;
            const unsigned char *p=bonus_treasures.pixels+(sy*bonus_treasures.width+sx)*4;
            if(p[3]<192||!artwork_priority_claim(artwork_owner,first_slot(m),dx,dy))continue;
            canvas[dy*512+dx]=0xff000000u|((uint32_t)p[0]<<16)|((uint32_t)p[1]<<8)|p[2];
        }
    }
}
/* Keep native decorative silhouettes and negative space. Only masonry gets
 * the replacement stone material; all masks come from the local ROM. */
static void paint_bonus_background(void) {
    if(!background_art||!bonus_bank())return;
    for(int y=0;y<240;y++) {
        int mask=presentation_line_mask(y);
        if(!(mask&8)||presentation_line_bg_table(y)!=0x1000)continue;
        int sy=presentation_line_scroll_y(y)%480;
        for(int x=0;x<256;x++) {
            if(sprite_guard[y*256+x]||(x<8&&!(mask&2)))continue;
            int sx=(presentation_line_scroll_x(y)+x)%512,cx=sx%256/8,cy=sy%240/8;
            int nt=0x2000+(sx/256+sy/240*2)*0x400;
            int tile=presentation_nametable(nt+cy*32+cx);
            uint32_t source=background_pixel(x,y);
            uint32_t native=horizontal_atmosphere(source,x,y);
            int r=(native>>16)&255,g=(native>>8)&255,b=native&255;
            if(((source>>16)&255)>150&&((source>>8)&255)<100&&(source&255)<100){r=178;g=75;b=41;}
            bool stone=tile>=100&&tile<=103;
            int material=256;
            if(stone&&bonus_bricks.pixels) {
                /* World-registered material cannot move any mortar or edge. */
                int u=sx%32,v=sy%16;
                int tx=u*bonus_bricks.width/32,ty=v*bonus_bricks.height/64;
                const unsigned char *p=bonus_bricks.pixels+(ty*bonus_bricks.width+tx)*4;
                material=256+(((p[0]+p[1]+p[2])/3)-130)/3;
            }
            for(int yy=0;yy<2;yy++)for(int xx=0;xx<2;xx++) {
                int shade=material;
                /* Crisp subpixel carving detail inside the native ink only.
                 * Dark gaps remain exactly the original dark background. */
                if(r+g+b>105)shade+=xx==0&&yy==0?5:(xx==1&&yy==1?-5:0);
                else shade=256;
                int rr=r*shade/256,gg=g*shade/256,bb=b*shade/256;
                if(rr>255)rr=255;if(gg>255)gg=255;if(bb>255)bb=255;
                canvas[(y*2+yy)*512+x*2+xx]=0xff000000u|((uint32_t)rr<<16)|((uint32_t)gg<<8)|bb;
            }
        }
    }
}

static const uint32_t mask_signature[6]={0xfc20ec48u,0x9424194eu,0xcf9c2475u,0xaab1f86du,0x0795a185u,0x6ce52a98u};
static HeroPoseMatch mask_matches[24];
static int mask_count;
static void find_flying_masks(void) {
    mask_count=0;if(!known_flight_art()||!flying_mask.pixels)return;
    for(int k=0;k<6;k++)if(!chr_matches((151+k)*16,mask_signature[k]))return;
    const uint8_t *o=cyc_render_oam();
    for(int seed=0;seed<64&&mask_count<24;seed++) {
        const uint8_t *s=o+seed*4;if(player_slots[seed]||s[0]>=239||(s[2]&0xc0))continue;
        for(int pose=0;pose<2;pose++) {
            int base=151+pose*6,rows=3;
            if(s[1]!=base+1)continue;
            HeroPoseMatch m={.pose=pose,.palette=s[2]&3,.x=s[3]-8,.y=s[0]+1};int count=0;bool valid=true;
            for(int k=0;k<rows*3;k++) {
                int col=k%3,row=k/3,x=m.x+col*8,y=m.y+row*8;
                int t=base+row*2+(col==1),attr=(col==2?64:0)|m.palette;
                if(x<0||x>=256||y<1||y>=240)continue;int found=-1;
                for(int j=0;j<64;j++)if(!player_slots[j]&&o[j*4]<239&&o[j*4+3]==x&&o[j*4]+1==y&&o[j*4+1]==t&&(o[j*4+2]&0xc3)==attr){found=j;break;}
                if(found<0){valid=false;break;}m.slots[found]=true;count++;
            }
            if(valid&&count>=3){mask_matches[mask_count++]=m;for(int j=0;j<64;j++)if(m.slots[j])player_slots[j]=true;}
        }
    }
    /* The released inner head is a distinct six-sprite 16x24 form. */
    for(int seed=0;seed<64&&mask_count<24;seed++) {
        const uint8_t *s=o+seed*4;
        if(player_slots[seed]||s[0]>=239||s[1]!=55||(s[2]&0x83)!=3)continue;
        bool flip=(s[2]&64)!=0;
        HeroPoseMatch m={.pose=2,.palette=3,.flip=flip,.x=s[3]-(flip?8:0),.y=s[0]+1};
        bool valid=true;int count=0;
        for(int k=0;k<6;k++) {
            int x=m.x+(flip?1-k%2:k%2)*8,y=m.y+(k/2)*8,found=-1;
            if(x<0||x>=256||y<1||y>=240)continue;
            for(int j=0;j<64;j++)if(!player_slots[j]&&o[j*4]<239&&o[j*4+3]==x&&o[j*4]+1==y&&o[j*4+1]==55+k&&(o[j*4+2]&0xc3)==(3|(flip?64:0))){found=j;break;}
            if(found<0){valid=false;break;}m.slots[found]=true;count++;
        }
        if(valid&&count>=2){mask_matches[mask_count++]=m;for(int j=0;j<64;j++)if(m.slots[j])player_slots[j]=true;}
    }

}

static void paint_flying_masks(void) {
    for(int i=0;i<mask_count;i++) {
        HeroPoseMatch *m=&mask_matches[i];
        int height=48,width=m->pose==2?32:48;
        for(int y=0;y<height;y++)for(int x=0;x<width;x++) {
            int dx=m->x*2+x,dy=m->y*2+y;if(dx<0||dx>=512||dy<0||dy>=480)continue;
            int px=m->flip?width-1-x:x;
            int sx=m->pose*(flying_mask.width/3)+70+px*564/width,sy=99+y*550/height;const unsigned char *p=flying_mask.pixels+(sy*flying_mask.width+sx)*4;
            if(p[3]<192||!artwork_priority_claim(artwork_owner,first_slot(m),dx,dy))continue;
            canvas[dy*512+dx]=0xff000000u|((uint32_t)p[0]<<16)|((uint32_t)p[1]<<8)|p[2];
        }
    }
}
