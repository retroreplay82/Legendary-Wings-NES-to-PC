#include "cyc_host_extras.h"
#include "cyc_core.h"
#include "cyc_render.h"
#include "cyc_png.h"
#include "cyc_mod.h"
#include "hero-animation.h"
#include "ground-animation.h"
#include "wyvern-animation.h"
#include "pod-animation.h"
#include "artwork-priority.h"
#include "guardian-animation.h"
#include "green-animation.h"
#include "cycling-animation.h"
#include "flash-animation.h"
#include "swirl-animation.h"
#include "halo-animation.h"
#include "chamber-animation.h"
#include "turret-animation.h"
#include "missile-animation.h"
#include "serpent-animation.h"
#include "burst-animation.h"
#include <SDL.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

/* Artwork changes stay in presentation. Respawn terrain correction is isolated
 * in respawn-collision.c; debug No Damage and capture aids are explicit options. */
static bool enhanced=true, tried_asset, player_slots[64],native_power_slots[64],native_power_active;
static int presentation_benchmark;
static bool water_motion=false,water_held;
static HeroPoseMatch late_guardians[12];
static int late_guardian_count;

static unsigned char dragon_filtered[3][160*144*4];
static HeroPoseMatch dragon_matches[2];
static int dragon_count;
static uint32_t canvas[512*480],clean[256*240],sprites[256*240];
static uint8_t artwork_owner[512*480];

static uint8_t presentation_chr_data[8192],presentation_nt_data[4096],presentation_masks[240];
static uint32_t presentation_colors[32];
static uint16_t presentation_tables[240];
static int presentation_scroll_x[240],presentation_scroll_y[240];
static uint8_t sprite_guard[256*240];
static uint32_t chr_fingerprints[512];
static bool chr_matches(unsigned address,uint32_t fingerprint) {
    return chr_fingerprints[(address&8191)/16]==fingerprint;
}
static bool live_chr_matches(unsigned address,uint32_t fingerprint) {
    uint32_t h=2166136261u;
    for(int i=0;i<16;i++)h=(h^cyc_render_chr(address+i))*16777619u;
    return h==fingerprint;
}
static inline uint8_t presentation_chr(uint16_t a){return presentation_chr_data[a&8191];}
static inline uint8_t presentation_nametable(uint16_t a){return presentation_nt_data[a&4095];}
static inline uint32_t presentation_color(int entry){return presentation_colors[entry&31];}
static inline uint8_t presentation_line_mask(int y){return presentation_masks[y];}
static inline uint16_t presentation_line_bg_table(int y){return presentation_tables[y];}
static inline int presentation_line_scroll_x(int y){return presentation_scroll_x[y];}
static inline int presentation_line_scroll_y(int y){return presentation_scroll_y[y];}
static void snapshot_presentation(void) {
    for(int a=0;a<8192;a++)presentation_chr_data[a]=cyc_render_chr(a);
    for(int a=0;a<4096;a++)presentation_nt_data[a]=cyc_render_nametable(0x2000+a);
    for(int t=0;t<512;t++) {
        uint32_t h=2166136261u;
        for(int i=0;i<16;i++)h=(h^presentation_chr_data[t*16+i])*16777619u;
        chr_fingerprints[t]=h;
    }
    for(int e=0;e<32;e++)presentation_colors[e]=cyc_render_color(e);
    for(int y=0;y<240;y++) {
        presentation_masks[y]=cyc_render_line_mask(y);
        presentation_tables[y]=cyc_render_line_bg_table(y);
        presentation_scroll_x[y]=cyc_render_line_scroll_x(y);
        presentation_scroll_y[y]=cyc_render_line_scroll_y(y);
    }
}
static void prepare_sprite_guard(void) {
    memset(sprite_guard,0,sizeof(sprite_guard));
    for(int y=0;y<240;y++)for(int x=0;x<256;x++)if(sprites[y*256+x])
        for(int yy=y>0?y-1:y;yy<=y+1&&yy<240;yy++)
            for(int xx=x>0?x-1:x;xx<=x+1&&xx<256;xx++)sprite_guard[yy*256+xx]=1;
}

typedef struct { unsigned char *pixels; int width,height; } HeroArt;
static HeroArt hero[3];
/* Filter once at load time, rather than discarding almost every source pixel
 * in each frame. Premultiplied coverage avoids dark transparent fringes. */
static unsigned char flight_filtered[3][80*72*4];
static void filter_flight_art(int left,int top,int width,int height) {
    for(int frame=0;frame<3;frame++) {
        const HeroArt *a=&hero[frame];if(!a->pixels)continue;
        for(int y=0;y<72;y++)for(int x=0;x<80;x++) {
            int x0=left+x*width/80,x1=left+(x+1)*width/80;
            int y0=top+y*height/72,y1=top+(y+1)*height/72;
            if(x1<=x0)x1=x0+1;if(y1<=y0)y1=y0+1;
            unsigned long long alpha=0,r=0,g=0,b=0;int count=0;
            for(int sy=y0;sy<y1&&sy<a->height;sy++)for(int sx=x0;sx<x1&&sx<a->width;sx++) {
                const unsigned char *p=a->pixels+(sy*a->width+sx)*4;
                alpha+=p[3];r+=p[0]*p[3];g+=p[1]*p[3];b+=p[2]*p[3];count++;
            }
            unsigned char *p=flight_filtered[frame]+(y*80+x)*4;
            if(alpha&&count){p[0]=r/alpha;p[1]=g/alpha;p[2]=b/alpha;p[3]=alpha/count;}
        }
    }
}
static HeroArt ground;
static HeroArt bonus_bricks,bonus_pharaoh,bonus_frieze,bonus_treasures,phoenix,flying_mask;
static HeroArt late_guardian_art,arena_bricks,arena_pilaster,dragon_art,boss_cannon_art;
static unsigned char boss_cannon_pixels[2][64*64*4],boss_cannon_small[2][32*32*4];
static HeroPoseMatch lion_bosses[8];
static int lion_boss_count;
static HeroPoseMatch lion_bolts[64];
static int lion_bolt_count;
static unsigned char late_guardian_filtered[2][96*96*4];
static HeroArt flight_sandstone, flight_heads, flight_water, title_art, ui_font;
static unsigned char heads_filtered[192*64*4];
static HeroArt flight_turret, flight_islet, guardian_dart, flight_gateway;
static unsigned char gateway_filtered[2][448*320*4];
static unsigned char dart_filtered[16*32*4];
static HeroPoseMatch guardian_darts[32];
static int guardian_dart_count;
static int flight_islet_bounds[4];
static unsigned char islet_filtered[32*32*4];
static int flight_turret_bounds[2][4];
static unsigned char flight_turret_filtered[2][32*32*4];
static HeroArt cavern_rock, cavern_sandstone, cavern_carvings, guardian_reliefs, cavern_details, cavern_brain;
static unsigned char modern_bone[3][128*128*4];
static unsigned char rib_bone_mask[128*128];
static HeroArt temple_jade, temple_panel, temple_statue;
static bool background_art=true;
static bool warning_screen=false;
static char background_lines_path[1024];
static HeroArt grounded;
static HeroArt walking;
static HeroArt chamber_art, spring_jumper, mechanical_boss, mechanical_body, mechanical_closed;
static int spring_bounds[3][4], spring_count;
static HeroPoseMatch spring_matches[24];
static int chamber_bounds[5][4],chamber_native[5][4];
static HeroPoseMatch chambers[32],missiles[32];
static int chamber_count,missile_count;
static HeroArt turret_art;
static int turret_bounds[2][4];
static HeroPoseMatch turrets[32];
static int turret_count;
static HeroArt power_art;
static int power_bounds[4];
static HeroArt jumping;
static int jump_bounds[2][4];
static HeroArt halo_art;
static int halo_bounds[4];
static HeroPoseMatch halos[32];
static int halo_count;
static HeroArt swirl_art;
static int swirl_bounds[3][4];
static HeroPoseMatch swirls[64];
static int swirl_count;
static HeroArt flash_art;
static int flash_bounds[4];
static HeroPoseMatch flashes[32];
static int flash_count;
static const int walk_left[3]={121,828,1528};
static const int grounded_bounds[2][4]={{87,57,769,927},{818,351,1490,927}};
static HeroArt wyvern;
static HeroPoseMatch wyverns[8];
static int wyvern_count;
static HeroArt guardian;
static HeroArt blue_orb;
static HeroArt green_orb;
static int green_crop_x,green_crop_y,green_crop_w,green_crop_h;
static HeroPoseMatch greens[32];
static int green_count;
static HeroArt serpent;
static HeroPoseMatch serpents[24];
static int serpent_count;
static HeroArt burst;
static HeroPoseMatch bursts[32];
static int burst_count;
static const int burst_bounds[7][4]={{21,282,189,448},{224,192,560,540},
    {593,180,945,548},{966,186,1278,540},{1296,185,1681,546},
    {1739,277,1936,463},{2000,304,2151,441}};
/* Measured component bounds; generated art is not registered to equal cells. */
static const int serpent_bounds[3][4]={{42,65,948,569},{1039,113,1552,611},{1644,121,2123,610}};
static int blue_crop_x,blue_crop_y,blue_crop_w,blue_crop_h;
static HeroPoseMatch guardians[24];
static int guardian_count;
static const int guardian_bounds[3][4]={{127,77,708,660},{71,25,653,660},{35,111,619,658}};
static HeroArt pod;
static HeroArt spindle;
static int spindle_crop_x,spindle_crop_y,spindle_crop_w,spindle_crop_h;
static HeroPoseMatch pods[24];
static int pod_count,pod_crop_x,pod_crop_y,pod_crop_w,pod_crop_h;
static const int wyvern_eye_x[2]={449,1352},wyvern_eye_y[2]={299,298};
static const int wyvern_bounds[2][4]={{377,49,845,681},{1281,187,1884,681}};
static HeroPoseMatch cycling[32];
static int cycling_count;
static bool ground_scene;
static const int ground_head_x[3]={387,1080,1878},ground_foot_y[3]={670,672,677};
static const int ground_bounds[3][4]={{161,76,575,670},{781,117,1265,672},{1437,66,2064,677}};
static HeroArt orb;
static HeroArt bolt;
static HeroArt pellet;
static HeroArt shot;
static HeroArt impact;
static int impact_crop_x,impact_crop_y,impact_crop_w,impact_crop_h;
static int shot_crop_x,shot_crop_y,shot_crop_w,shot_crop_h;
static int pellet_crop_x,pellet_crop_y,pellet_crop_w,pellet_crop_h;
static int bolt_crop_x,bolt_crop_y,bolt_crop_w,bolt_crop_h;
static int orb_crop_x,orb_crop_y,orb_crop_w,orb_crop_h;
typedef struct { int x,y,pose,slot; } OrbMatch;
static OrbMatch orbs[16];
static OrbMatch bolts[32];
static int bolt_count;
static int orb_count;
static OrbMatch pellets[64];
static int pellet_count;
static int crop_x,crop_y,crop_w,crop_h;
static HeroPoseMatch players[2];
static int player_count;
static long frames,shot_frame=301;
static bool capture_infinite_lives;
static bool debug_no_damage;
static char shot_path[1024];
static void load_asset(void) {
    if(tried_asset)return;tried_asset=true;
    char path[1024];char *base=SDL_GetBasePath();if(!base)return;
    const char *names[3]={"hero-v1.png","hero-down-v1.png","hero-level-v1.png"};
    const char *extra_names[]={"bonus-bricks-v2.png","bonus-pharaoh-v2.png","bonus-frieze-v3.png","bonus-treasures-v1.png","phoenix-v1.png","flying-mask-v2.png"};
    HeroArt *extra_art[]={&bonus_bricks,&bonus_pharaoh,&bonus_frieze,&bonus_treasures,&phoenix,&flying_mask};
    for(int i=0;i<6;i++){snprintf(path,sizeof(path),"%sassets/%s",base,extra_names[i]);extra_art[i]->pixels=stbi_load(path,&extra_art[i]->width,&extra_art[i]->height,NULL,4);}
    int minx=0x7fffffff,miny=0x7fffffff,maxx=-1,maxy=-1;
    for(int frame=0;frame<3;frame++) {
        snprintf(path,sizeof(path),"%sassets/%s",base,names[frame]);
        HeroArt *art=&hero[frame];art->pixels=stbi_load(path,&art->width,&art->height,NULL,4);
        if(!art->pixels){fprintf(stderr,"Enhanced animation asset unavailable: %s\n",path);continue;}
        if(frame&&(art->width!=hero[0].width||art->height!=hero[0].height)) {
            fprintf(stderr,"Animation canvas dimensions differ: %s\n",path);
            stbi_image_free(art->pixels);art->pixels=NULL;continue;
        }
        for(int y=0;y<art->height;y++)for(int x=0;x<art->width;x++) {
            if(art->pixels[(y*art->width+x)*4+3]<128)continue;
            if(x<minx)minx=x;if(x>maxx)maxx=x;
            if(y<miny)miny=y;if(y>maxy)maxy=y;
        }
    }
    snprintf(path,sizeof(path),"%sassets/orb-v1.png",base);
    orb.pixels=stbi_load(path,&orb.width,&orb.height,NULL,4);
    if(orb.pixels) {
        int x0=orb.width,y0=orb.height,x1=-1,y1=-1,cell=orb.width/2;
        for(int pose=0;pose<2;pose++)for(int y=0;y<orb.height;y++)for(int x=0;x<cell;x++) {
            if(orb.pixels[(y*orb.width+pose*cell+x)*4+3]<128)continue;
            if(x<x0)x0=x;if(x>x1)x1=x;if(y<y0)y0=y;if(y>y1)y1=y;
        }
        orb_crop_x=x0;orb_crop_y=y0;orb_crop_w=x1-x0+1;orb_crop_h=y1-y0+1;
    }
    snprintf(path,sizeof(path),"%sassets/bolt-v1.png",base);
    bolt.pixels=stbi_load(path,&bolt.width,&bolt.height,NULL,4);
    if(bolt.pixels) {
        int x0=bolt.width,y0=bolt.height,x1=-1,y1=-1;
        for(int y=0;y<bolt.height;y++)for(int x=0;x<bolt.width;x++) {
            if(bolt.pixels[(y*bolt.width+x)*4+3]<128)continue;
            if(x<x0)x0=x;if(x>x1)x1=x;if(y<y0)y0=y;if(y>y1)y1=y;
        }
        bolt_crop_x=x0;bolt_crop_y=y0;bolt_crop_w=x1-x0+1;bolt_crop_h=y1-y0+1;
    }
    snprintf(path,sizeof(path),"%sassets/ground-v1.png",base);
    ground.pixels=stbi_load(path,&ground.width,&ground.height,NULL,4);
    if(ground.pixels&&(ground.width!=2172||ground.height!=724)) {
        fprintf(stderr,"Ground artwork registration requires its original 2172x724 canvas\n");
        stbi_image_free(ground.pixels);ground.pixels=NULL;
    }
    snprintf(path,sizeof(path),"%sassets/wyvern-v1.png",base);
    wyvern.pixels=stbi_load(path,&wyvern.width,&wyvern.height,NULL,4);
    if(wyvern.pixels&&(wyvern.width!=2172||wyvern.height!=724)) {
        fprintf(stderr,"Wyvern registration requires its original 2172x724 canvas\n");
        stbi_image_free(wyvern.pixels);wyvern.pixels=NULL;
    }
    snprintf(path,sizeof(path),"%sassets/pod-v1.png",base);
    pod.pixels=stbi_load(path,&pod.width,&pod.height,NULL,4);
    if(pod.pixels) {
        int x0=pod.width,y0=pod.height,x1=-1,y1=-1,cell=pod.width/2;
        for(int pose=0;pose<2;pose++)for(int y=0;y<pod.height;y++)for(int x=0;x<cell;x++) {
            if(pod.pixels[(y*pod.width+pose*cell+x)*4+3]<128)continue;
            if(x<x0)x0=x;if(x>x1)x1=x;if(y<y0)y0=y;if(y>y1)y1=y;
        }
        pod_crop_x=x0;pod_crop_y=y0;pod_crop_w=x1-x0+1;pod_crop_h=y1-y0+1;
        if(x1<x0){stbi_image_free(pod.pixels);pod.pixels=NULL;}
    }
    snprintf(path,sizeof(path),"%sassets/spindle-v1.png",base);
    spindle.pixels=stbi_load(path,&spindle.width,&spindle.height,NULL,4);
    if(spindle.pixels) {
        int x0=spindle.width,y0=spindle.height,x1=-1,y1=-1;
        for(int y=0;y<spindle.height;y++)for(int x=0;x<spindle.width;x++) {
            if(spindle.pixels[(y*spindle.width+x)*4+3]<128)continue;
            if(x<x0)x0=x;if(x>x1)x1=x;if(y<y0)y0=y;if(y>y1)y1=y;
        }
        spindle_crop_x=x0;spindle_crop_y=y0;spindle_crop_w=x1-x0+1;spindle_crop_h=y1-y0+1;
        if(x1<x0){stbi_image_free(spindle.pixels);spindle.pixels=NULL;}
    }
    snprintf(path,sizeof(path),"%sassets/pellet-v1.png",base);
    pellet.pixels=stbi_load(path,&pellet.width,&pellet.height,NULL,4);
    if(pellet.pixels) {
        int x0=pellet.width,y0=pellet.height,x1=-1,y1=-1;
        for(int y=0;y<pellet.height;y++)for(int x=0;x<pellet.width;x++) {
            if(pellet.pixels[(y*pellet.width+x)*4+3]<128)continue;
            if(x<x0)x0=x;if(x>x1)x1=x;if(y<y0)y0=y;if(y>y1)y1=y;
        }
        pellet_crop_x=x0;pellet_crop_y=y0;pellet_crop_w=x1-x0+1;pellet_crop_h=y1-y0+1;
        if(x1<x0){stbi_image_free(pellet.pixels);pellet.pixels=NULL;}
    }
    snprintf(path,sizeof(path),"%sassets/shot-v1.png",base);
    shot.pixels=stbi_load(path,&shot.width,&shot.height,NULL,4);
    if(shot.pixels) {
        int x0=shot.width,y0=shot.height,x1=-1,y1=-1;
        for(int y=0;y<shot.height;y++)for(int x=0;x<shot.width;x++) {
            if(shot.pixels[(y*shot.width+x)*4+3]<128)continue;
            if(x<x0)x0=x;if(x>x1)x1=x;if(y<y0)y0=y;if(y>y1)y1=y;
        }
        shot_crop_x=x0;shot_crop_y=y0;shot_crop_w=x1-x0+1;shot_crop_h=y1-y0+1;
        if(x1<x0){stbi_image_free(shot.pixels);shot.pixels=NULL;}
    }
    snprintf(path,sizeof(path),"%sassets/impact-v1.png",base);
    impact.pixels=stbi_load(path,&impact.width,&impact.height,NULL,4);
    if(impact.pixels) {
        int x0=impact.width,y0=impact.height,x1=-1,y1=-1;
        for(int y=0;y<impact.height;y++)for(int x=0;x<impact.width;x++) {
            if(impact.pixels[(y*impact.width+x)*4+3]<128)continue;
            if(x<x0)x0=x;if(x>x1)x1=x;if(y<y0)y0=y;if(y>y1)y1=y;
        }
        impact_crop_x=x0;impact_crop_y=y0;impact_crop_w=x1-x0+1;impact_crop_h=y1-y0+1;
        if(x1<x0){stbi_image_free(impact.pixels);impact.pixels=NULL;}
    }
    snprintf(path,sizeof(path),"%sassets/guardian-v1.png",base);
    guardian.pixels=stbi_load(path,&guardian.width,&guardian.height,NULL,4);
    if(guardian.pixels&&(guardian.width!=2172||guardian.height!=724)) {
        fprintf(stderr,"Guardian registration requires its original 2172x724 canvas\n");
        stbi_image_free(guardian.pixels);guardian.pixels=NULL;
    }
    snprintf(path,sizeof(path),"%sassets/blue-orb-v1.png",base);
    blue_orb.pixels=stbi_load(path,&blue_orb.width,&blue_orb.height,NULL,4);
    if(blue_orb.pixels) {
        int x0=blue_orb.width,y0=blue_orb.height,x1=-1,y1=-1;
        for(int y=0;y<blue_orb.height;y++)for(int x=0;x<blue_orb.width;x++) {
            if(blue_orb.pixels[(y*blue_orb.width+x)*4+3]<128)continue;
            if(x<x0)x0=x;if(x>x1)x1=x;if(y<y0)y0=y;if(y>y1)y1=y;
        }
        blue_crop_x=x0;blue_crop_y=y0;blue_crop_w=x1-x0+1;blue_crop_h=y1-y0+1;
        if(x1<x0){stbi_image_free(blue_orb.pixels);blue_orb.pixels=NULL;}
    }
    snprintf(path,sizeof(path),"%sassets/green-orb-v1.png",base);
    green_orb.pixels=stbi_load(path,&green_orb.width,&green_orb.height,NULL,4);
    if(green_orb.pixels) {
        int x0=green_orb.width,y0=green_orb.height,x1=-1,y1=-1;
        for(int y=0;y<green_orb.height;y++)for(int x=0;x<green_orb.width;x++) {
            if(green_orb.pixels[(y*green_orb.width+x)*4+3]<128)continue;
            if(x<x0)x0=x;if(x>x1)x1=x;if(y<y0)y0=y;if(y>y1)y1=y;
        }
        green_crop_x=x0;green_crop_y=y0;green_crop_w=x1-x0+1;green_crop_h=y1-y0+1;
        if(x1<x0){stbi_image_free(green_orb.pixels);green_orb.pixels=NULL;}
    }
    snprintf(path,sizeof(path),"%sassets/serpent-v1.png",base);
    serpent.pixels=stbi_load(path,&serpent.width,&serpent.height,NULL,4);
    if(serpent.pixels&&(serpent.width!=2172||serpent.height!=724)) {
        fprintf(stderr,"Serpent registration requires its original 2172x724 canvas\n");
        stbi_image_free(serpent.pixels);serpent.pixels=NULL;
    }
    snprintf(path,sizeof(path),"%sassets/burst-v1.png",base);
    burst.pixels=stbi_load(path,&burst.width,&burst.height,NULL,4);
    if(burst.pixels&&(burst.width!=2172||burst.height!=724)) {
        fprintf(stderr,"Burst registration requires its original 2172x724 canvas\n");
        stbi_image_free(burst.pixels);burst.pixels=NULL;
    }
    snprintf(path,sizeof(path),"%sassets/grounded-cutout-v1.png",base);
    grounded.pixels=stbi_load(path,&grounded.width,&grounded.height,NULL,4);
    if(grounded.pixels&&(grounded.width!=1536||grounded.height!=1024)) {
        stbi_image_free(grounded.pixels);grounded.pixels=NULL;
    }
    snprintf(path,sizeof(path),"%sassets/turret-v1.png",base);
    turret_art.pixels=stbi_load(path,&turret_art.width,&turret_art.height,NULL,4);
    if(turret_art.pixels)for(int cell=0;cell<2;cell++) {
        int *b=turret_bounds[cell],left=cell*turret_art.width/2,right=(cell+1)*turret_art.width/2;
        b[0]=right;b[1]=turret_art.height;
        for(int y=0;y<turret_art.height;y++)for(int x=left;x<right;x++)
            if(turret_art.pixels[(y*turret_art.width+x)*4+3]>=128) {
                if(x<b[0])b[0]=x;if(y<b[1])b[1]=y;if(x>b[2])b[2]=x;if(y>b[3])b[3]=y;
            }
        if(b[0]>b[2]){stbi_image_free(turret_art.pixels);turret_art.pixels=NULL;break;}
    }
    snprintf(path,sizeof(path),"%sassets/mechanical-boss-body-v2.png",base);
    mechanical_body.pixels=stbi_load(path,&mechanical_body.width,&mechanical_body.height,NULL,4);
    snprintf(path,sizeof(path),"%sassets/mechanical-boss-closed-v2.png",base);
    mechanical_closed.pixels=stbi_load(path,&mechanical_closed.width,&mechanical_closed.height,NULL,4);
    snprintf(path,sizeof(path),"%sassets/mechanical-boss-v2.png",base);
    mechanical_boss.pixels=stbi_load(path,&mechanical_boss.width,&mechanical_boss.height,NULL,4);
    snprintf(path,sizeof(path),"%sassets/spring-jumper-v1.png",base);
    spring_jumper.pixels=stbi_load(path,&spring_jumper.width,&spring_jumper.height,NULL,4);
    if(spring_jumper.pixels)for(int cell=0;cell<3;cell++) {
        int *b=spring_bounds[cell];b[0]=(cell+1)*spring_jumper.width/3;b[1]=spring_jumper.height;b[2]=b[3]=0;
        for(int y=0;y<spring_jumper.height;y++)for(int x=cell*spring_jumper.width/3;x<(cell+1)*spring_jumper.width/3;x++)
            if(spring_jumper.pixels[(y*spring_jumper.width+x)*4+3]>=192) {
                if(x<b[0])b[0]=x;if(y<b[1])b[1]=y;if(x>b[2])b[2]=x;if(y>b[3])b[3]=y;
            }
    }
    snprintf(path,sizeof(path),"%sassets/chamber-eyes-v2.png",base);
    chamber_art.pixels=stbi_load(path,&chamber_art.width,&chamber_art.height,NULL,4);
    if(chamber_art.pixels)for(int cell=0;cell<5;cell++) {
        int *b=chamber_bounds[cell],left=cell*chamber_art.width/5,right=(cell+1)*chamber_art.width/5;
        b[0]=right;b[1]=chamber_art.height;
        for(int y=0;y<chamber_art.height;y++)for(int x=left;x<right;x++)
            if(chamber_art.pixels[(y*chamber_art.width+x)*4+3]>=128) {
                if(x<b[0])b[0]=x;if(y<b[1])b[1]=y;if(x>b[2])b[2]=x;if(y>b[3])b[3]=y;
            }
        if(b[0]>b[2]){stbi_image_free(chamber_art.pixels);chamber_art.pixels=NULL;break;}
    }
    snprintf(path,sizeof(path),"%sassets/powerup-v1.png",base);
    power_art.pixels=stbi_load(path,&power_art.width,&power_art.height,NULL,4);
    if(power_art.pixels) {
        power_bounds[0]=power_art.width;power_bounds[1]=power_art.height;
        for(int y=0;y<power_art.height;y++)for(int x=0;x<power_art.width;x++)
            if(power_art.pixels[(y*power_art.width+x)*4+3]>=128) {
                if(x<power_bounds[0])power_bounds[0]=x;if(y<power_bounds[1])power_bounds[1]=y;
                if(x>power_bounds[2])power_bounds[2]=x;if(y>power_bounds[3])power_bounds[3]=y;
            }
        if(power_bounds[0]>power_bounds[2]){stbi_image_free(power_art.pixels);power_art.pixels=NULL;}
    }
    snprintf(path,sizeof(path),"%sassets/jump-v1.png",base);
    jumping.pixels=stbi_load(path,&jumping.width,&jumping.height,NULL,4);
    if(jumping.pixels) {
        if(jumping.width%2){stbi_image_free(jumping.pixels);jumping.pixels=NULL;}
        else for(int cell=0;cell<2;cell++) {
            int *b=jump_bounds[cell],w=jumping.width/2;b[0]=w;b[1]=jumping.height;
            for(int y=0;y<jumping.height;y++)for(int x=0;x<w;x++)
                if(jumping.pixels[(y*jumping.width+cell*w+x)*4+3]>=128) {
                    if(x<b[0])b[0]=x;if(y<b[1])b[1]=y;if(x>b[2])b[2]=x;if(y>b[3])b[3]=y;
                }
            if(b[0]>b[2]){stbi_image_free(jumping.pixels);jumping.pixels=NULL;break;}
            b[0]+=cell*w;b[2]+=cell*w;
        }
    }
    snprintf(path,sizeof(path),"%sassets/death-halo-v1.png",base);
    halo_art.pixels=stbi_load(path,&halo_art.width,&halo_art.height,NULL,4);
    if(halo_art.pixels) {
        halo_bounds[0]=halo_art.width;halo_bounds[1]=halo_art.height;
        for(int y=0;y<halo_art.height;y++)for(int x=0;x<halo_art.width;x++)
            if(halo_art.pixels[(y*halo_art.width+x)*4+3]>=128) {
                if(x<halo_bounds[0])halo_bounds[0]=x;if(y<halo_bounds[1])halo_bounds[1]=y;
                if(x>halo_bounds[2])halo_bounds[2]=x;if(y>halo_bounds[3])halo_bounds[3]=y;
            }
        if(halo_bounds[0]>halo_bounds[2]){stbi_image_free(halo_art.pixels);halo_art.pixels=NULL;}
    }
    snprintf(path,sizeof(path),"%sassets/swirl-v1.png",base);
    swirl_art.pixels=stbi_load(path,&swirl_art.width,&swirl_art.height,NULL,4);
    if(swirl_art.pixels) {
        if(swirl_art.width%3){stbi_image_free(swirl_art.pixels);swirl_art.pixels=NULL;}
        else for(int cell=0;cell<3;cell++) {
            int *b=swirl_bounds[cell],w=swirl_art.width/3;b[0]=w;b[1]=swirl_art.height;
            for(int y=0;y<swirl_art.height;y++)for(int x=0;x<w;x++)
                if(swirl_art.pixels[(y*swirl_art.width+cell*w+x)*4+3]>=128) {
                    if(x<b[0])b[0]=x;if(y<b[1])b[1]=y;if(x>b[2])b[2]=x;if(y>b[3])b[3]=y;
                }
            if(b[0]>b[2]){stbi_image_free(swirl_art.pixels);swirl_art.pixels=NULL;break;}
            b[0]+=cell*w;b[2]+=cell*w;
        }
    }
    snprintf(path,sizeof(path),"%sassets/flash-v1.png",base);
    flash_art.pixels=stbi_load(path,&flash_art.width,&flash_art.height,NULL,4);
    if(flash_art.pixels) {
        flash_bounds[0]=flash_art.width;flash_bounds[1]=flash_art.height;
        for(int y=0;y<flash_art.height;y++)for(int x=0;x<flash_art.width;x++)
            if(flash_art.pixels[(y*flash_art.width+x)*4+3]>=128) {
                if(x<flash_bounds[0])flash_bounds[0]=x;if(y<flash_bounds[1])flash_bounds[1]=y;
                if(x>flash_bounds[2])flash_bounds[2]=x;if(y>flash_bounds[3])flash_bounds[3]=y;
            }
        if(flash_bounds[0]>flash_bounds[2]){stbi_image_free(flash_art.pixels);flash_art.pixels=NULL;}
    }
    snprintf(path,sizeof(path),"%sassets/walk-v1.png",base);
    walking.pixels=stbi_load(path,&walking.width,&walking.height,NULL,4);
    if(walking.pixels&&(walking.width!=2172||walking.height!=724)) {
        stbi_image_free(walking.pixels);walking.pixels=NULL;
    }
    snprintf(path,sizeof(path),"%sassets/cavern-tissue-v2.png",base);
    cavern_rock.pixels=stbi_load(path,&cavern_rock.width,&cavern_rock.height,NULL,4);
    snprintf(path,sizeof(path),"%sassets/cavern-details-v1.png",base);
    cavern_details.pixels=stbi_load(path,&cavern_details.width,&cavern_details.height,NULL,4);
    snprintf(path,sizeof(path),"%sassets/cavern-sandstone-v1.png",base);
    cavern_sandstone.pixels=stbi_load(path,&cavern_sandstone.width,&cavern_sandstone.height,NULL,4);
    snprintf(path,sizeof(path),"%sassets/cavern-ribs-public-v1.png",base);
    cavern_carvings.pixels=stbi_load(path,&cavern_carvings.width,&cavern_carvings.height,NULL,4);
    if(cavern_carvings.pixels&&(cavern_carvings.width!=256||cavern_carvings.height!=128)) {
        stbi_image_free(cavern_carvings.pixels);cavern_carvings.pixels=NULL;
    }
    if(cavern_carvings.pixels) {
        /* Packed approved material caches; no gameplay preview is distributed. */
        for(int cell=0;cell<2;cell++)for(int y=0;y<128;y++)
            memcpy(modern_bone[cell?2:0]+y*128*4,
                   cavern_carvings.pixels+(y*256+cell*128)*4,128*4);
    }
    /* Separate connected ivory rib material from isolated flesh highlights.
       Component classification is cached once, not a frame-time color guess. */
    if(cavern_carvings.pixels) {
        unsigned char candidate[128*128]={0},visited[128*128]={0};
        int queue[128*128];
        for(int i=0;i<128*128;i++) {
            const unsigned char *q=modern_bone[0]+i*4;
            candidate[i]=q[1]>65&&q[0]*10<q[1]*18;
        }
        for(int start=0;start<128*128;start++)if(candidate[start]&&!visited[start]) {
            int head=0,count=1;queue[0]=start;visited[start]=1;
            while(head<count) {
                int i=queue[head++],x=i%128,y=i/128;
                int next[4]={i-1,i+1,i-128,i+128};
                for(int d=0;d<4;d++) {
                    if((d==0&&!x)||(d==1&&x==127)||(d==2&&!y)||(d==3&&y==127))continue;
                    int j=next[d];if(candidate[j]&&!visited[j]){visited[j]=1;queue[count++]=j;}
                }
            }
            if(count>=80)for(int i=0;i<count;i++)rib_bone_mask[queue[i]]=1;
        }
        unsigned char grown[128*128];memcpy(grown,rib_bone_mask,sizeof(grown));
        for(int y=0;y<128;y++)for(int x=0;x<128;x++)if(rib_bone_mask[y*128+x]) {
            for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++) {
                int xx=x+dx,yy=y+dy;if(xx>=0&&xx<128&&yy>=0&&yy<128)grown[yy*128+xx]=1;
            }
        }
        memcpy(rib_bone_mask,grown,sizeof(grown));
    }
    snprintf(path,sizeof(path),"%sassets/cavern-brain-v2.png",base);
    cavern_brain.pixels=stbi_load(path,&cavern_brain.width,&cavern_brain.height,NULL,4);
    if(cavern_brain.pixels)for(int y=0;y<128;y++)for(int x=0;x<128;x++) {
        int x0=x*cavern_brain.width/128,x1=(x+1)*cavern_brain.width/128;
        int y0=y*cavern_brain.height/128,y1=(y+1)*cavern_brain.height/128;
        unsigned long sum[3]={0,0,0},n=0;
        for(int yy=y0;yy<y1;yy++)for(int xx=x0;xx<x1;xx++) {
            const unsigned char *q=cavern_brain.pixels+(yy*cavern_brain.width+xx)*4;
            for(int c=0;c<3;c++)sum[c]+=q[c];n++;
        }
        unsigned char *q=modern_bone[1]+(y*128+x)*4;
        if(n){const unsigned char *src=cavern_brain.pixels+(y0*cavern_brain.width+x0)*4;for(int c=0;c<4;c++)q[c]=src[c];}
    }
    snprintf(path,sizeof(path),"%sassets/guardian-reliefs-v1.png",base);
    guardian_reliefs.pixels=stbi_load(path,&guardian_reliefs.width,&guardian_reliefs.height,NULL,4);
    snprintf(path,sizeof(path),"%sassets/temple-jade-v2.png",base);
    temple_jade.pixels=stbi_load(path,&temple_jade.width,&temple_jade.height,NULL,4);
    snprintf(path,sizeof(path),"%sassets/temple-panel-v2.png",base);
    temple_panel.pixels=stbi_load(path,&temple_panel.width,&temple_panel.height,NULL,4);
    snprintf(path,sizeof(path),"%sassets/temple-statue-v2.png",base);
    temple_statue.pixels=stbi_load(path,&temple_statue.width,&temple_statue.height,NULL,4);
    snprintf(path,sizeof(path),"%sassets/flight-sandstone-v2.png",base);
    flight_sandstone.pixels=stbi_load(path,&flight_sandstone.width,&flight_sandstone.height,NULL,4);
    snprintf(path,sizeof(path),"%sassets/flight-heads-v4.png",base);
    flight_heads.pixels=stbi_load(path,&flight_heads.width,&flight_heads.height,NULL,4);
    snprintf(path,sizeof(path),"%sassets/arena-pilaster-v1.png",base);
    arena_pilaster.pixels=stbi_load(path,&arena_pilaster.width,&arena_pilaster.height,NULL,4);
    snprintf(path,sizeof(path),"%sassets/arena-bricks-v1.png",base);
    arena_bricks.pixels=stbi_load(path,&arena_bricks.width,&arena_bricks.height,NULL,4);
    snprintf(path,sizeof(path),"%sassets/dragon-boss-v3.png",base);
    dragon_art.pixels=stbi_load(path,&dragon_art.width,&dragon_art.height,NULL,4);
    if(dragon_art.pixels) {
        /* Shared canvas and scale keep the head fixed while the wings move.
         * Separate source regions exclude neighboring animation frames. */
        const int centers[3]={362,1086,1810},lo[3]={0,620,1510},hi[3]={620,1510,2172};
        for(int pose=0;pose<3;pose++)for(int y=0;y<144;y++)for(int x=0;x<160;x++) {
            int x0=(centers[pose]-425+x*850/160)*dragon_art.width/2172;
            int x1=(centers[pose]-425+(x+1)*850/160)*dragon_art.width/2172;
            int y0=y*dragon_art.height/144,y1=(y+1)*dragon_art.height/144;
            uint64_t a=0,r=0,g=0,b=0;int count=0;
            for(int sy=y0;sy<y1;sy++)for(int sx=x0;sx<x1;sx++) {
                count++;if(sx<lo[pose]*dragon_art.width/2172||sx>=hi[pose]*dragon_art.width/2172||sx<0||sx>=dragon_art.width)continue;
                const unsigned char *q=dragon_art.pixels+(sy*dragon_art.width+sx)*4;
                a+=q[3];r+=q[0]*q[3];g+=q[1]*q[3];b+=q[2]*q[3];
            }
            unsigned char *q=dragon_filtered[pose]+(y*160+x)*4;
            if(a&&count){q[0]=r/a;q[1]=g/a;q[2]=b/a;q[3]=a/count;}
        }
    }
    snprintf(path,sizeof(path),"%sassets/boss-cannon-v1.png",base);
    boss_cannon_art.pixels=stbi_load(path,&boss_cannon_art.width,&boss_cannon_art.height,NULL,4);
    if(boss_cannon_art.pixels)for(int pose=0;pose<2;pose++) {
        int left=boss_cannon_art.width,top=boss_cannon_art.height,right=-1,bottom=-1;
        for(int y=0;y<boss_cannon_art.height;y++)for(int x=pose*boss_cannon_art.width/2;x<(pose+1)*boss_cannon_art.width/2;x++) {
            const unsigned char *q=boss_cannon_art.pixels+(y*boss_cannon_art.width+x)*4;
            if(q[3]<128)continue;
            if(x<left)left=x;if(x>right)right=x;if(y<top)top=y;if(y>bottom)bottom=y;
        }
        int w=right-left+1,h=bottom-top+1;if(w<=0||h<=0)continue;
        for(int level=0;level<2;level++) {
            int size=level?32:64;unsigned char *out=level?boss_cannon_small[pose]:boss_cannon_pixels[pose];
            for(int y=0;y<size;y++)for(int x=0;x<size;x++) {
                int x0=left+x*w/size,x1=left+(x+1)*w/size,y0=top+y*h/size,y1=top+(y+1)*h/size;
                if(x1<=x0)x1=x0+1;if(y1<=y0)y1=y0+1;
                uint64_t a=0,r=0,g=0,b=0;int count=0;
                for(int sy=y0;sy<y1;sy++)for(int sx=x0;sx<x1;sx++) {
                    const unsigned char *q=boss_cannon_art.pixels+(sy*boss_cannon_art.width+sx)*4;
                    a+=q[3];r+=q[0]*q[3];g+=q[1]*q[3];b+=q[2]*q[3];count++;
                }
                unsigned char *q=out+(y*size+x)*4;
                if(a&&count){q[0]=r/a;q[1]=g/a;q[2]=b/a;q[3]=a/count;}
            }
        }
    }
    snprintf(path,sizeof(path),"%sassets/late-guardian-v1.png",base);
    late_guardian_art.pixels=stbi_load(path,&late_guardian_art.width,&late_guardian_art.height,NULL,4);
    if(late_guardian_art.pixels) {
        int cell=late_guardian_art.width/2;
        for(int frame=0;frame<2;frame++)for(int y=0;y<96;y++)for(int x=0;x<96;x++) {
            int x0=frame*cell+x*cell/96,x1=frame*cell+(x+1)*cell/96;
            int y0=y*late_guardian_art.height/96,y1=(y+1)*late_guardian_art.height/96;
            unsigned long long a=0,r=0,g=0,b=0;int count=0;
            for(int sy=y0;sy<y1;sy++)for(int sx=x0;sx<x1;sx++) {
                const unsigned char *q=late_guardian_art.pixels+(sy*late_guardian_art.width+sx)*4;
                a+=q[3];r+=q[0]*q[3];g+=q[1]*q[3];b+=q[2]*q[3];count++;
            }
            unsigned char *q=late_guardian_filtered[frame]+(y*96+x)*4;
            if(a&&count){q[0]=r/a;q[1]=g/a;q[2]=b/a;q[3]=a/count;}
        }
    }
    snprintf(path,sizeof(path),"%sassets/flight-gateway-v1.png",base);
    flight_gateway.pixels=stbi_load(path,&flight_gateway.width,&flight_gateway.height,NULL,4);
    if(flight_gateway.pixels) {
        int cell=flight_gateway.width/2;
        for(int frame=0;frame<2;frame++)for(int y=0;y<320;y++)for(int x=0;x<448;x++) {
            int x0=frame*cell+x*cell/448,x1=frame*cell+(x+1)*cell/448;
            /* The source has a transparent footer beneath the foundation. */
            int art_height=flight_gateway.height*732/749;
            int y0=y*art_height/320,y1=(y+1)*art_height/320;
            if(x1<=x0)x1=x0+1;if(y1<=y0)y1=y0+1;
            uint64_t alpha=0,r=0,g=0,b=0;int count=0;
            for(int sy=y0;sy<y1;sy++)for(int sx=x0;sx<x1;sx++) {
                const unsigned char *p=flight_gateway.pixels+(sy*flight_gateway.width+sx)*4;
                alpha+=p[3];r+=p[0]*p[3];g+=p[1]*p[3];b+=p[2]*p[3];count++;
            }
            unsigned char *p=gateway_filtered[frame]+(y*448+x)*4;
            if(alpha&&count){p[0]=r/alpha;p[1]=g/alpha;p[2]=b/alpha;p[3]=alpha/count;}
        }
    }
    snprintf(path,sizeof(path),"%sassets/flight-water-v2.png",base);
    flight_water.pixels=stbi_load(path,&flight_water.width,&flight_water.height,NULL,4);
    snprintf(path,sizeof(path),"%sassets/flight-turret-v1.png",base);
    flight_turret.pixels=stbi_load(path,&flight_turret.width,&flight_turret.height,NULL,4);
    if(flight_turret.pixels)for(int cell=0;cell<2;cell++) {
        int *b=flight_turret_bounds[cell];b[0]=flight_turret.width;b[1]=flight_turret.height;
        for(int y=0;y<flight_turret.height;y++)for(int x=cell*flight_turret.width/2;x<(cell+1)*flight_turret.width/2;x++)
            if(flight_turret.pixels[(y*flight_turret.width+x)*4+3]>=128) {
                if(x<b[0])b[0]=x;if(y<b[1])b[1]=y;if(x>b[2])b[2]=x;if(y>b[3])b[3]=y;
            }
    }
    if(flight_turret.pixels)for(int cell=0;cell<2;cell++) {
        const int *b=flight_turret_bounds[cell];int w=b[2]-b[0]+1,h=b[3]-b[1]+1;
        if(w<=0||h<=0)continue;
        for(int y=0;y<32;y++)for(int x=0;x<32;x++) {
            int x0=b[0]+x*w/32,x1=b[0]+(x+1)*w/32,y0=b[1]+y*h/32,y1=b[1]+(y+1)*h/32;
            if(x1<=x0)x1=x0+1;if(y1<=y0)y1=y0+1;
            uint64_t a=0,r=0,g=0,blue=0;int count=0;
            for(int sy=y0;sy<y1;sy++)for(int sx=x0;sx<x1;sx++) {
                const unsigned char *q=flight_turret.pixels+(sy*flight_turret.width+sx)*4;
                a+=q[3];r+=q[0]*q[3];g+=q[1]*q[3];blue+=q[2]*q[3];count++;
            }
            unsigned char *q=flight_turret_filtered[cell]+(y*32+x)*4;
            if(a&&count){q[0]=r/a;q[1]=g/a;q[2]=blue/a;q[3]=a/count;}
        }
    }
    snprintf(path,sizeof(path),"%sassets/flight-islet-v1.png",base);
    flight_islet.pixels=stbi_load(path,&flight_islet.width,&flight_islet.height,NULL,4);
    if(flight_islet.pixels) {
        int *b=flight_islet_bounds;b[0]=flight_islet.width;b[1]=flight_islet.height;
        for(int y=0;y<flight_islet.height;y++)for(int x=0;x<flight_islet.width;x++)
            if(flight_islet.pixels[(y*flight_islet.width+x)*4+3]>=128) {
                if(x<b[0])b[0]=x;if(y<b[1])b[1]=y;if(x>b[2])b[2]=x;if(y>b[3])b[3]=y;
            }
    }
    if(flight_islet.pixels)for(int y=0;y<32;y++)for(int x=0;x<32;x++) {
        const int *b=flight_islet_bounds;int w=b[2]-b[0]+1,h=b[3]-b[1]+1;
        int x0=b[0]+x*w/32,x1=b[0]+(x+1)*w/32,y0=b[1]+y*h/32,y1=b[1]+(y+1)*h/32;
        if(x1<=x0)x1=x0+1;if(y1<=y0)y1=y0+1;
        unsigned long long alpha=0,r=0,g=0,blue=0;int count=0;
        for(int sy=y0;sy<y1&&sy<flight_islet.height;sy++)for(int sx=x0;sx<x1&&sx<flight_islet.width;sx++) {
            const unsigned char *p=flight_islet.pixels+(sy*flight_islet.width+sx)*4;
            alpha+=p[3];r+=p[0]*p[3];g+=p[1]*p[3];blue+=p[2]*p[3];count++;
        }
        unsigned char *p=islet_filtered+(y*32+x)*4;
        if(alpha&&count){p[0]=r/alpha;p[1]=g/alpha;p[2]=blue/alpha;p[3]=alpha/count;}
    }
    snprintf(path,sizeof(path),"%sassets/title-v1.png",base);
    title_art.pixels=stbi_load(path,&title_art.width,&title_art.height,NULL,4);
    snprintf(path,sizeof(path),"%sassets/ui-font-v1.png",base);
    ui_font.pixels=stbi_load(path,&ui_font.width,&ui_font.height,NULL,4);
    if(flight_heads.pixels)for(int y=0;y<64;y++)for(int x=0;x<192;x++) {
        int x0=x*flight_heads.width/192,x1=(x+1)*flight_heads.width/192;
        int y0=y*flight_heads.height/64,y1=(y+1)*flight_heads.height/64;
        if(x1<=x0)x1=x0+1;if(y1<=y0)y1=y0+1;
        unsigned long long alpha=0,r=0,g=0,b=0;int count=0;
        for(int sy=y0;sy<y1&&sy<flight_heads.height;sy++)for(int sx=x0;sx<x1&&sx<flight_heads.width;sx++) {
            const unsigned char *p=flight_heads.pixels+(sy*flight_heads.width+sx)*4;
            alpha+=p[3];r+=p[0]*p[3];g+=p[1]*p[3];b+=p[2]*p[3];count++;
        }
        unsigned char *p=heads_filtered+(y*192+x)*4;
        if(alpha&&count){p[0]=r/alpha;p[1]=g/alpha;p[2]=b/alpha;p[3]=alpha/count;}
    }
    snprintf(path,sizeof(path),"%sassets/guardian-dart-v1.png",base);
    guardian_dart.pixels=stbi_load(path,&guardian_dart.width,&guardian_dart.height,NULL,4);
    if(guardian_dart.pixels) {
        int left=guardian_dart.width,top=guardian_dart.height,right=-1,bottom=-1;
        for(int y=0;y<guardian_dart.height;y++)for(int x=0;x<guardian_dart.width;x++)
            if(guardian_dart.pixels[(y*guardian_dart.width+x)*4+3]>=128) {
                if(x<left)left=x;if(x>right)right=x;if(y<top)top=y;if(y>bottom)bottom=y;
            }
        if(right>=left)for(int y=0;y<32;y++)for(int x=0;x<16;x++) {
            int w=right-left+1,h=bottom-top+1;
            int x0=left+x*w/16,x1=left+(x+1)*w/16,y0=top+y*h/32,y1=top+(y+1)*h/32;
            if(x1<=x0)x1=x0+1;if(y1<=y0)y1=y0+1;
            unsigned long long alpha=0,r=0,g=0,b=0;int count=0;
            for(int sy=y0;sy<y1;sy++)for(int sx=x0;sx<x1;sx++) {
                const unsigned char *p=guardian_dart.pixels+(sy*guardian_dart.width+sx)*4;
                alpha+=p[3];r+=p[0]*p[3];g+=p[1]*p[3];b+=p[2]*p[3];count++;
            }
            unsigned char *p=dart_filtered+(y*16+x)*4;
            if(alpha&&count){p[0]=r/alpha;p[1]=g/alpha;p[2]=b/alpha;p[3]=alpha/count;}
        }
    }
    SDL_free(base);
    if(maxx<minx)return;
    /* One union crop for the whole animation: do not stretch narrower wings
     * to fill the raised pose's width or move the body between frames. */
    crop_x=minx;crop_y=miny;crop_w=maxx-minx+1;crop_h=maxy-miny+1;
    filter_flight_art(crop_x,crop_y,crop_w,crop_h);
}
static bool known_flight_art(void) {
    static const uint32_t torso=0x8bbf2928u;
    for(int i=0;i<16;i++)if(!live_chr_matches((4*16+i)-(i),torso))return false;
    return true;
}
static bool known_ground_art(void) {
    static const uint32_t arm=0x79323a21u;
    for(int i=0;i<16;i++)if(!live_chr_matches((4*16+i)-(i),arm))return false;
    return true;
}
static bool find_player(void) {
    player_count=0;memset(player_slots,0,sizeof(player_slots));
    memset(native_power_slots,0,sizeof(native_power_slots));native_power_active=false;
    ground_scene=known_ground_art();
    if(!ground_scene&&!known_flight_art())return false;
    uint8_t remaining[256];memcpy(remaining,cyc_render_oam(),sizeof(remaining));
    for(int index=0;index<2;index++) {
        HeroPoseMatch match;
        if(!(ground_scene?ground_pose_find(remaining,&match):hero_pose_find(remaining,&match)))break;
        if(match.flash==8) {
            native_power_active=true;
            for(int slot=0;slot<64;slot++)if(match.slots[slot]) {
                native_power_slots[slot]=true;remaining[slot*4]=255;
            }
            continue;
        }
        if(ground_scene?(match.pose>=8?!jumping.pixels:match.pose>=5?!walking.pixels:match.pose>=3?!grounded.pixels:!ground.pixels):!hero[match.pose].pixels)break;
        players[player_count++]=match;
        for(int slot=0;slot<64;slot++)if(match.slots[slot]) {
            player_slots[slot]=true;remaining[slot*4]=255;
        }
    }
    if(player_count==2) {
        int first[2]={64,64};
        for(int index=0;index<2;index++)for(int slot=0;slot<64;slot++)
            if(players[index].slots[slot]){first[index]=slot;break;}
        if(first[0]>first[1]){HeroPoseMatch swap=players[0];players[0]=players[1];players[1]=swap;}
    }
    return player_count>0||native_power_active;
}
static void find_orbs(void) {
    orb_count=0;if(!orb.pixels||!known_flight_art())return;
    const uint8_t *oam=cyc_render_oam();
    for(int seed=0;seed<64&&orb_count<16;seed++) {
        const uint8_t *s=oam+seed*4;
        if(player_slots[seed]||s[0]>=239||(s[1]!=129&&s[1]!=130)||(s[2]&3))continue;
        int x=s[3]-((s[2]&64)?8:0),y=s[0]+1-((s[2]&128)?8:0);
        int slots[4],count=0;bool valid=true;
        for(int part=0;part<4;part++) {
            int px=x+(part/2)*8,py=y+(part%2)*8,attr=(part/2)*64+(part%2)*128;
            if(px<0||px>=256||py<1||py>=240)continue;
            int found=-1;
            for(int i=0;i<64;i++) {
                const uint8_t *q=oam+i*4;
                if(q[0]<239&&q[0]+1==py&&q[3]==px&&q[1]==s[1]&&(q[2]&0xc3)==attr){found=i;break;}
            }
            if(found<0){valid=false;break;}slots[count++]=found;
        }
        if(valid&&count>=2) {
            int first=64;for(int k=0;k<count;k++)if(slots[k]<first)first=slots[k];
            orbs[orb_count++]=(OrbMatch){x,y,s[1]-129,first};
            for(int k=0;k<count;k++)player_slots[slots[k]]=true;
        }
    }
}
static void find_wyverns(void) {
    wyvern_count=0;if(!ground_scene||!wyvern.pixels)return;
    uint8_t remaining[256];memcpy(remaining,cyc_render_oam(),sizeof(remaining));
    for(int slot=0;slot<64;slot++)if(player_slots[slot])remaining[slot*4]=255;
    for(int count=0;count<8;count++) {
        HeroPoseMatch match;if(!wyvern_pose_find(remaining,&match))break;
        wyverns[wyvern_count++]=match;
        for(int slot=0;slot<64;slot++)if(match.slots[slot]) {
            player_slots[slot]=true;remaining[slot*4]=255;
        }
    }
}
static void find_pods(void) {
    pod_count=0;if((!pod.pixels&&!spindle.pixels)||!known_flight_art())return;
    static const uint32_t patterns[10]={0xc3007970u,0x1d9c4b96u,0x03ff9a34u,0xe7e19a81u,0x4536fca4u,0xb10923d9u,0x43d709fcu,0x4bb72145u,0x1ac2e01eu,0xeb77db65u};
    bool valid[3]={pod.pixels!=NULL,pod.pixels!=NULL,spindle.pixels!=NULL};
    for(int tile=0;tile<10;tile++)for(int i=0;i<16;i++)
        if(!chr_matches(((131+tile)*16+i)-(i),patterns[tile]))valid[tile<2?2:tile<6?0:1]=false;
    uint8_t remaining[256];memcpy(remaining,cyc_render_oam(),sizeof(remaining));
    for(int slot=0;slot<64;slot++)if(player_slots[slot])remaining[slot*4]=255;
    for(int count=0;count<24;count++) {
        HeroPoseMatch match;if(!pod_pose_find(remaining,&match))break;
        if(!valid[match.pose]) {
            for(int slot=0;slot<64;slot++)if(match.slots[slot])remaining[slot*4]=255;
            continue;
        }
        pods[pod_count++]=match;
        for(int slot=0;slot<64;slot++)if(match.slots[slot]) {
            player_slots[slot]=true;remaining[slot*4]=255;
        }
    }
}
static void find_bolts(void) {
    bolt_count=0;if(!bolt.pixels||!known_flight_art())return;
    const uint8_t *oam=cyc_render_oam();
    for(int seed=0;seed<64&&bolt_count<32;seed++) {
        const uint8_t *s=oam+seed*4;
        if(player_slots[seed]||s[0]>=239||s[1]!=128||(s[2]&0x63))continue;
        int x=s[3],y=s[0]+1-((s[2]&128)?8:0),slots[2],count=0;bool valid=true;
        for(int part=0;part<2;part++) {
            int py=y+part*8;if(py<1||py>=240)continue;
            int found=-1;
            for(int slot=0;slot<64;slot++) {
                const uint8_t *q=oam+slot*4;
                if(q[0]<239&&q[0]+1==py&&q[3]==x&&q[1]==128&&(q[2]&0xe3)==part*128){found=slot;break;}
            }
            if(found<0){valid=false;break;}slots[count++]=found;
        }
        if(valid&&count) {
            int first=64;for(int k=0;k<count;k++)if(slots[k]<first)first=slots[k];
            bolts[bolt_count++]=(OrbMatch){x,y,0,first};
            for(int k=0;k<count;k++)player_slots[slots[k]]=true;
        }
    }
}
static uint32_t background_pixel(int x,int y) {
    uint8_t mask=presentation_line_mask(y);
    if(!(mask&8)||(x<8&&!(mask&2)))return presentation_color(0);
    int sx=(presentation_line_scroll_x(y)+x)%512,sy=presentation_line_scroll_y(y)%480;
    int cx=(sx%256)/8,cy=(sy%240)/8,nt=0x2000+(sx/256+(sy/240)*2)*0x400;
    int tile=presentation_nametable(nt+cy*32+cx);
    int attr=presentation_nametable(nt+0x3c0+(cy/4)*8+cx/4);
    int palette=(attr>>((cy&2)*2+(cx&2)))&3;
    int addr=presentation_line_bg_table(y)+tile*16+sy%8,bit=7-sx%8;
    int value=((presentation_chr(addr)>>bit)&1)|(((presentation_chr(addr+8)>>bit)&1)<<1);
    return presentation_color(value?palette*4+value:0);
}
static int keep_sprite(int slot,int x,int y,int *out_x,void *ctx) {
    (void)y;(void)ctx;*out_x=x;return !player_slots[slot];
}
static void find_pellets(void) {
    pellet_count=0;bool flight=known_flight_art();
    if(!flight&&!ground_scene)return;
    static const uint32_t patterns[2]={0xfbb582e5u,0xeaffccbdu};
    const int tiles[2]={254,61};bool valid[2]={flight&&pellet.pixels!=NULL,flight&&shot.pixels!=NULL};
    for(int pose=0;pose<2;pose++)for(int i=0;i<16;i++)
        if(!chr_matches((tiles[pose]*16+i)-(i),patterns[pose]))valid[pose]=false;
    static const uint32_t horizontal_pattern=0xdc638c05u;
    bool horizontal_valid=ground_scene&&shot.pixels!=NULL;
    for(int i=0;i<16;i++)if(!chr_matches((73*16+i)-(i),horizontal_pattern))horizontal_valid=false;
    static const uint32_t impact_pattern=0x0ba8ecb3u;
    bool impact_valid=ground_scene&&impact.pixels!=NULL;
    for(int i=0;i<16;i++)if(!chr_matches((87*16+i)-(i),impact_pattern))impact_valid=false;
    const uint8_t *oam=cyc_render_oam();
    for(int slot=0;slot<64;slot++) {
        const uint8_t *p=oam+slot*4;
        int pose=p[1]==254?0:p[1]==61?1:p[1]==73?2:p[1]==87?3:-1;
        if(pose<0||player_slots[slot]||p[0]>=207)continue;
        if(pose==3?(!impact_valid||(p[2]&0xa3)!=0):
           pose==2?(!horizontal_valid||(p[2]&0xa3)!=2):(!valid[pose]||(p[2]&0xe3)))continue;
        pellets[pellet_count++]=(OrbMatch){p[3],p[0]+1,pose,slot};player_slots[slot]=true;
    }
}
static void find_extra_effects(void) {
    bool flight=known_flight_art();if(!flight&&!ground_scene)return;
    static const uint32_t patterns[4]={0x8845be7du,0xfa1c0e66u,0xbd786f95u,0x09a18705u};
    const int tiles[4]={92,93,197,202},attrs[4]={0,0,1,2};const uint8_t *oam=cyc_render_oam();
    for(int pose=0;pose<4;pose++) {
        bool valid=(pose<2?flight:ground_scene)&&(pose==1?impact.pixels!=NULL:pose==3?flash_art.pixels!=NULL:pellet.pixels!=NULL);
        for(int i=0;i<16;i++)if(!chr_matches((tiles[pose]*16+i)-(i),patterns[pose]))valid=false;
        if(!valid)continue;
        for(int slot=0;slot<64&&pellet_count<64;slot++){const uint8_t *q=oam+slot*4;
            if(q[0]>=207||player_slots[slot]||q[1]!=tiles[pose]||(q[2]&227)!=attrs[pose])continue;
            pellets[pellet_count++]=(OrbMatch){q[3],q[0]+1,pose+4,slot};player_slots[slot]=true;
        }
    }
}
static int first_slot(const HeroPoseMatch *match) {
    for(int slot=0;slot<64;slot++)if(match->slots[slot])return slot;
    return 64;
}
static void find_guardians(void) {
    guardian_count=0;if(!guardian.pixels||!ground_scene)return;
    static const uint32_t pattern=0xf05dcbbcu;
    for(int i=0;i<16;i++)if(!chr_matches((188*16+i)-(i),pattern))return;
    static const uint32_t blue_pattern=0x9b606318u;
    bool blue_valid=blue_orb.pixels!=NULL;
    for(int i=0;i<16;i++)if(!chr_matches((160*16+i)-(i),blue_pattern))blue_valid=false;
    uint8_t remaining[256];memcpy(remaining,cyc_render_oam(),sizeof(remaining));
    for(int slot=0;slot<64;slot++)if(player_slots[slot])remaining[slot*4]=255;
    for(int count=0;count<24;count++) {
        HeroPoseMatch match;if(!guardian_pose_find(remaining,&match))break;
        if(match.pose==3&&!blue_valid) {
            for(int slot=0;slot<64;slot++)if(match.slots[slot])remaining[slot*4]=255;
            continue;
        }
        guardians[guardian_count++]=match;
        for(int slot=0;slot<64;slot++)if(match.slots[slot]) {
            player_slots[slot]=true;remaining[slot*4]=255;
        }
    }
}
static void find_spring_jumpers(void) {
    spring_count=0;if(!ground_scene||!spring_jumper.pixels)return;
    static const uint32_t signature=0xd6746ff3u;
    for(int k=0;k<16;k++)if(!chr_matches((228*16+k)-(k),signature))return;
    const uint8_t *o=cyc_render_oam();
    for(int seed=0;seed<64&&spring_count<24;seed++) {
        const uint8_t *q=o+seed*4;if(player_slots[seed]||q[0]>=207||(q[2]&3)!=3)continue;
        int t=q[1],pose=t>=228&&t<=229?0:t>=230&&t<=234?1:t>=235&&t<=237?2:-1;
        if(pose<0)continue;
        int rows=pose+2;
        for(int flip=0;flip<2;flip++)for(int part=0;part<rows*2;part++) {
            int r=part/2,c=part%2,tt,attr=3;
            if(pose==0){tt=228+c;if(r==1)attr|=128;}
            else if(pose==1){tt=r==2?234:230+r*2+c;if(r==2&&c)attr|=64;}
            else {tt=r==0||r==3?235:236+c;if((r==0||r==3)&&c)attr|=64;if(r>=2)attr|=128;}
            if(flip)attr^=128;
            if(t!=tt||(q[2]&0xe3)!=attr)continue;
            HeroPoseMatch m={.pose=pose,.flip=flip,.x=q[3]-c*8,.y=q[0]+1-(flip?rows-1-r:r)*8};
            bool ok=true;int count=0;
            for(int k=0;k<rows*2;k++) {
                int rr=k/2,cc=k%2,kt,ka=3,xx=m.x+cc*8,yy=m.y+(flip?rows-1-rr:rr)*8;
                if(pose==0){kt=228+cc;if(rr==1)ka|=128;}
                else if(pose==1){kt=rr==2?234:230+rr*2+cc;if(rr==2&&cc)ka|=64;}
                else {kt=rr==0||rr==3?235:236+cc;if((rr==0||rr==3)&&cc)ka|=64;if(rr>=2)ka|=128;}
                if(flip)ka^=128;
                if(xx<0||xx>=256||yy<1||yy>=208)continue;
                int slot=-1;for(int j=0;j<64;j++)if(!player_slots[j]&&o[j*4]<207&&o[j*4+3]==xx&&o[j*4]+1==yy&&o[j*4+1]==kt&&(o[j*4+2]&0xe3)==ka){slot=j;break;}
                if(slot<0){ok=false;break;}m.slots[slot]=true;count++;
            }
            if(ok&&count>=2){spring_matches[spring_count++]=m;for(int k=0;k<64;k++)if(m.slots[k])player_slots[k]=true;goto matched;}
        }
        matched:;
    }
}
static void find_chambers(void) {
    chamber_count=0;if(!ground_scene||!chamber_art.pixels)return;
    static const int tiles[10]={251,252,253,254,255,192,193,194,195,196};
    static const uint32_t patterns[10]={0xb400cac7u,0xf9457c65u,0x92a839aeu,0xaf773048u,0xa23e2768u,0xd74a8658u,0x2a0c1497u,0x28bbe88cu,0x13ace54eu,0x0b5d2d72u};
    for(int k=0;k<10;k++)for(int i=0;i<16;i++)if(!chr_matches((tiles[k]*16+i)-(i),patterns[k]))return;
    for(int pose=0;pose<5;pose++) {
        int *b=chamber_native[pose];b[0]=b[1]=16;b[2]=b[3]=0;
        for(int y=0;y<16;y++)for(int x=0;x<16;x++) {
            int tile=tiles[pose*2+y/8],bit=x<8?7-x:x-8;
            int hi=presentation_chr(tile*16+(y%8)+8);
            if((hi>>bit)&1){if(x<b[0])b[0]=x;if(y<b[1])b[1]=y;if(x>b[2])b[2]=x;if(y>b[3])b[3]=y;}
        }
        if(b[0]>0)b[0]--;if(b[1]>0)b[1]--;if(b[2]<15)b[2]++;if(b[3]<15)b[3]++;
    }
    uint8_t remaining[256];memcpy(remaining,cyc_render_oam(),sizeof(remaining));
    for(int slot=0;slot<64;slot++)if(player_slots[slot])remaining[slot*4]=255;
    while(chamber_count<32){HeroPoseMatch m;if(!chamber_pose_find(remaining,&m))break;
        chambers[chamber_count++]=m;
        for(int slot=0;slot<64;slot++)if(m.slots[slot]){player_slots[slot]=true;remaining[slot*4]=255;}
    }
}
static void find_turrets(void) {
    turret_count=0;if(!ground_scene||!turret_art.pixels)return;
    static const uint32_t patterns[4]={0xcdc9f362u,0xd1ccf686u,0x41195292u,0x0c4d4891u};
    for(int k=0;k<4;k++)for(int i=0;i<16;i++)if(!chr_matches(((198+k)*16+i)-(i),patterns[k]))return;
    uint8_t remaining[256];memcpy(remaining,cyc_render_oam(),sizeof(remaining));
    for(int slot=0;slot<64;slot++)if(player_slots[slot])remaining[slot*4]=255;
    while(turret_count<32){HeroPoseMatch m;if(!turret_pose_find(remaining,&m))break;
        turrets[turret_count++]=m;
        for(int slot=0;slot<64;slot++)if(m.slots[slot]){player_slots[slot]=true;remaining[slot*4]=255;}
    }
}
static void find_missiles(void) {
    missile_count=0;if(!known_flight_art()||!flash_art.pixels)return;
    static const uint32_t pattern=0x34f7c63du;
    for(int i=0;i<16;i++)if(!chr_matches((83*16+i)-(i),pattern))return;
    uint8_t remaining[256];memcpy(remaining,cyc_render_oam(),sizeof(remaining));
    for(int slot=0;slot<64;slot++)if(player_slots[slot])remaining[slot*4]=255;
    while(missile_count<32){HeroPoseMatch m;if(!missile_pose_find(remaining,&m))break;
        missiles[missile_count++]=m;
        for(int slot=0;slot<64;slot++)if(m.slots[slot]){player_slots[slot]=true;remaining[slot*4]=255;}
    }
}
static void find_swirls(void) {
    swirl_count=0;if(!known_flight_art()||!swirl_art.pixels)return;
    static const uint32_t patterns[4]={0x0f6b0cc9u,0xcc9a0fc9u,0xd44e1f55u,0x54a29fc5u};
    static const int tiles[4]={32,33,37,46};
    for(int k=0;k<4;k++)for(int i=0;i<16;i++)if(!chr_matches((tiles[k]*16+i)-(i),patterns[k]))return;
    uint8_t remaining[256];memcpy(remaining,cyc_render_oam(),sizeof(remaining));
    for(int slot=0;slot<64;slot++)if(player_slots[slot])remaining[slot*4]=255;
    while(swirl_count<64){HeroPoseMatch m;if(!swirl_pose_find(remaining,&m))break;
        swirls[swirl_count++]=m;
        for(int slot=0;slot<64;slot++)if(m.slots[slot]){player_slots[slot]=true;remaining[slot*4]=255;}
    }
}
static void find_halos(void) {
    halo_count=0;if(!known_flight_art()||!halo_art.pixels)return;
    static const uint32_t pattern=0xf9dfba95u;
    for(int i=0;i<16;i++)if(!chr_matches((94*16+i)-(i),pattern))return;
    uint8_t remaining[256];memcpy(remaining,cyc_render_oam(),sizeof(remaining));
    for(int slot=0;slot<64;slot++)if(player_slots[slot])remaining[slot*4]=255;
    while(halo_count<32){HeroPoseMatch m;if(!halo_pose_find(remaining,&m))break;
        halos[halo_count++]=m;
        for(int slot=0;slot<64;slot++)if(m.slots[slot]){player_slots[slot]=true;remaining[slot*4]=255;}
    }
}
static void find_flashes(void) {
    flash_count=0;if((!ground_scene&&!known_flight_art())||!flash_art.pixels)return;
    for(int i=0;i<16;i++)if(presentation_chr(95*16+i)!=60)return;
    uint8_t remaining[256];memcpy(remaining,cyc_render_oam(),sizeof(remaining));
    for(int slot=0;slot<64;slot++)if(player_slots[slot])remaining[slot*4]=255;
    while(flash_count<32){HeroPoseMatch m;if(!flash_pose_find(remaining,&m))break;
        flashes[flash_count++]=m;
        for(int slot=0;slot<64;slot++)if(m.slots[slot]){player_slots[slot]=true;remaining[slot*4]=255;}
    }
}
static void find_cycling(void) {
    cycling_count=0;if(!known_flight_art()||!blue_orb.pixels)return;
    static const uint32_t patterns[2]={0x6a855936u,0x21735086u};
    for(int k=0;k<2;k++)for(int i=0;i<16;i++)
        if(!chr_matches(((k?90:86)*16+i)-(i),patterns[k]))return;
    uint8_t remaining[256];memcpy(remaining,cyc_render_oam(),sizeof(remaining));
    for(int slot=0;slot<64;slot++)if(player_slots[slot])remaining[slot*4]=255;
    while(cycling_count<32) {
        HeroPoseMatch match;if(!cycling_pose_find(remaining,&match))break;
        if(match.pose==1) {
            static const uint32_t p_pattern=0xfd6107e5u;
            bool valid=power_art.pixels!=NULL;
            for(int i=0;i<16;i++)if(!chr_matches((91*16+i)-(i),p_pattern))valid=false;
            if(!valid){match.pose=0;for(int slot=0;slot<64;slot++)if(remaining[slot*4+1]==91)match.slots[slot]=false;}
        }
        cycling[cycling_count++]=match;
        for(int slot=0;slot<64;slot++)if(match.slots[slot]){player_slots[slot]=true;remaining[slot*4]=255;}
    }
}
static void find_greens(void) {
    green_count=0;if(!ground_scene||!green_orb.pixels)return;
    static const uint32_t pattern=0x2c79497fu;
    for(int i=0;i<16;i++)if(!chr_matches((132*16+i)-(i),pattern))return;
    uint8_t remaining[256];memcpy(remaining,cyc_render_oam(),sizeof(remaining));
    for(int slot=0;slot<64;slot++)if(player_slots[slot])remaining[slot*4]=255;
    for(int count=0;count<32;count++) {
        HeroPoseMatch match;if(!green_pose_find(remaining,&match))break;
        greens[green_count++]=match;
        for(int slot=0;slot<64;slot++)if(match.slots[slot]){player_slots[slot]=true;remaining[slot*4]=255;}
    }
}
static void find_serpents(void) {
    serpent_count=0;if(!ground_scene||!serpent.pixels)return;
    static const uint32_t patterns[3]={0x8aa1fe0au,0x78de952bu,0x0259fa8eu};
    const int tiles[3]={177,180,181};
    for(int pose=0;pose<3;pose++)for(int i=0;i<16;i++)
        if(!chr_matches((tiles[pose]*16+i)-(i),patterns[pose]))return;
    uint8_t remaining[256];memcpy(remaining,cyc_render_oam(),sizeof(remaining));
    for(int slot=0;slot<64;slot++)if(player_slots[slot])remaining[slot*4]=255;
    for(int count=0;count<24;count++) {
        HeroPoseMatch match;if(!serpent_pose_find(remaining,&match))break;
        serpents[serpent_count++]=match;
        for(int slot=0;slot<64;slot++)if(match.slots[slot]){player_slots[slot]=true;remaining[slot*4]=255;}
    }
}
static void find_bursts(void) {
    burst_count=0;if((!ground_scene&&!known_flight_art())||!burst.pixels)return;
    static const uint32_t patterns[7]={0x90842621u,0x9813d9c5u,0x8effe53au,0xccabe63du,0x8c9f9ea7u,0xfba2e4b5u,0x5344a9a5u};
    const int tiles[7]={125,126,127,115,119,123,124};
    for(int pose=0;pose<7;pose++)for(int i=0;i<16;i++)
        if(!chr_matches((tiles[pose]*16+i)-(i),patterns[pose]))return;
    uint8_t remaining[256];memcpy(remaining,cyc_render_oam(),sizeof(remaining));
    for(int slot=0;slot<64;slot++)if(player_slots[slot])remaining[slot*4]=255;
    for(int count=0;count<32;count++) {
        HeroPoseMatch match;if(!burst_pose_find(remaining,&match))break;
        bursts[burst_count++]=match;
        for(int slot=0;slot<64;slot++)if(match.slots[slot]){player_slots[slot]=true;remaining[slot*4]=255;}
    }
}
static uint32_t energy_color(const unsigned char *p,int palette) {
    int r=p[0],g=p[1],b=p[2],hi=r>g?r:g;hi=hi>b?hi:b;
    int lo=r<g?r:g;lo=lo<b?lo:b;
    uint32_t accent=presentation_color(16+palette*4+2),light=presentation_color(16+palette*4+3);
    if(hi-lo>32){r=((accent>>16)&255)*hi/255;g=((accent>>8)&255)*hi/255;b=(accent&255)*hi/255;}
    if(!(light&0xffffff)&&!(accent&0xffffff))r=g=b=0;
    return 0xff000000u|((uint32_t)r<<16)|((uint32_t)g<<8)|b;
}
static uint32_t atmosphere(uint32_t p,int x,int y) {
    int r=(p>>16)&255,g=(p>>8)&255,b=p&255;
    if(b>100&&r<25&&g<40) {
        int wy=presentation_line_scroll_y(y);
        int mist=(int)(3*sin((x+wy*.2)*.045)+2*sin((wy+x*.3)*.07));
        r=8+y/24;g=25+y/8+mist;b=65+y/5+mist;
    }
    else if(r>95&&g>50&&b<65) {
        /* Warm sandstone retains the original carved shapes while gaining
         * a broader tonal range; the new character is composited afterward. */
        int grain=((x*13+presentation_line_scroll_y(y)*7)&3)-1;
        r=r*9/10+grain;g=g*8/10+grain;b=20+r/7;
    }
    return 0xff000000u|((uint32_t)r<<16)|((uint32_t)g<<8)|(uint32_t)b;
}
static uint32_t horizontal_atmosphere(uint32_t p,int x,int y) {
    int r=(p>>16)&255,g=(p>>8)&255,b=p&255;
    /* Background-only grading: keep hostile magenta sprites distinct from
     * the cavern and preserve the native HUD/projectile palettes. */
    if(r>80&&b>20&&g<80){r=25+r/9;g=26+g/4;b=45+b/5;}
    else if(r>180&&g>130&&b>100){r=150+r/4;g=125+g/5;b=95+b/5;}
    else if(r>120&&g>60&&b<130){r=120+r/4;g=85+g/5;b=58+b/5;}
    else if(b>120&&r<130){r=35+r/5;g=50+g/5;b=80+b/7;}
    else if(r<20&&g<20&&b<20){r=13;g=20;b=33;}
    int grain=(((x+presentation_line_scroll_x(y))*13+presentation_line_scroll_y(y)*7)&3)-1;
    r+=grain;g+=grain;b+=grain;
    r=r<0?0:r>255?255:r;g=g<0?0:g>255?255:g;b=b<0?0:b>255?255:b;
    return 0xff000000u|((uint32_t)r<<16)|((uint32_t)g<<8)|(uint32_t)b;
}
/* Read neighboring native bone pixels across tile and nametable seams.
   This is lighting only; no geometry is expanded into the wall or hazards. */
static int cavern_bone_level(int sx,int sy) {
    sx=(sx+512)%512;sy=(sy+480)%480;
    int cx=sx%256/8,cy=sy%240/8;
    int base=0x2000+(sx/256+sy/240*2)*0x400;
    int tile=presentation_nametable(base+cy*32+cx);
    int attr=presentation_nametable(base+0x3c0+cy/4*8+cx/4);
    int pal=(attr>>((cy&2)*2+(cx&2)))&3;
    if(pal!=1||tile<4)return 0;
    int bit=7-sx%8,row=sy%8;
    int value=(presentation_chr(0x1000+tile*16+row)>>bit&1)|
              ((presentation_chr(0x1000+tile*16+row+8)>>bit&1)<<1);
    int level=value==1?236:value==2?148:value==3?72:0;
    return level;
}
static bool cavern_tissue_at(int sx,int sy) {
    sx=(sx+512)%512;sy=(sy+480)%480;
    int cx=sx%256/8,cy=sy%240/8;
    int base=0x2000+(sx/256+sy/240*2)*0x400;
    int tile=presentation_nametable(base+cy*32+cx);
    int attr=presentation_nametable(base+0x3c0+cy/4*8+cx/4);
    return ((attr>>((cy&2)*2+(cx&2)))&3)==1&&tile>=64&&tile<108;
}
static void paint_cavern_background(void) {
    if(!background_art||!ground_scene||!cavern_rock.pixels)return;
    static const uint32_t patterns[156]={0xcac7fd97u,0xa140e71du,0x2d3e6a74u,0xc648891eu,0x09f46529u,0x0cc55d9au,0x9d21c869u,0xee75d30cu,0xe9507912u,0x151a46c7u,0x87ad5daau,0x9f851e7du,0xa6a8fe61u,0x47f5e72au,0x2e3347a4u,0xa0ad287du,0x1167aa75u,0x8fbd8399u,0xe2c8d5b5u,0xfce2e850u,0x9cda1c23u,0x4a06305du,0xecdef088u,0xbe99066fu,0xe1e8997bu,0x7d3724adu,0xa36dae07u,0x4caa2969u,0x48139a45u,0x128cccf5u,0xef4c6e6eu,0x2aecbdd4u,0x5d07f3a2u,0xdc564ec1u,0x500e6a1eu,0x65dca54bu,0xb681d11du,0xa7a1b58du,0xe4123571u,0xd4fc2f6du,0x19a92bb7u,0xdf58f9a4u,0x82f72cd3u,0x2eacd92au,0x3758e88bu,0x79cb8ffdu,0xb7d134a5u,0xf0f3a7ebu,0x5b5a3af0u,0xc1b2f42bu,0x12578295u,0x53a4bf7du,0x88e2c5dbu,0xa102776eu,0x8e5818c8u,0xd39b9af5u,0x415b4185u,0x0635c972u,0x894246bcu,0xc952217fu,0xc4e766bcu,0xf8f17f45u,0x56dcaf84u,0x30f81dc7u,0x8535ac14u,0x386be46au,0xb3b791f9u,0x9e7d5103u,0xb2b2eb04u,0x6275f3eau,0x9cdfde17u,0x5a4efa5bu,0x4e6899c5u,0x942fd2dau,0x599b447bu,0x2b6f0d72u,0xbf9440bfu,0xd7e1471cu,0xb4129aeeu,0x88c7a034u,0x3ea6a3fdu,0xed4afe95u,0xc48ddef0u,0xf0880c96u,0x10557020u,0xac572304u,0xcbf77b40u,0xdc419151u,0x46b842fdu,0x71ac3c5bu,0x2facee1fu,0x59929485u,0x21a9ee7au,0xe406107bu,0xbe59b00fu,0x60847e1au,0xfb3ad25au,0xd8a05a9bu,0xc30708a1u,0xfcc5af30u,0x32826a18u,0x6168acaeu,0x47f315a5u,0xca8bfc37u,0x14bb613fu,0xd86a6397u,0xf42aa1d5u,0x886e538fu,0x4382e5ccu,0x58a2c838u,0x8dde0419u,0xcb36f509u,0x18d4eea9u,0xe63cba5bu,0xf13d9153u,0x80d2687eu,0x571cc649u,0xb2bfa6bau,0x1f9c6dbcu,0xe4db055cu,0xe4f576a4u,0x8e94e220u,0xfddb2aa2u,0xb01572d0u,0xf25c60c2u,0xa80a8f53u,0x9d1a0785u,0xcd88bc45u,0xf8eba9dau,0x27217900u,0xae202138u,0x11b77076u,0xb1768fd0u,0xfbf7c4c2u,0xaaa4c026u,0x94d93b17u,0x4d746e6fu,0x8f5ece9fu,0xf1bf8d95u,0x975cb77du,0x41396365u,0x5f233201u,0x607ac7d0u,0xb1450a04u,0x8780c67eu,0x6d0bf219u,0x19924b45u,0x2e80369du,0x391c66ddu,0x8bd8dee1u,0x6bbc9c87u,0x71c6db7du,0xcecd5bfdu,0x9ad862d4u,0xf86cea21u,0xb001dc1cu};
    /* Individually gate decorations: reused or animated CHR keeps native art. */
    bool eligible[156];
    for(int tile=0;tile<156;tile++) {
        eligible[tile]=true;
        for(int i=0;i<16;i++)if(!chr_matches((0x1040+tile*16+i)-(i),patterns[tile]))eligible[tile]=false;
        if(tile<12&&!eligible[tile])return;
    }
    uint32_t light=presentation_color(2);int brightness=(light>>16)&255;
    if(((light>>8)&255)>brightness)brightness=(light>>8)&255;
    if((light&255)>brightness)brightness=light&255;
    if(brightness>126)brightness=126;
    for(int y=0;y<240;y++) {
        uint8_t mask=presentation_line_mask(y);
        if(!(mask&8)||presentation_line_bg_table(y)!=0x1000)continue;
        int sy=presentation_line_scroll_y(y)%480,scroll_x=presentation_line_scroll_x(y);
        for(int x=0;x<256;x++) {
            if(x<8&&!(mask&2))continue;
            /* Scale2x reads neighbours; leave its retained-sprite fringe. */
            bool occupied=sprite_guard[y*256+x]!=0;
            if(occupied)continue;
            int sx=(scroll_x+x)%512,cx=(sx%256)/8,cy=(sy%240)/8;
            int nt=0x2000+(sx/256+(sy/240)*2)*0x400;
            int tile=presentation_nametable(nt+cy*32+cx);
            if(tile<4)continue;
            int attr=presentation_nametable(nt+0x3c0+(cy/4)*8+cx/4);
            int palette=(attr>>((cy&2)*2+(cx&2)))&3;
            if(palette!=1&&(tile>159||!eligible[tile-4]))continue;
            if(palette==1&&!cavern_sandstone.pixels)continue;
            /* Hazard tiles 152..159 retain native silhouette and palette, never masonry. */
            if(palette>=2&&(tile<108||tile>127||!guardian_reliefs.pixels))continue;
            int bit=7-sx%8,row=sy%8;
            int lo=presentation_chr(0x1000+tile*16+row),hi=presentation_chr(0x1000+tile*16+row+8);
            int value=((lo>>bit)&1)|(((hi>>bit)&1)<<1);
            bool detail=palette==0&&cavern_details.pixels&&(tile<16?value==1:value!=0);
            if(palette==0&&!detail&&(value==1||(tile>=16&&value!=0)))continue;
            if(palette==1&&cavern_carvings.pixels) {
                /* New painted art replaces the old pixels, within native
                   foreground tile bounds. Ribs and sockets span 16x32. */
                int cell=1,px0=sx%64*2,py0=sy%64*2,flip=0,height=1;
                if(tile>=64&&tile<80) {
                    int group=(tile-64)/4,part=(tile-64)%4;
                    cell=0;px0=part/2*64+sx%8*8;
                    py0=((group%2)*16+part%2*8+row)*4;
                    flip=group>=2;height=2;
                } else if(tile>=80&&tile<96) { cell=1;px0=sx%64*2;py0=sy%64*2;height=1;
                } else if(tile>=56&&tile<64) {
                    cell=2;px0=sx%16*8;
                    py0=((tile-56)/4*16+tile%2*8+row)*4;height=2;
                } else if(tile>=96&&tile<108) { cell=1;px0=sx%64*2;py0=sy%64*2;height=1;
                }
                int edge_x=9,edge_y=9;
                if(cell==1)for(int d=1;d<=8;d++) {
                    if(edge_x==9&&(!cavern_tissue_at(sx-d,sy)||!cavern_tissue_at(sx+d,sy)))edge_x=d;
                    if(edge_y==9&&(!cavern_tissue_at(sx,sy-d)||!cavern_tissue_at(sx,sy+d)))edge_y=d;
                }
                uint32_t native_light=presentation_color(5);
                int fade=(native_light>>16)&255;
                if(((native_light>>8)&255)>fade)fade=(native_light>>8)&255;
                if(fade>236)fade=236;
                for(int yy=0;yy<2;yy++)for(int xx=0;xx<2;xx++) {
                    int px=(px0+xx*(cell==1?1:4))%128,py=(py0+yy*height)%128;
                    if(flip)py=127-py;
                    const unsigned char *q=modern_bone[cell]+(py*128+px)*4;
                    int r=q[0]*fade/236,g=q[1]*fade/236,b=q[2]*fade/236;
                    if(cell==0&&!rib_bone_mask[py*128+px]) {
                        /* Flesh between ribs shares the continuous wall membrane,
                           while the ivory bone keeps its painted highlights. */
                        int u=(sx*2+xx)%512,v=(sy*2+yy)%512;
                        if(u>=256)u=511-u;if(v>=256)v=511-v;
                        const unsigned char *wall=cavern_rock.pixels+((v*cavern_rock.height/256)*cavern_rock.width+u*cavern_rock.width/256)*4;
                        int wr=wall[0]*brightness*3/(126*5),wg=wall[1]*brightness*3/(126*5),wb=wall[2]*brightness*3/(126*5);
                        const unsigned char *tissue=modern_bone[1]+(((sy%64*2+yy)%128)*128+(sx%64*2+xx)%128)*4;
                        int tr=tissue[0]*fade/236*95/100,tg=tissue[1]*fade/236*90/100,tb=tissue[2]*fade/236*95/100;
                        r=(tr*4+wr)/5+24;g=(tg*4+wg)/5+12;b=(tb*4+wb)/5+13;
                        if(r>255)r=255;if(g>255)g=255;if(b>255)b=255;
                    }
                    if(cell==1) {
                        /* Burgundy tissue belongs to the same organic wall.
                           Feather only exposed borders, never internal tile seams. */
                        r=r*95/100;g=g*90/100;b=b*95/100;
                        int alpha=256;
                        int edge=edge_x<edge_y?edge_x:edge_y;
                        if(edge<=8)alpha=72+edge*20;
                        if(edge_x<=4&&edge_y<=4) {
                            int cx=4-edge_x,cy=4-edge_y,dist=cx*cx+cy*cy;
                            if(dist>9)alpha=dist>=18?0:(18-dist)*28;
                        }
                        int u=(sx*2+xx)%512,v=(sy*2+yy)%512;
                        if(u>=256)u=511-u;if(v>=256)v=511-v;
                        int tx=u*cavern_rock.width/256,ty=v*cavern_rock.height/256;
                        const unsigned char *wall=cavern_rock.pixels+(ty*cavern_rock.width+tx)*4;
                        int wr=wall[0]*brightness*3/(126*5),wg=wall[1]*brightness*3/(126*5),wb=wall[2]*brightness*3/(126*5);
                        r=(r*4+wr)/5;g=(g*4+wg)/5;b=(b*4+wb)/5;
                        /* Lift the raised mass, preserving the recessed wall. */
                        r+=24;g+=12;b+=13;
                        if(r>255)r=255;if(g>255)g=255;if(b>255)b=255;
                        r=(r*alpha+wr*(256-alpha))/256;
                        g=(g*alpha+wg*(256-alpha))/256;
                        b=(b*alpha+wb*(256-alpha))/256;
                    }
                    canvas[(y*2+yy)*512+x*2+xx]=0xff000000u|((uint32_t)r<<16)|((uint32_t)g<<8)|b;
                }
                continue;
            }
            bool complete_carving=palette>=2||(palette==1&&tile>=64&&tile<=95&&cavern_carvings.pixels);
            if(palette>=1&&!complete_carving&&(value==0||value==3))continue; /* Retain the carved outlines and empty space. */ /* Preserve crystals and every native vine/detail pixel. */
            for(int yy=0;yy<2;yy++)for(int xx=0;xx<2;xx++) {
                if(detail) {
                    int k=tile-4,px=k%16*16+sx%8*2+xx,py=k/16*16+row*2+yy;
                    int tx=px*cavern_details.width/256,ty=py*cavern_details.height/160;
                    const unsigned char *p=cavern_details.pixels+(ty*cavern_details.width+tx)*4;
                    uint32_t light=presentation_color(1);int fade=(light>>16)&255;
                    if(((light>>8)&255)>fade)fade=(light>>8)&255;if((light&255)>fade)fade=light&255;
                    if(fade>236)fade=236;
                    int lum=(p[0]*3+p[1]*5+p[2]*2)/10;
                    int r=(66+lum*126/255)*fade/236,g=(16+lum*58/255)*fade/236,b=(27+lum*58/255)*fade/236;
                    canvas[(y*2+yy)*512+x*2+xx]=0xff000000u|((uint32_t)r<<16)|((uint32_t)g<<8)|b;
                    continue;
                }
                /* Mirrored repetition joins generated edges exactly without
                 * altering the source asset; coordinates follow the camera. */
                int u=(sx*2+xx)%512,v=(sy*2+yy)%512;
                if(u>=256)u=511-u;if(v>=256)v=511-v;
                if(palette>=2) {
                    /* Blue arena walls: five four-quadrant motifs, gated above
                     * by their actual CHR and native arena palettes. */
                    int cell=(tile-108)/4,part=(tile-108)%4;
                    int px=(part/2)*16+(sx%8)*2+xx,py=(part%2)*16+row*2+yy;
                    int tx=(cell*32+px)*guardian_reliefs.width/160,ty=(109+py*478/32)*guardian_reliefs.height/709; /* Measured art band; generated margins stay outside sampling. */
                    const unsigned char *p=guardian_reliefs.pixels+(ty*guardian_reliefs.width+tx)*4;
                    uint32_t light=presentation_color(palette*4+1);int fade=(light>>16)&255;
                    if(((light>>8)&255)>fade)fade=(light>>8)&255;
                    if((light&255)>fade)fade=light&255;if(fade>236)fade=236;
                    int r=p[0]*fade/236,g=p[1]*fade/236,b=p[2]*fade/236;
                    canvas[(y*2+yy)*512+x*2+xx]=0xff000000u|((uint32_t)r<<16)|((uint32_t)g<<8)|b;
                    continue;
                }
                if(palette==1) {
                    static const int cells[8]={0,1,1,0,2,2,2,2};
                    static const int flips[8]={0,0,2,1,0,1,3,2};
                    int group=(tile-64)/4,cell=-1,part=0,flip=0;
                    if(tile>=64&&tile<=95){cell=cells[group];part=(tile-64)%4;flip=flips[group];}
                    if(cell>=0&&cavern_carvings.pixels) {
                        /* The native four-tile order is TL, BL, TR, BR.
                         * Texture each quadrant by identity, including camera-clipped panels. */
                        int px=(part/2)*16+(sx%8)*2+xx,py=(part%2)*16+row*2+yy;
                        if(flip&1)px=31-px;if(flip&2)py=31-py;
                        int tx=(cell*32+px)*cavern_carvings.width/96,ty=py*cavern_carvings.height/128;
                        const unsigned char *p=cavern_carvings.pixels+(ty*cavern_carvings.width+tx)*4;
                        uint32_t light=presentation_color(5);int fade=(light>>16)&255;
                        if(((light>>8)&255)>fade)fade=(light>>8)&255;
                        if((light&255)>fade)fade=light&255;if(fade>236)fade=236;
                        int r=p[0]*fade/236,g=p[1]*fade/236,b=p[2]*fade/236;
                        canvas[(y*2+yy)*512+x*2+xx]=0xff000000u|((uint32_t)r<<16)|((uint32_t)g<<8)|b;
                        continue;
                    }
                    int tx=u*cavern_sandstone.width/256,ty=v*cavern_sandstone.height/256;
                    const unsigned char *p=cavern_sandstone.pixels+(ty*cavern_sandstone.width+tx)*4;
                    int light=(p[0]+p[1]+p[2])/3,factor=160+light/3;
                    if(factor>256)factor=256;
                    uint32_t native=canvas[(y*2+yy)*512+x*2+xx];
                    int r=((native>>16)&255)*factor/256,g=((native>>8)&255)*factor/256,b=(native&255)*factor/256;
                    canvas[(y*2+yy)*512+x*2+xx]=0xff000000u|((uint32_t)r<<16)|((uint32_t)g<<8)|b;
                    continue;
                }
                int tx=u*cavern_rock.width/256,ty=v*cavern_rock.height/256;
                const unsigned char *p=cavern_rock.pixels+(ty*cavern_rock.width+tx)*4;
                int r=p[0]*brightness*3/(126*5),g=p[1]*brightness*3/(126*5),b=p[2]*brightness*3/(126*5);
                canvas[(y*2+yy)*512+x*2+xx]=0xff000000u|((uint32_t)r<<16)|((uint32_t)g<<8)|b;
            }
        }
    }
}
static void paint_flight_terrain(void) {
    if(!background_art||ground_scene||!known_flight_art()||!flight_sandstone.pixels)return;
    uint64_t water_clock=cyc_cycle_count();
    int water_x=water_motion?(int)((water_clock/178686u)%512u):0,water_y=water_motion?(int)((water_clock/714744u)%512u):0;
    static const uint32_t patterns[252]={0x69995d4fu,0x8c135708u,0x3ff47f97u,0xbef2c15fu,0x360779f5u,0x360779f5u,0x360779f5u,0x360779f5u,0xe3e62862u,0xc7922c81u,0x0c8eead5u,0x360779f5u,0xe514ab80u,0x360779f5u,0xa0373bf0u,0xe1fba627u,0x656ecc25u,0x42392144u,0x01ef472au,0x83efe243u,0x5488e5a7u,0x360779f5u,0x55960905u,0x360779f5u,0xf3e8cde0u,0x47a942b0u,0x97f4f0bau,0x118b278au,0xd4231f39u,0xc6f9a469u,0x1cb7d101u,0xfc0352f4u,0x4873e9bcu,0xab56168cu,0x763820bdu,0xee2836cbu,0xf1b15103u,0x7bc66396u,0xbf0fe823u,0x72d464fdu,0xce8e1da7u,0x360779f5u,0x3b939f3au,0x360779f5u,0x42853530u,0x6b1eb18fu,0x5707f464u,0x17209872u,0xd097d98du,0x75d2791du,0x2defe2a7u,0xfce8df89u,0x9c659141u,0x6e669dc2u,0xaee087d2u,0xe936bd63u,0xc2d589acu,0x179efde1u,0x26a9698cu,0xa3f6583au,0xa3fe6db7u,0x9109376fu,0x4d3e1bc5u,0x5769be7du,0xdbcff648u,0xf408a048u,0x3e98832au,0xe3aecb16u,0xcb609af9u,0x716175deu,0x360779f5u,0xe16c86d5u,0x47f1cf11u,0x1aa51f24u,0x292d4301u,0x46136691u,0x917c661au,0x39377a1bu,0xcdad19aeu,0x1a723973u,0x055a71bfu,0x880dd278u,0x3ff47f97u,0x8fc759adu,0x9c881e1du,0xd6e432a4u,0x9fccf874u,0xc237aa71u,0x4aa9115eu,0x69968a66u,0x2c497f03u,0xaa2922bfu,0x32467290u,0x9e7ed7d5u,0x66870802u,0x0eaa3284u,0x58d335c6u,0x86a815a8u,0x385d18a4u,0x79c09d68u,0xae6a1963u,0xb5dd9dc7u,0xdf823c1du,0xf4b8ec71u,0xf3ce38c9u,0xba8479cfu,0xc1314abdu,0x8f80bc19u,0xd7761384u,0xe34586f4u,0x839b6900u,0x67d6c8f2u,0x26779a3au,0x072a4c75u,0xbda84b80u,0x7ae1421fu,0x3d1f348du,0xbf0d8716u,0x449ae2c0u,0xa99159f0u,0xe2db4df4u,0x0f9b8e8bu,0xdeeca823u,0x639b7769u,0xae817133u,0x1601e15fu,0x8619d005u,0x7e665031u,0xf4451281u,0x46fac975u,0x4d9d82a9u,0x8ace77c0u,0x0c58331cu,0x76313ad8u,0x72d3c549u,0xdb37cf85u,0x3247dd15u,0xa4b11defu,0xd5d4e59au,0x04aa6898u,0x725b0399u,0xb93e950fu,0xd0d0f023u,0xe5762a71u,0x62ceb1cdu,0x035dbfaau,0x29010ebau,0x2a6a339fu,0x4db031d2u,0xf54cf92bu,0xed276219u,0x85871c44u,0x5e7babc1u,0x5f872b1cu,0xe7a2d395u,0x234c8b37u,0xad928b51u,0x57dcecc9u,0x5fbf8539u,0x18988011u,0x0ad6b986u,0xfcdf6e31u,0xf755a029u,0x63fd2e4bu,0xf27b785fu,0x7edf087cu,0x546c3fedu,0x062f3db5u,0x71fd373bu,0x48035a58u,0x539000d7u,0xcfd3ac63u,0xe3e62862u,0x159bae6eu,0xb0aa8f7bu,0x4de058d5u,0x68c8afe2u,0xefc4c276u,0xa0373bf0u,0x3afe6055u,0x44b7d2cdu,0xa90e85c5u,0xf56ac071u,0x5d32c705u,0x4a66065au,0x93eec616u,0x60bf4b0bu,0xa0ed9a05u,0xb0b26ca9u,0xc0bd8c24u,0x694da844u,0x36fd77dfu,0x8856c1bfu,0xd288c94du,0xe2647dedu,0x2f6b8a81u,0xef01d104u,0x1a8481a5u,0x9c5730f5u,0x360779f5u,0x800bdbb4u,0x360779f5u,0x6d8136b8u,0x423ed479u,0xa54bcfb3u,0x4221032du,0x17a8b4b5u,0x8a9fe58bu,0x75c53fcfu,0xe013a42du,0xcaf7aeb1u,0xae9e5fc3u,0x6cf1ff1eu,0xe295af5cu,0x66d4ec73u,0xae341cd4u,0x2764a392u,0x1ea112ecu,0xd60822efu,0xcd0fffd7u,0x8b082779u,0xd173bc5fu,0x6a4eb59eu,0x417f845bu,0x75e0758fu,0x5a827960u,0x3ff47f97u,0xc8f33b6fu,0xa23abeb5u,0xcccfe975u,0x360779f5u,0x360779f5u,0x360779f5u,0x360779f5u,0x6bf7aa7bu,0xd0dba635u,0xf27b785fu,0x3386d984u,0x03b48f5du,0x0e643cc1u,0xec4be99fu,0xecd39d44u,0x0255f845u,0xb089d079u,0x8c97109fu,0xbb558238u,0x920e080fu,0x53cb6744u,0xa9309e1fu,0xe9a38189u,0x9cf61ff0u,0x6a17e927u};
    bool eligible[256]={false};
    for(int t=4;t<256;t++) {
        bool match=true;
        for(int i=0;i<16;i++)if(!chr_matches((0x1000+t*16+i)-(i),patterns[t-4]))match=false;
        if(t<8&&!match)return;
        eligible[t]=match;
    }
    int8_t head_rects[64*60];memset(head_rects,-1,sizeof(head_rects));
    for(int y=0;y<240;y++) {
        uint8_t mask=presentation_line_mask(y);
        if(!(mask&8)||presentation_line_bg_table(y)!=0x1000)continue;
        int sy=presentation_line_scroll_y(y)%480;
        for(int x=0;x<256;x++) {
            if(x<8&&!(mask&2))continue;
            bool occupied=sprite_guard[y*256+x]!=0;
            if(occupied)continue;
            int sx=(presentation_line_scroll_x(y)+x)%512,cx=sx%256/8,cy=sy%240/8;
            int nt=0x2000+(sx/256+sy/240*2)*0x400;
            int tile=presentation_nametable(nt+cy*32+cx);
            if(!eligible[tile])continue;
            int attr=presentation_nametable(nt+0x3c0+cy/4*8+cx/4);
            int palette=(attr>>((cy&2)*2+(cx&2)))&3;
            bool head=tile>=96&&tile<160,water=false;
            bool islet=tile>=48&&tile<52&&(palette==1||palette==2)&&flight_islet.pixels;
            bool mine=tile>=48&&tile<64&&(palette==3||(tile>=52&&palette==2))&&flight_turret.pixels;
            if(head&&!flight_heads.pixels)continue;
            bool whole_head=false;
            if(head) {
                int k=tile-96,col=(k%32)/2,row=k/32*2+k%2;
                if(col<12) {
                    int face=col/6,ax=(sx/8-col%6+64)%64,ay=(sy/8-row+60)%60,index=ay*64+ax;
                    if(head_rects[index]<0) {
                        bool complete=true;
                        for(int c=0;c<6;c++)for(int r=0;r<4;r++) {
                            int tx=(ax+c)%64,ty=(ay+r)%60;
                            int address=0x2000+(tx/32+ty/30*2)*0x400+(ty%30)*32+tx%32;
                            if(presentation_nametable(address)!=96+face*12+c*2+r%2+r/2*32)complete=false;
                        }
                        head_rects[index]=complete;
                    }
                    whole_head=head_rects[index]!=0;
                }
            }
            int row=sy%8,bit=7-sx%8;
            int value=((presentation_chr(0x1000+tile*16+row)>>bit)&1)|
                (((presentation_chr(0x1000+tile*16+row+8)>>bit)&1)<<1);
            uint32_t native_color=presentation_color(value?palette*4+value:0);
            /* Shoreline and head tiles reuse the same blue in several
             * palette slots. Select the actual water colour, not one slot. */
            water=((native_color>>16)&255)<25&&((native_color>>8)&255)<40&&(native_color&255)>100;
            bool warm=((native_color>>16)&255)>95&&((native_color>>8)&255)>50&&(native_color&255)<65;
            /* Exact flight CHR gates permit every shoreline palette variant. */
            if(water&&!flight_water.pixels)continue;
            
            /* Shore tiles also contain open water and black silhouettes.
             * Only the two stone colours receive surface detail. */
            /* Complete coast material includes dark edge and cliff-base pixels. */
            /* World anchored mirrored sampling avoids a visible texture seam.
             * Preserve native strata, palette fades, cliffs and carved heads. */
            for(int yy=0;yy<2;yy++)for(int xx=0;xx<2;xx++) {
                int u=(sx*2+xx)%512,v=(sy*2+yy)%512;
                if(u>=256)u=511-u;if(v>=256)v=511-v;
                if(islet) {
                    int part=tile%4,px=part/2*16+sx%8*2+xx,py=part%2*16+row*2+yy;
                    const unsigned char *p=islet_filtered+(py*32+px)*4;
                    if(p[3]>=128) {
                        canvas[(y*2+yy)*512+x*2+xx]=0xff000000u|((uint32_t)p[0]<<16)|((uint32_t)p[1]<<8)|p[2];
                        continue;
                    }
                    /* Transparent corners expose the same world-aligned ocean. */
                    water=true;
                }
                if(mine) {
                    int part=tile%4,cell=tile>=56?1:0;
                    int px=part/2*16+sx%8*2+xx,py=part%2*16+row*2+yy;
                    const unsigned char *p=flight_turret_filtered[cell]+(py*32+px)*4;
                    if(p[3]>=128) {
                        canvas[(y*2+yy)*512+x*2+xx]=0xff000000u|((uint32_t)p[0]<<16)|((uint32_t)p[1]<<8)|p[2];
                        continue;
                    }
                    /* Transparent replacement corners expose the new stone,
                     * rather than retaining a fragment of the gray object. */
                }
                bool pixel_water=water;
                HeroArt *material=pixel_water?&flight_water:&flight_sandstone;
                if(pixel_water) {
                    /* Emulated cycles keep motion stable through pauses and
                     * state replay. Wrap after applying the shared offset so
                     * shoreline and palette boundaries cannot split the ocean. */
                    u=(sx*2+xx+water_x)%512;
                    v=(sy*2+yy+water_y)%512;
                    if(u>=256)u=511-u;if(v>=256)v=511-v;
                }
                int tx=u*material->width/256,ty=v*material->height/256;
                const unsigned char *p=material->pixels+(ty*material->width+tx)*4;
                if(pixel_water) {
                    /* A single material across every blue tile removes the
                     * rectangular joins between water palette assignments. */
                    int fade=((head||islet)?presentation_color(7):native_color)&255;if(fade>160)fade=160;
                    canvas[(y*2+yy)*512+x*2+xx]=0xff000000u|((uint32_t)(p[0]*fade*3/640)<<16)|((uint32_t)(p[1]*fade*3/640)<<8)|p[2]*fade*3/640;
                    continue;
                }
                {
                    uint32_t light=presentation_color(1);int fade=(light>>16)&255;
                    if(((light>>8)&255)>fade)fade=(light>>8)&255;if((light&255)>fade)fade=light&255;
                    if(fade>236)fade=236;
                    /* Shoreline tiles share the land material; subdued relief avoids
                     * exposing the original tile-sized shadow patches. */
                    int native_light=(native_color>>16)&255;
                    if(((native_color>>8)&255)>native_light)native_light=(native_color>>8)&255;
                    /* Native relief drives all cliff tiles alike: no tile-ID
                       brightness steps or independently projected shadow blocks. */
                    int shade=64+native_light*176/236;if(shade>240)shade=240;
                    int r=p[0]*fade*shade/(236*256),g=p[1]*fade*shade/(236*256),b=p[2]*fade*shade/(236*256);
                    canvas[(y*2+yy)*512+x*2+xx]=0xff000000u|((uint32_t)r<<16)|((uint32_t)g<<8)|b;
                    continue;
                }

            }
        }
    }
}

static void paint_flight_gateway(void) {
    if(!background_art||!flight_gateway.pixels||!known_flight_art())return;
    static const uint8_t tiles[6]={88,90,89,91,160,164};
    static const uint32_t patterns[6]={0xa97b8425u,0x9c213a59u,0xcf9d38c4u,0xcdac5b50u,0x49ef050eu,0x3a979d81u};
    static const uint32_t beam_patterns[64]={0x3e248fc2u,0xca951551u,0x69691905u,0x279b0691u,0xd2f39a19u,0x694df119u,0x2af73aeau,0x18a9e367u,0x2fb4be9du,0xd48f73dau,0x984b4553u,0x94ad0705u,0x1dead8e2u,0x6656bf8fu,0x324894b8u,0x3bc2ad35u,0x5bbe46b4u,0x219701adu,0x994c1759u,0xaceda455u,0x170bea14u,0xea826415u,0x4a9f64f3u,0xf7277defu,0xa97b8425u,0xcf9d38c4u,0x9c213a59u,0xcdac5b50u,0x30e57810u,0x7eb9e46cu,0xb242ffb1u,0xb51f45acu,0xe54f2b8fu,0x58a87ba7u,0xe54f2b8fu,0xc4ed0928u,0x91e88a3au,0x72ceb509u,0x61c0a898u,0x4af32117u,0x85f5b827u,0x2d72caa5u,0x69691905u,0x69691905u,0x69691905u,0x45f9edf5u,0xfed4029au,0xc54482c5u,0x42fdd3c5u,0x53077e52u,0x53077e52u,0x42fdd3c5u,0x35a96903u,0xc48ed975u,0xb43c9e71u,0x2d9aa13du,0xcc2567bfu,0x7b3d92ffu,0x17242e1du,0x11a51de3u,0xebca78afu,0x3b520c84u,0x2fa28694u,0xf10840c5u};
    for(int t=0;t<6;t++)for(int k=0;k<16;k++)if(!chr_matches((0x1000+tiles[t]*16+k)-(k),patterns[t]))return;
    int anchor_row=-1,anchor_col=-1,bank=0;
    for(int page=0;page<2&&anchor_row<0;page++)for(int row=0;row<29&&anchor_row<0;row++)for(int col=0;col<31;col++) {
        int nt=0x2000+page*0x400+row*32+col;
        if(presentation_nametable(nt)==88&&presentation_nametable(nt+1)==90&&
           presentation_nametable(nt+32)==89&&presentation_nametable(nt+33)==91) {
            anchor_row=row;anchor_col=col;bank=page;break;
        }
    }
    /* A stale complete mouth can also survive in the other nametable.
       Prefer a face supported by its currently streamed, matching rows. */
    {
        static const int face_tiles[10]={128,129,152,153,172,173,192,193,224,225};
        int best_score=1;
        for(int page=0;page<2;page++)for(int row=0;row<30;row++)for(int col=0;col<32;col++) {
            int tile=presentation_nametable(0x2000+page*0x400+row*32+col);
            for(int r=0;r<10;r++)if(tile==face_tiles[r]) {
                int ar=(row-16-r+60)%30,ac=(col-(r<6?2:0)+32)%32,score=0;
                for(int k=0;k<10;k++) {
                    int nr=(ar+16+k)%30,nc=(ac+(k<6?2:0))%32;
                    int nt=0x2000+page*0x400+nr*32+nc;
                    if(presentation_nametable(nt)==face_tiles[k]&&presentation_nametable(nt+(nc<31?1:-31))==face_tiles[k]+2)score++;
                }
                if(score>best_score){best_score=score;anchor_row=ar;anchor_col=ac;bank=page;}
            }
        }
    }
    int left=(anchor_col*8-48+256)%256,top=(anchor_row*8-112+240)%240;
    /* Streaming overwrites rows behind the camera. Draw the visible instance
     * supported by actual face rows, rather than repeating it into stale voids
     * on the other side of a mirrored nametable wrap. */
    static const uint8_t row_tiles[16]={128,129,152,153,172,173,192,193,224,225,64,65,72,73,88,89};
    bool row_valid[16]={false};
    for(int r=0;r<16;r++) {
        int nr=(anchor_row+16+r)%30,nc=(anchor_col+(r<6?2:0))%32;
        row_valid[r]=anchor_row>=0&&presentation_nametable(0x2000+bank*0x400+nr*32+nc)==row_tiles[r];
        if(anchor_row>=0&&r>=14&&!row_valid[r]) {
            int foot=(anchor_col+14)%32,nt=0x2000+bank*0x400+nr*32+foot;
            row_valid[r]=presentation_nametable(nt)==68+r-14&&presentation_nametable(nt+(foot<31?1:-31))==70+r-14;
        }
    }
    int display_top=top-(presentation_line_scroll_y(0)%240),best=-1;
    for(int copy=-1;copy<=1;copy++) {
        int candidate=top-(presentation_line_scroll_y(0)%240)+copy*240,score=0;
        for(int r=0;r<16;r++)if(row_valid[r]&&candidate+r*8<240&&candidate+r*8+8>0)score++;
        if(score>best){best=score;display_top=candidate;}
    }
    int mouth_row=(anchor_row+24)%30,mouth_col=(anchor_col+4)%32;
    int mouth=presentation_nametable(0x2000+bank*0x400+mouth_row*32+mouth_col);
    int pose=(mouth==0||mouth==2)?1:0;
    int fade=0;
    for(int i=1;i<16;i++) {
        uint32_t light=presentation_color(i);
        if(((light>>16)&255)>fade)fade=(light>>16)&255;
        if(((light>>8)&255)>fade)fade=(light>>8)&255;if((light&255)>fade)fade=light&255;
    }
    if(fade>236)fade=236;
    for(int y=0;y<240;y++) {
        uint8_t mask=presentation_line_mask(y);if(!(mask&8)||presentation_line_bg_table(y)!=0x1000)continue;
        int sy=presentation_line_scroll_y(y)%480;
        for(int x=0;x<256;x++) {
            if(x<8&&!(mask&2))continue;
            bool occupied=sprite_guard[y*256+x]!=0;
            if(occupied)continue;
            int sx=(presentation_line_scroll_x(y)+x)%512,page=sx/256;
            int nt=0x2000+(sx/256+sy/240*2)*0x400+(sy%240/8)*32+sx%256/8;
            int tile=presentation_nametable(nt),px=(sx%256-left+256)%256,py=(sy%240-top+240)%240;
            bool complete=anchor_row>=0&&page==bank&&px<224&&py<128&&row_valid[py/8]&&y>=display_top&&y<display_top+128;
            bool beam=tile>=64&&tile<128&&flight_sandstone.pixels;
            if(beam)for(int k=0;k<16;k++)if(!chr_matches((0x1000+tile*16+k)-(k),beam_patterns[tile-64]))beam=false;
            if(!complete&&!beam)continue;
            int row=sy%8,bit=7-sx%8;
            int value=((presentation_chr(0x1000+tile*16+row)>>bit)&1)|(((presentation_chr(0x1000+tile*16+row+8)>>bit)&1)<<1);
            for(int yy=0;yy<2;yy++)for(int xx=0;xx<2;xx++) {
                if(complete) {
                    const unsigned char *p=gateway_filtered[pose]+(((py*2+yy)*320/256)*448+px*2+xx)*4;
                    uint32_t bg=py>=124?canvas[(y*2+yy)*512+x*2+xx]:presentation_color(0);
                    int alpha=p[3],r=(((bg>>16)&255)*(255-alpha)+p[0]*fade*alpha/236)/255;
                    int g=(((bg>>8)&255)*(255-alpha)+p[1]*fade*alpha/236)/255;
                    int b=((bg&255)*(255-alpha)+p[2]*fade*alpha/236)/255;
                    canvas[(y*2+yy)*512+x*2+xx]=0xff000000u|((uint32_t)r<<16)|((uint32_t)g<<8)|b;
                } else if(value) {
                    int tx=(sx*2+xx)%256*flight_sandstone.width/256,ty=(sy*2+yy)%256*flight_sandstone.height/256;
                    const unsigned char *p=flight_sandstone.pixels+(ty*flight_sandstone.width+tx)*4;
                    int shade=value==1?144:value==2?192:224;
                    canvas[(y*2+yy)*512+x*2+xx]=0xff000000u|((uint32_t)(p[0]*fade*shade/(236*256))<<16)|((uint32_t)(p[1]*fade*shade/(236*256))<<8)|p[2]*fade*shade/(236*256);
                }
            }
        }
    }
}
static void paint_temple_background(void) {
    if(!background_art||!ground_scene||(!temple_jade.pixels&&!temple_panel.pixels&&!temple_statue.pixels))return;
    static const uint32_t patterns[116]={0xcbd1fa2fu,0x10d6b49du,0xfaccc571u,0x40d7c253u,0x69691905u,0xacb8bf45u,0x69691905u,0x6f8f09feu,0xe064d1a3u,0x682ef20fu,0x02d0cc9du,0x61abc8d0u,0xced27e0du,0x3f68a2abu,0x39ba715bu,0xb80f7547u,0xbfddc99cu,0xa1f413bdu,0xc1d9099au,0xeeddde0eu,0xd5d1ededu,0xa966f0f5u,0x8a9f3b17u,0x5ba85feeu,0x13c79b87u,0x3854fe01u,0x9b363b42u,0x3bc8f0d7u,0x6890117bu,0x2c8d5a68u,0x28f18571u,0x9bc67587u,0x94c45515u,0x39b47825u,0xfce375ddu,0xc449e01du,0x6c3dededu,0x317f12e5u,0x599c2595u,0x4d782180u,0x8f959864u,0xa1b7742bu,0x8c630392u,0xb771ae5bu,0x1ce241bau,0x49e59d0du,0x96a4d22du,0xc3959380u,0xdbb029e5u,0xe2521405u,0xba314620u,0x2311e75fu,0xdefb1a14u,0x06a21234u,0xcd8ca097u,0xe35167e3u,0xffd63645u,0x7f8675a5u,0x3a1b4169u,0x055a4295u,0xa7eb94a3u,0x3ca1626au,0x5482f47fu,0x930fe629u,0x901bee43u,0xc38567a4u,0xe0f85e5cu,0xecc834ddu,0xe4b26a6du,0x839129c6u,0x0c87cc67u,0x7ae059ddu,0x01bb9764u,0x6debb9fdu,0x01bb9764u,0x6debb9fdu,0x1ed62aacu,0x98fca06fu,0x0b2a8665u,0x60239d97u,0x007b6472u,0x2590da9du,0x34495635u,0x91e39a71u,0x363dfb24u,0x92e24a4eu,0x929fa01du,0x3453dbf4u,0x6debb9fdu,0x0282def9u,0x6debb9fdu,0x0282def9u,0x49d0e408u,0x35e575f4u,0xc67ca40eu,0x787dc392u,0x8db4fde4u,0x18c8a5adu,0xe94d1064u,0xb19eb04bu,0x8c87029du,0x3ccbaa9cu,0xb72688f9u,0x47564b51u,0x0b49a0bau,0xfc6574ebu,0x3a9da5dbu,0xb80f7547u,0x3796c84bu,0x4383754au,0x8e89d201u,0xca5a86abu,0x916f2a78u,0x9632beaeu,0x45e7683du,0x1c63e2c5u};
    bool eligible[116];
    for(int i=0;i<116;i++) {
        int tile=i<8?28+i:i<16?84+i-8:i<40?4+i-16:i<64?36+i-40:i<80?i:i<92?96+i-80:i<96?60+i-92:108+i-96;eligible[i]=true;
        for(int byte=0;byte<16;byte++)if(!chr_matches((0x1000+tile*16+byte)-(byte),patterns[i]))eligible[i]=false;
    }
    /* Brick, panel and statue patterns identify this temple CHR upload. */
    for(int i=0;i<116;i++)if(!eligible[i])return;
    for(int y=0;y<240;y++) {
        uint8_t mask=presentation_line_mask(y);
        if(!(mask&8)||presentation_line_bg_table(y)!=0x1000)continue;
        int sy=presentation_line_scroll_y(y)%480,scroll_x=presentation_line_scroll_x(y);
        for(int x=0;x<256;x++) {
            if(x<8&&!(mask&2))continue;
            bool occupied=sprite_guard[y*256+x]!=0;
            if(occupied)continue;
            int sx=(scroll_x+x)%512,cx=(sx%256)/8,cy=(sy%240)/8;
            int nt=0x2000+(sx/256+(sy/240)*2)*0x400;
            int tile=presentation_nametable(nt+cy*32+cx),index=-1;
            if(tile>=28&&tile<=35)index=tile-28;
            else if(tile>=84&&tile<=91)index=tile-84+8;
            else if(tile>=4&&tile<=27)index=tile-4+16;
            else if(tile>=36&&tile<=59)index=tile-36+40;
            else if(tile>=64&&tile<=79)index=tile;
            else if(tile>=96&&tile<=107)index=80+tile-96;
            else if(tile>=60&&tile<=63)index=92+tile-60;
            else if(tile>=108&&tile<=127)index=96+tile-108;
            if(index<0)continue;
            int attr=presentation_nametable(nt+0x3c0+(cy/4)*8+cx/4);
            int palette=(attr>>((cy&2)*2+(cx&2)))&3;
            bool jade=index<8,statue=index>=16&&index<64,floor=index>=64&&index<92,trim=index>=92&&index<96,masonry=index>=96;
            if(palette!=((jade||statue)?1:0))continue;
            HeroArt *art=statue?&temple_statue:jade?&temple_jade:&temple_panel;if(!art->pixels)continue;
            int row=sy%8,bit=7-sx%8;
            int value=((presentation_chr(0x1000+tile*16+row)>>bit)&1)|(((presentation_chr(0x1000+tile*16+row+8)>>bit)&1)<<1);
            if(masonry&&value==0)continue;
            if(jade&&value==0)continue; /* Preserve native brick silhouette and mortar gaps. */
            int part=jade?index%4:(index-8)%4;
            uint32_t light=presentation_color(palette*4+1);int fade=(light>>16)&255;
            if(((light>>8)&255)>fade)fade=(light>>8)&255;if((light&255)>fade)fade=light&255;
            int normal=(jade||statue)?216:236;if(fade>normal)fade=normal;
            for(int yy=0;yy<2;yy++)for(int xx=0;xx<2;xx++) {
                int px=(part/2)*16+(sx%8)*2+xx,py=(part%2)*16+row*2+yy;
                                if(statue) {
                    /* Six native 32x16 bands alternate two tile ranges. */
                    int first=tile<28?4:36,local=(tile-first)%8;
                    int band=((tile-first)/8)*2+(first==36?1:0);
                    px=(local/2)*16+(sx%8)*2+xx;py=band*32+(local%2)*16+row*2+yy;
                                } else if(floor) {
                    int local=tile>=96?tile-96:tile-64;
                    /* End caps flank repeatable middle sections; lower tiles
                     * complete the same slab rather than retaining native circles. */
                    px=(local<8?local/2*16:16+(local-8)/2*16)+(sx%8)*2+xx;
                    py=(tile>=96?32:0)+(local%2)*16+row*2+yy;
                } else if(trim) {
                    int local=tile-60;px=16+(local/2)*16+(sx%8)*2+xx;py=(local%2)*16+row*2+yy;
                } else if(masonry) {px=(sx*2+xx)%256;py=(sy*2+yy)%256;}
                else if(!jade)px+=((index-8)/4)*32;
                                int tx=px*art->width/(masonry?256:jade?32:64),ty=py*art->height/(masonry?256:statue?192:floor?64:32);
                /* Use a stone face inside the source tile; the native CHR provides
                 * brick courses, so source brick joints must not repeat inside them. */
                if(jade) {tx=art->width*30/100+px*art->width*18/3200;ty=art->height*28/100+py*art->height*16/3200;}
                if(masonry) {tx=art->width/5+(px%64)*art->width*3/320;ty=art->height*3/10+(py%64)*art->height*7/1280;}
                const unsigned char *p=art->pixels+(ty*art->width+tx)*4;
                int material_fade=fade;
                if(jade)material_fade=fade*(value==3?135:value==2?220:256)/256;
                if(masonry)material_fade=fade*(value==1?208:value==2?150:240)/256;
                if(trim)material_fade=fade*(row==0?256:row==7?112:row>=5?176:224)/256;
                int r=p[0]*material_fade/normal,g=p[1]*material_fade/normal,b=p[2]*material_fade/normal;
                canvas[(y*2+yy)*512+x*2+xx]=0xff000000u|((uint32_t)r<<16)|((uint32_t)g<<8)|b;
            }
        }
    }
}
static void find_guardian_darts(void) {
    guardian_dart_count=0;
    if(!guardian_dart.pixels||!known_ground_art())return;
    static const uint32_t patterns[2]={0x6e90f7bdu,0x975e0361u};
    for(int t=0;t<2;t++)for(int k=0;k<16;k++)if(!chr_matches(((190+t)*16+k)-(k),patterns[t]))return;
    const uint8_t *oam=cyc_render_oam();
    for(int slot=0;slot<64&&guardian_dart_count<32;slot++) {
        const uint8_t *s=oam+slot*4;bool flip=s[2]&128;
        if(player_slots[slot]||s[0]>=239||s[1]!=(flip?190:191)||(s[2]&3)!=3)continue;
        int other=-1;
        for(int j=0;j<64;j++)if(!player_slots[j]&&oam[j*4]<239&&oam[j*4+3]==s[3]&&oam[j*4]==s[0]+8&&oam[j*4+1]==(flip?191:190)&&oam[j*4+2]==s[2]){other=j;break;}
        if(other<0&&s[0]+9<240)continue;
        HeroPoseMatch *m=&guardian_darts[guardian_dart_count++];memset(m,0,sizeof(*m));
        m->x=s[3];m->y=s[0]+1;m->flip=flip;m->pose=s[2];m->slots[slot]=true;player_slots[slot]=true;
        if(other>=0){m->slots[other]=true;player_slots[other]=true;}
    }
}
static void paint_guardian_darts(void) {
    for(int i=0;i<guardian_dart_count;i++) {
        const HeroPoseMatch *m=&guardian_darts[i];
        for(int y=0;y<32;y++)for(int x=0;x<16;x++) {
            int dx=m->x*2+x,dy=m->y*2+y;if(dx<0||dx>=512||dy<0||dy>=480)continue;
            if((m->pose&32)&&cyc_frame_bg_opaque()[(dy/2)*256+dx/2])continue;
            int px=(m->pose&64)?15-x:x,py=m->flip?31-y:y;
            const unsigned char *p=dart_filtered+(py*16+px)*4;
            if(p[3]<128||!artwork_priority_claim(artwork_owner,first_slot(m),dx,dy))continue;
            uint32_t light=presentation_color(29);int fade=(light>>16)&255;
            if(((light>>8)&255)>fade)fade=(light>>8)&255;if((light&255)>fade)fade=light&255;if(fade>236)fade=236;
            canvas[dy*512+dx]=0xff000000u|((uint32_t)(p[0]*fade/236)<<16)|((uint32_t)(p[1]*fade/236)<<8)|p[2]*fade/236;
        }
    }
}
static bool paint_danger_screen(void) {
    if(!background_art||!title_art.pixels||presentation_line_bg_table(120)!=0x1000||!(presentation_line_mask(120)&8))return false;
    static const uint8_t tiles[7]={141,138,151,144,142,155,169};
    static const uint32_t patterns[7]={0xba7e7a1du,0x3b29d2d7u,0x5bd5c322u,0xc5b5e0a4u,0x909ed2bau,0x271992ecu,0xb56682e1u};
    for(int i=0;i<7;i++) {
        if(presentation_nametable(0x2000+13*32+13+i)!=tiles[i])return false;
        for(int k=0;k<16;k++)if(!chr_matches((0x1000+tiles[i]*16+k)-(k),patterns[i]))return false;
    }
    const uint8_t *oam=cyc_render_oam();
    for(int slot=0;slot<64;slot++)if(oam[slot*4]<239)return false;
    int fade=0;
    for(int i=1;i<4;i++) {
        uint32_t light=presentation_color(i);
        if(((light>>16)&255)>fade)fade=(light>>16)&255;
        if(((light>>8)&255)>fade)fade=(light>>8)&255;if((light&255)>fade)fade=light&255;
    }
    for(int y=0;y<480;y++)for(int x=0;x<512;x++) {
        /* Crop the illustrated coast below the title logo. Native warning
         * text and timing are retained; only its presentation changes. */
        int tx=x*title_art.width/512,ty=title_art.height*45/100+y*title_art.height/960;
        const unsigned char *p=title_art.pixels+(ty*title_art.width+tx)*4;
        int dim=(y>=190&&y<=244)?8:4;
        int r=p[0]*fade/(255*dim),g=p[1]*fade/(255*dim),b=p[2]*fade/(255*dim);
        if((y==190||y==244)&&x>=170&&x<=342) {r=160*fade/255;g=118*fade/255;b=54*fade/255;}
        canvas[y*512+x]=0xff000000u|((uint32_t)r<<16)|((uint32_t)g<<8)|b;
    }
    return true;
}
static void paint_ui_text(void) {
    if(!background_art||!ui_font.pixels)return;
    static const uint32_t patterns[39]={0xae2c178cu,0x95284931u,0xc8dc2f03u,0xce3cb8fdu,0xdacddc35u,0x1161eda7u,0x9386c903u,0x57c1f07fu,0x8b977059u,0xa9e2c05au,0x3b29d2d7u,0x443600ddu,0xcf199ac4u,0xba7e7a1du,0x909ed2bau,0x678ad5c3u,0xc5b5e0a4u,0x0e33a3d2u,0xee023f01u,0x5f3ded4fu,0x18639714u,0x99ea5e58u,0xf82c3632u,0x5bd5c322u,0x26c0b581u,0xd5446f62u,0xc558816bu,0x271992ecu,0x78f2be6fu,0xb1d8fa5au,0x0ff18ceau,0xe0aab336u,0x2037b15eu,0x5b7f6c03u,0x542739a9u,0x2e82b895u,0xad1a180fu,0xac93fce5u,0xb56682e1u};
    static const char chars[]="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ,.!";
    int glyph[256];bool found=false;
    for(int t=0;t<256;t++) {
        glyph[t]=-1;
        for(int g=0;g<39;g++) {
            bool match=true;for(int k=0;k<16;k++)if(!chr_matches((0x1000+t*16+k)-(k),patterns[g])){match=false;break;}
            if(match){glyph[t]=chars[g]-32;found=true;break;}
        }
    }
    if(!found)return;
    for(int y=0;y<240;y++) {
        uint8_t mask=presentation_line_mask(y);if(!(mask&8)||presentation_line_bg_table(y)!=0x1000)continue;
        int sy=presentation_line_scroll_y(y)%480;
        for(int x=0;x<256;x++) {
            if(x<8&&!(mask&2))continue;
            bool occupied=sprite_guard[y*256+x]!=0;
            if(occupied)continue;
            int sx=(presentation_line_scroll_x(y)+x)%512,cx=sx%256/8,cy=sy%240/8;
            int nt=0x2000+(sx/256+sy/240*2)*0x400,t=presentation_nametable(nt+cy*32+cx),g=glyph[t];if(g<0)continue;
            uint32_t bg=presentation_color(0);int fade=0;
            int attr=presentation_nametable(nt+0x3c0+cy/4*8+cx/4),palette=(attr>>((cy&2)*2+(cx&2)))&3;
            for(int entry=1;entry<4;entry++) {
                uint32_t light=presentation_color(palette*4+entry);
                if(((light>>16)&255)>fade)fade=(light>>16)&255;
                if(((light>>8)&255)>fade)fade=(light>>8)&255;if((light&255)>fade)fade=light&255;
            }
            for(int yy=0;yy<2;yy++)for(int xx=0;xx<2;xx++) {
                int px=sx%8*2+xx,py=sy%8*2+yy;
                int alpha=ui_font.pixels[((g/16*16+py)*ui_font.width+g%16*16+px)*4+3];
                uint32_t base=warning_screen?canvas[(y*2+yy)*512+x*2+xx]:bg;
                int r=(((base>>16)&255)*(255-alpha)+245*fade*alpha/255)/255;
                int green=(((base>>8)&255)*(255-alpha)+218*fade*alpha/255)/255;
                int b=((base&255)*(255-alpha)+158*fade*alpha/255)/255;
                canvas[(y*2+yy)*512+x*2+xx]=0xff000000u|((uint32_t)r<<16)|((uint32_t)green<<8)|b;
            }
        }
    }
}
static bool hud_slots[64];
static void find_hud(void) {
    memset(hud_slots,0,sizeof(hud_slots));
    if(!ui_font.pixels||(!known_flight_art()&&!known_ground_art()))return;
    static const uint32_t glyphs[11]={0x0f0c016du,0x0c9df18du,0x06731430u,0x5f6a0cc8u,0x83fc2279u,0x92c1a0efu,0x25fafd95u,0x72f11aadu,0x5553cb88u,0x9f77a9f5u,0x133deb22u};
    const uint8_t *oam=cyc_render_oam();
    for(int slot=0;slot<64;slot++) {
        int tile=oam[slot*4+1];if(tile<96||tile>106||oam[slot*4]<208||oam[slot*4]>=239)continue;
        bool match=true;for(int k=0;k<16;k++)if(!chr_matches((tile*16+k)-(k),glyphs[tile-96]))match=false;
        if(match){hud_slots[slot]=true;player_slots[slot]=true;}
    }
}
static void paint_hud(void) {
    const uint8_t *oam=cyc_render_oam();
    for(int slot=0;slot<64;slot++)if(hud_slots[slot]) {
        int c=oam[slot*4+1]==106?'P':'0'+oam[slot*4+1]-96,glyph=c-32;
        int left=oam[slot*4+3]*2,top=(oam[slot*4]+1)*2;
        uint32_t light=presentation_color(16+(oam[slot*4+2]&3)*4+3);int fade=(light>>16)&255;
        if(((light>>8)&255)>fade)fade=(light>>8)&255;if((light&255)>fade)fade=light&255;
        /* Dark keyline keeps the counter readable over bright masonry. */
        for(int y=0;y<16;y++)for(int x=0;x<16;x++) {
            int alpha=ui_font.pixels[((glyph/16*16+y)*ui_font.width+glyph%16*16+x)*4+3];
            if(alpha<64)continue;
            for(int oy=-1;oy<=1;oy++)for(int ox=-1;ox<=1;ox++) {
                int dx=left+x+ox,dy=top+y+oy;
                if(artwork_priority_claim(artwork_owner,slot,dx,dy))canvas[dy*512+dx]=0xff000000u|((uint32_t)(16*fade/255)<<16)|((uint32_t)(18*fade/255)<<8)|26*fade/255;
            }
        }
        for(int y=0;y<16;y++)for(int x=0;x<16;x++) {
            int dx=left+x,dy=top+y;if(dx<0||dx>=512||dy<0||dy>=480)continue;
            int alpha=ui_font.pixels[((glyph/16*16+y)*ui_font.width+glyph%16*16+x)*4+3];
            if(!alpha||!artwork_priority_claim(artwork_owner,slot,dx,dy))continue;
            uint32_t old=canvas[dy*512+dx];
            int r=(((old>>16)&255)*(255-alpha)+245*fade*alpha/255)/255;
            int g=(((old>>8)&255)*(255-alpha)+218*fade*alpha/255)/255;
            int b=((old&255)*(255-alpha)+158*fade*alpha/255)/255;
            canvas[dy*512+dx]=0xff000000u|((uint32_t)r<<16)|((uint32_t)g<<8)|b;
        }
    }
}
static void title_text(const char *text,int y,int step,int fade) {
    if(!ui_font.pixels)return;
    int left=(512-(int)strlen(text)*step)/2;
    for(int i=0;text[i];i++) {
        int glyph=(unsigned char)text[i]-32;if(glyph<0||glyph>=95)continue;
        for(int yy=0;yy<16;yy++)for(int xx=0;xx<16;xx++) {
            int dx=left+i*step+xx,dy=y+yy;if(dx<0||dx>=512||dy<0||dy>=480)continue;
            int alpha=ui_font.pixels[((glyph/16*16+yy)*ui_font.width+glyph%16*16+xx)*4+3];
            if(!alpha)continue;
            uint32_t old=canvas[dy*512+dx];
            int r=(((old>>16)&255)*(255-alpha)+245*fade*alpha/255)/255;
            int g=(((old>>8)&255)*(255-alpha)+218*fade*alpha/255)/255;
            int b=((old&255)*(255-alpha)+158*fade*alpha/255)/255;
            canvas[dy*512+dx]=0xff000000u|((uint32_t)r<<16)|((uint32_t)g<<8)|b;
        }
    }
}
static bool paint_title(void) {
    if(!background_art||!title_art.pixels||!ui_font.pixels)return false;
    static const int ids[3]={0,129,153};
    static const uint32_t signature[3]={0xaec4ef8du,0x95284931u,0xd5446f62u};
    static const uint8_t logo[23]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,64,65,66,15,67,68,69};
    for(int i=0;i<3;i++)for(int k=0;k<16;k++)if(!chr_matches((0x1000+ids[i]*16+k)-(k),signature[i]))return false;
    for(int i=0;i<23;i++)if(presentation_nametable(0x2000+9*32+5+i)!=logo[i])return false;
    if(!(presentation_line_mask(120)&8))return false;
    uint32_t light=presentation_color(9);int fade=(light>>16)&255;
    if(((light>>8)&255)>fade)fade=(light>>8)&255;if((light&255)>fade)fade=light&255;
    for(int y=0;y<480;y++)for(int x=0;x<512;x++) {
        const unsigned char *p=title_art.pixels+((y*title_art.height/480)*title_art.width+x*title_art.width/512)*4;
        canvas[y*512+x]=0xff000000u|((uint32_t)(p[0]*fade/255)<<16)|((uint32_t)(p[1]*fade/255)<<8)|p[2]*fade/255;
    }
    title_text("1 PLAYER",252,16,fade);title_text("2 PLAYERS",286,16,fade);
    title_text("TM AND (C)1988 CAPCOM U.S.A., INC.",378,12,fade);
    title_text("LICENSED BY",408,12,fade);title_text("NINTENDO OF AMERICA INC.",430,12,fade);
    title_text("modded by retro-replay.com",456,12,fade);
    const uint8_t *oam=cyc_render_oam();
    for(int slot=0;slot<64;slot++)if(oam[slot*4+1]==42&&oam[slot*4+3]==80) {
        int y=oam[slot*4]<130?259:293;
        for(int yy=-5;yy<=5;yy++)for(int xx=0;xx<6-abs(yy);xx++)
            canvas[(y+yy)*512+166+xx]=0xff000000u|((uint32_t)(255*fade/255)<<16)|((uint32_t)(204*fade/255)<<8)|64*fade/255;
        break;
    }
    return true;
}
static void paint_late_arena(void) {
    if(!background_art||!arena_bricks.pixels)return;
    static const uint32_t expected[4]={0xf3e8cde0u,0x47a942b0u,0x97f4f0bau,0x118b278au};
    for(int t=0;t<4;t++)for(int k=0;k<16;k++)if(!chr_matches((0x1000+(128+t)*16+k)-(k),expected[t]))return;
    static const uint32_t body_expected[48]={0x7d923f7cu,0x7d923f7cu,0x7d923f7cu,0x7d923f7cu,0xe3b399c5u,0x6aa9e488u,0xe6644647u,0xb486553bu,0x02602184u,0x4fb5c1dau,0xa616dc99u,0xff2c28c2u,0xc56d7cdcu,0xb5036485u,0xd807060du,0xf329c148u,0x6217c213u,0xb51659ccu,0x0cecc585u,0x944ba8f6u,0xcc2923a5u,0x2783fb46u,0x13b24472u,0xb1a1e018u,0x1c0c66f9u,0x1289ab21u,0xbda9506du,0x5b3dcb2du,0xf6e0c165u,0x96642273u,0x962e2485u,0x0b33fdcdu,0x4ac96d5du,0x73fecf05u,0x35b174f3u,0xaea9e151u,0x370b1d1bu,0x75b0e90du,0xee68caaau,0xbd05d1eau,0xc078edf8u,0x95c5175au,0x9f9fdb56u,0x83a50754u,0xc298467cu,0xc5230a45u,0x11683c54u,0x96031644u};
    bool body_valid[48];for(int t=0;t<48;t++){body_valid[t]=true;for(int k=0;k<16;k++)if(!chr_matches((0x1000+(192+t)*16+k)-(k),body_expected[t]))body_valid[t]=false;}
    static const uint32_t trim_expected[68]={0xb91c2549u,0x18bcbe4bu,0xbbc27ff3u,0xb3a85e31u,0x0b904ddfu,0xbe08ce33u,0x0f1171d3u,0xce4cabd1u,0xdafe1a0au,0x022590f4u,0x93713a45u,0x9a29cda5u,0xd8a24264u,0x8f4ace98u,0x81e87365u,0xbedb4e05u,0x086052ddu,0xd07461f0u,0x937babd0u,0xdd06211bu,0xb35843adu,0xcba39502u,0x68878261u,0x23ae7231u,0xb35843adu,0x29b4f73cu,0x68878261u,0xa9fceaa5u,0xb6dcfa5du,0x904ceef1u,0x6c70cf4cu,0xea8a292bu,0x0d75db58u,0x8f8c7b98u,0xbe5c694bu,0x2b76c8bbu,0x5a97b98cu,0xbb935a97u,0x04b29b69u,0x89eea659u,0x1a8d234cu,0x6072b7e4u,0xa2c8b54fu,0x6603babbu,0x7d923f7cu,0xa73a172fu,0x7d923f7cu,0x13fdf8bcu,0x7d923f7cu,0x01faaf76u,0x7d923f7cu,0xab8209b2u,0x7d923f7cu,0x340e4c68u,0x7d923f7cu,0x340e4c68u,0x48b0a436u,0x7d923f7cu,0x73edb6f8u,0x7d923f7cu,0x7d923f7cu,0x7d923f7cu,0x7d923f7cu,0x7d923f7cu,0xe3b399c5u,0x6aa9e488u,0xe6644647u,0xb486553bu};
    bool trim_valid[68];for(int t=0;t<68;t++){trim_valid[t]=true;for(int k=0;k<16;k++)if(!chr_matches((0x1000+(132+t)*16+k)-(k),trim_expected[t]))trim_valid[t]=false;}
    int fade=0;for(int i=1;i<16;i++){uint32_t c=presentation_color(i);int r=c>>16&255,g=c>>8&255,b=c&255;if(r>fade)fade=r;if(g>fade)fade=g;if(b>fade)fade=b;}if(fade>236)fade=236;
    for(int y=0;y<240;y++) {
        int mask=presentation_line_mask(y);if(!(mask&8)||presentation_line_bg_table(y)!=0x1000)continue;
        int sy=presentation_line_scroll_y(y)%480;
        for(int x=0;x<256;x++) {
            if(sprite_guard[y*256+x]||(x<8&&!(mask&2)))continue;
            int sx=(presentation_line_scroll_x(y)+x)%512;
            int nt=0x2000+(sx/256+sy/240*2)*0x400+(sy%240/8)*32+sx%256/8;
            int tile=presentation_nametable(nt);
            int attr=presentation_nametable((nt&~0x3ff)+0x3c0+(sy%240/8)/4*8+(sx%256/8)/4);
            int palette=(attr>>(((sy%240/8)&2)*2+((sx%256/8)&2)))&3;
            bool body=late_guardian_art.pixels&&tile>=192&&tile<240&&body_valid[tile-192]&&palette==1;
            bool trim=tile>=132&&tile<200&&trim_valid[tile-132]&&palette==0;
            if(!body&&!trim&&(tile<128||tile>131))continue;
            for(int yy=0;yy<2;yy++)for(int xx=0;xx<2;xx++) {
                int tx=(sx*2+xx)%128*arena_bricks.width/128,ty=(sy*2+yy)%128*arena_bricks.height/128;
                const unsigned char *p=arena_bricks.pixels+(ty*arena_bricks.width+tx)*4;
                int r=p[0],g=p[1],b=p[2];
                if(trim) {
                    if(tile>=132&&tile<148&&arena_pilaster.pixels) {
                        int part=(tile-132)%4,px=part/2*16+sx%8*2+xx;
                        int py=(tile<140?32:0)+part%2*16+sy%8*2+yy;
                        int tx=px*arena_pilaster.width/32,ty=py*arena_pilaster.height/64;
                        const unsigned char *q=arena_bricks.pixels+(ty*arena_bricks.width+tx)*4;
                        r=q[0];g=q[1];b=q[2];
                    } else if(tile>=152&&tile<=159) {
                        r=r*3/4;g=g*3/4;b=b*3/4;
                    } else {
                        int bit=7-sx%8,row=sy%8;
                        int lo=presentation_chr(0x1000+tile*16+row),hi=presentation_chr(0x1000+tile*16+row+8);
                        int value=(lo>>bit&1)|((hi>>bit&1)<<1);
                        const unsigned char *q=flight_sandstone.pixels+(((sy*2+yy)%256)*flight_sandstone.height/256*flight_sandstone.width+((sx*2+xx)%256)*flight_sandstone.width/256)*4;
                        int shade=value==0?40:value==1?245:value==2?190:120;
                        r=q[0]*shade/256;g=q[1]*shade/256;b=q[2]*shade/256;
                    }
                }
                if(body) {
                    int local=tile>=228?tile-228:tile>=216?tile-216:tile-192;
                    int col=(local%12)/2,row=tile>=228?local%2:tile>=216?2+local%2:4+local%2;
                    int px=col*16+(sx%8)*2+xx,py=row*16+(sy%8)*2+yy;
                    /* The destructible head is native OAM, not the permanent wall.
                       Match its screen-space body before choosing live artwork. */
                    int left=x-px/2,top=y-py/2;
                    bool alive=false;const uint8_t *o=cyc_render_oam();
                    for(int slot=0;slot<64;slot++) {
                        const uint8_t *part=o+slot*4;
                        if(part[0]<239&&part[1]>=55&&part[1]<=60&&(part[2]&3)==3&&
                           part[3]>=left+12&&part[3]<left+36&&part[0]+1>=top+12&&part[0]+1<top+36){alive=true;break;}
                    }
                    int pose=alive?(cyc_cycle_count()/2144232u)%2u:0;
                    const unsigned char *q=late_guardian_filtered[pose]+(py*96+px)*4;
                    /* Leave the neck stump and shoulders; the severed crown never regrows. */
                    bool missing_head=!alive&&px>=32&&px<64&&py<32;
                    if(!missing_head) {
                        r=(r*(255-q[3])+q[0]*q[3])/255;g=(g*(255-q[3])+q[1]*q[3])/255;b=(b*(255-q[3])+q[2]*q[3])/255;
                    }
                }
                canvas[(y*2+yy)*512+x*2+xx]=0xff000000u|((uint32_t)(r*fade/236)<<16)|((uint32_t)(g*fade/236)<<8)|b*fade/236;
            }
        }
    }
}
static void find_late_guardians(void) {
    late_guardian_count=0;if(!late_guardian_art.pixels)return;
    static const uint32_t expected[6]={0x731c8a0du,0x4395dcfdu,0x9aca9543u,0x7536205cu,0xc216c30au,0x08cb0841u};
    for(int t=0;t<6;t++)for(int k=0;k<16;k++)if(!chr_matches(((55+t)*16+k)-(k),expected[t]))return;
    const uint8_t *o=cyc_render_oam();
    for(int seed=0;seed<64&&late_guardian_count<12;seed++) {
        const uint8_t *q=o+seed*4;
        if(player_slots[seed]||q[0]>=239||q[1]!=55||(q[2]&3)!=3)continue;
        bool flip=(q[2]&64)!=0;int left=q[3]-(flip?8:0),top=q[0]+1;
        HeroPoseMatch m;memset(&m,0,sizeof(m));m.x=left;m.y=top;m.flip=flip;
        m.pose=(cyc_cycle_count()/2144232u)%2u;
        int count=0;
        for(int t=0;t<6;t++) {
            int x=left+((t%2)^(flip?1:0))*8,y=top+t/2*8;
            for(int slot=0;slot<64;slot++)if(!player_slots[slot]&&o[slot*4]<239&&o[slot*4+1]==55+t&&o[slot*4+3]==x&&o[slot*4]+1==y&&o[slot*4+2]==q[2]){m.slots[slot]=true;count++;break;}
        }
        /* A complete upper pair plus any lower row permits native edge clipping. */
        if(count<4)continue;
        late_guardians[late_guardian_count++]=m;
        for(int slot=0;slot<64;slot++)if(m.slots[slot])player_slots[slot]=true;
    }
}
static void find_dragon_boss(void) {
    dragon_count=0;if(!dragon_art.pixels)return;
    static const uint32_t expected[60]={0x9e890631u,0x6070979du,0x08498d95u,0xaa4f2bf3u,0x32443095u,0xa049c26du,0x89cb97c0u,0xecdcc3bdu,0x4f16e3d5u,0x1d9dff60u,0x91bf27b3u,0xd94208fdu,0xb704a9b7u,0xf4e95ee0u,0x53c818cau,0x44b9f69fu,0x34a92a3eu,0x853817efu,0x8f2adc32u,0x3ccbdb65u,0x23a5e09bu,0x529a2f53u,0x6491a84du,0xe524716bu,0x440273a7u,0x5dd32f60u,0x19fee385u,0x42b9f2cdu,0x856472d5u,0xc79548bcu,0x25f2ce66u,0x7694fb24u,0x176de742u,0xa8032a9fu,0xdfd26229u,0xed61bf43u,0x268ce99au,0xb4a4f27cu,0x46556abdu,0x7fc29e11u,0x0c6074d3u,0xa756f55eu,0x19745e41u,0x6ff914b2u,0x6e43580du,0x42e7e474u,0x9046f1d2u,0x88ac8c05u,0x17754ea9u,0xd89324ddu,0xedcfaa00u,0xeeadf7cdu,0x88aefdedu,0x64cfe58du,0xcb861444u,0x0baab81fu,0x02230aecu,0xa9c28a69u,0x59ddcb24u,0x27a47cbdu};
    for(int t=0;t<60;t++)for(int k=0;k<16;k++)if(!chr_matches(((128+t)*16+k)-(k),expected[t]))return;
    const uint8_t *o=cyc_render_oam();
    for(int seed=0;seed<64&&dragon_count<2;seed++) {
        const uint8_t *q=o+seed*4;
        if(player_slots[seed]||q[0]>=239||q[1]!=132||(q[2]&3)!=3)continue;
        HeroPoseMatch m;memset(&m,0,sizeof(m));m.x=q[3]-16;m.y=q[0]+1-16;
        int count=0;bool folded=false,spread=false;
        for(int slot=0;slot<64;slot++) {
            const uint8_t *v=o+slot*4;
            if(player_slots[slot]||v[0]>=239||v[1]<128||v[1]>187||(v[2]&3)!=3)continue;
            if(v[3]<m.x||v[3]>=m.x+48||v[0]+1<m.y||v[0]+1>=m.y+72)continue;
            m.slots[slot]=true;count++;folded|=v[1]>=171&&v[1]<=178;spread|=v[1]>=179;
        }
        if(count<12)continue;
        m.pose=folded?0:spread?1:2;dragon_matches[dragon_count++]=m;
        for(int slot=0;slot<64;slot++)if(m.slots[slot])player_slots[slot]=true;
    }
}
static bool lion_boss_scene(void) {
    if(!ground_scene||!boss_cannon_art.pixels)return false;
    static const uint32_t expected[32]={0x04dda6b9u,0x7388dd3au,0x9e030f7eu,0x439fe629u,0x5a6367eau,0xb83b3e85u,0xa138c87au,0xb780052bu,0x41edb0b6u,0xe36308dfu,0x9e0d4a68u,0x727dff35u,0xe266bed9u,0x91090b4cu,0x45559ccbu,0x78d6a3b1u,0x858727ebu,0x77a179fdu,0xf002baabu,0xc4371140u,0x011dc19bu,0xde6a0828u,0xfa572b47u,0x858727ebu,0xf21c8753u,0x5cd3e7abu,0xec1732f1u,0x3c0e654bu,0x93be27fdu,0xd575542bu,0x38ece4ebu,0x9394ae01u};
    for(int t=0;t<32;t++)for(int k=0;k<16;k++)if(!chr_matches(((128+t)*16+k)-(k),expected[t]))return false;
    return true;
}
static int lion_boss_fade(void) {
    int fade=0;for(int i=0;i<32;i++) {
        uint32_t c=presentation_color(i);int r=(c>>16)&255,g=(c>>8)&255,b=c&255;
        if(r>fade)fade=r;if(g>fade)fade=g;if(b>fade)fade=b;
    }
    return fade>236?236:fade;
}
static void find_lion_bolts(void) {
    lion_bolt_count=0;if(!lion_boss_scene())return;
    const uint8_t *o=cyc_render_oam();
    for(int slot=0;slot<64;slot++) {
        const uint8_t *q=o+slot*4;if(player_slots[slot]||q[0]>=207||q[1]!=168||(q[2]&3)!=0)continue;
        HeroPoseMatch m;memset(&m,0,sizeof(m));m.x=q[3];m.y=q[0]+1;m.slots[slot]=true;
        lion_bolts[lion_bolt_count++]=m;player_slots[slot]=true;
    }
}
static void find_lion_bosses(void) {
    lion_boss_count=0;if(!lion_boss_scene())return;
    const uint8_t *o=cyc_render_oam();
    for(int seed=0;seed<64&&lion_boss_count<8;seed++) {
        const uint8_t *q=o+seed*4;
        if(player_slots[seed]||q[0]>=207||(q[2]&3)!=3)continue;
        int t=q[1];bool large=t>=132&&t<=135,small=t==128||t==141;
        if(!large&&!small)continue;
        HeroPoseMatch m;memset(&m,0,sizeof(m));m.x=q[3]-(large?(t-132)*8:0);m.y=q[0]+1-(large&&(q[2]&128)?24:0);m.palette=large?64:32;m.pose=small?(t==141):1;
        int size=large?32:16,count=0;
        for(int slot=0;slot<64;slot++) {
            const uint8_t *v=o+slot*4;
            if(player_slots[slot]||v[0]>=207||(v[2]&3)!=3||v[3]<m.x||v[3]>=m.x+size||v[0]+1<m.y||v[0]+1>=m.y+size)continue;
            if(large?(v[1]<132||v[1]>159):(v[1]!=t&&v[1]!=t+1))continue;
            m.slots[slot]=true;count++;
        }
        if(count<2)continue;lion_bosses[lion_boss_count++]=m;
        for(int slot=0;slot<64;slot++)if(m.slots[slot])player_slots[slot]=true;
    }
}
static void paint_lion_wall(void) {
    if(!background_art||!lion_boss_scene()||!mechanical_boss.pixels)return;
    static const uint32_t expected[256]={0x69691905u,0x69691905u,0x69691905u,0x69691905u,0xbfddc99cu,0xa1f413bdu,0xc1d9099au,0xeeddde0eu,0xd5d1ededu,0xa966f0f5u,0x8a9f3b17u,0x5ba85feeu,0x13c79b87u,0x3854fe01u,0x9b363b42u,0x3bc8f0d7u,0x6890117bu,0x2c8d5a68u,0x28f18571u,0x9bc67587u,0x94c45515u,0x39b47825u,0xfce375ddu,0xc449e01du,0x6c3dededu,0x317f12e5u,0x599c2595u,0x4d782180u,0xcbd1fa2fu,0x10d6b49du,0xfaccc571u,0x40d7c253u,0x69691905u,0xacb8bf45u,0x69691905u,0x6f8f09feu,0x8f959864u,0xa1b7742bu,0x8c630392u,0xb771ae5bu,0x1ce241bau,0x49e59d0du,0x96a4d22du,0xc3959380u,0xdbb029e5u,0xe2521405u,0xba314620u,0x2311e75fu,0xdefb1a14u,0x06a21234u,0xcd8ca097u,0xe35167e3u,0xffd63645u,0x7f8675a5u,0x3a1b4169u,0x055a4295u,0xa7eb94a3u,0x3ca1626au,0x5482f47fu,0x930fe629u,0x49d0e408u,0x35e575f4u,0xc67ca40eu,0x787dc392u,0x901bee43u,0xc38567a4u,0xe0f85e5cu,0xecc834ddu,0xe4b26a6du,0x839129c6u,0x0c87cc67u,0x7ae059ddu,0x01bb9764u,0x6debb9fdu,0x01bb9764u,0x6debb9fdu,0x1ed62aacu,0x98fca06fu,0x0b2a8665u,0x60239d97u,0xd7c8ae40u,0x17a05cb9u,0xc86afa3eu,0x3c86693bu,0xe064d1a3u,0x682ef20fu,0x02d0cc9du,0x61abc8d0u,0xced27e0du,0x3f68a2abu,0x39ba715bu,0xb80f7547u,0xc44cd45du,0xc44cd45du,0x83008b85u,0x83008b85u,0x007b6472u,0x2590da9du,0x34495635u,0x91e39a71u,0x363dfb24u,0x92e24a4eu,0x929fa01du,0x3453dbf4u,0x6debb9fdu,0x0282def9u,0x6debb9fdu,0x0282def9u,0x8db4fde4u,0x18c8a5adu,0xe94d1064u,0xb19eb04bu,0x8c87029du,0x3ccbaa9cu,0xb72688f9u,0x47564b51u,0x0b49a0bau,0xfc6574ebu,0x3a9da5dbu,0xb80f7547u,0x3796c84bu,0x4383754au,0x8e89d201u,0xca5a86abu,0x916f2a78u,0x9632beaeu,0x45e7683du,0x1c63e2c5u,0x7617cf7du,0x43a3d6a1u,0x068db6bdu,0x69e0a6c7u,0x068db6bdu,0x4a99b7a8u,0xee885502u,0x8b92ddbbu,0x313a0c6au,0x65226a10u,0x62e46143u,0x83d5d40au,0xe54c95c5u,0xe54c95c5u,0xe54c95c5u,0xe54c95c5u,0xb14482e1u,0xae0d3bbcu,0xf8bacc4eu,0xf2634011u,0x9daea403u,0x9a472095u,0x3ae79209u,0xbce54a6cu,0xfa246865u,0x5f3a75b3u,0x1c8a43edu,0xedc1bc5du,0xc8d8ced5u,0xf920792cu,0x40deb9a1u,0xb337ccd9u,0x5a21a912u,0x941deec3u,0x66dee10du,0x669c12a5u,0x9858cac5u,0x303760b1u,0xfbddac25u,0xf47ac345u,0x69691905u,0x01074225u,0x69691905u,0x850257c5u,0xa6b48aa7u,0x3bd19415u,0xd2e816adu,0x32406d8eu,0x6b4f5301u,0xaa16e3b9u,0x4d49b821u,0x9c2e88e5u,0x22496d56u,0x22be1cd6u,0x2ab1d545u,0xc12c17c5u,0xeb0a4acdu,0xeb0a4acdu,0x3c576945u,0x3c576945u,0x573f16e1u,0xf6b14dd2u,0xe0864b25u,0x520cdb25u,0x1afa7095u,0x60668a85u,0xe9e1c505u,0xa4615805u,0xc16b8842u,0xbf53d849u,0xe0eaa5f1u,0x57a27029u,0xc470b09au,0x54b256f1u,0x3c7cf5abu,0x8aee6dfau,0x1d37e3f8u,0xdd701b46u,0x1d37e3f8u,0xdd701b46u,0xf350685cu,0x07a897f6u,0x7e2452c1u,0x1389f003u,0x4e10bfd6u,0x69fd2875u,0x8768a57cu,0x1689e335u,0x4a0c23c7u,0x79c70920u,0xe5576df9u,0x50574d8du,0x4116df8au,0x5a44d88fu,0x8aa9d6f6u,0xf251be99u,0xf25c60c2u,0xa80a8f53u,0x9d1a0785u,0xcd88bc45u,0xf8eba9dau,0x27217900u,0xae202138u,0x11b77076u,0xb1768fd0u,0xfbf7c4c2u,0xaaa4c026u,0x94d93b17u,0x4d746e6fu,0x8f5ece9fu,0xf1bf8d95u,0x975cb77du,0x41396365u,0x5f233201u,0x607ac7d0u,0xb1450a04u,0x8780c67eu,0x6d0bf219u,0x19924b45u,0x2e80369du,0x391c66ddu,0x8bd8dee1u,0x6bbc9c87u,0x71c6db7du,0xcecd5bfdu,0x9ad862d4u,0xf86cea21u,0xb001dc1cu};
    for(int t=212;t<=244;t+=16)for(int k=0;k<16;k++)if(!chr_matches((0x1000+t*16+k)-(k),expected[t]))return;
    bool valid[256];for(int t=0;t<256;t++) {
        valid[t]=true;for(int k=0;k<16;k++)if(!chr_matches((0x1000+t*16+k)-(k),expected[t]))valid[t]=false;
    }
    int fade=lion_boss_fade(),root_x=0,root_y=0;bool boss_visible=false;
    for(int i=0;i<lion_boss_count;i++)if(lion_bosses[i].palette==64){root_x=lion_bosses[i].x-26;root_y=lion_bosses[i].y-81;boss_visible=true;break;}
    /* Body follows the actual streamed map; moving grille and eyelids follow
     * OAM. The core starts at native 172,174,224 with 225 beneath it. */
    for(int row=1;row<29;row++)for(int col=0;col<64;col++) {
        int nt=0x2000+(col/32)*0x400+row*32+col%32;
        int c1=(col+63)%64,c2=(col+62)%64;
        int l1=0x2000+c1/32*0x400+row*32+c1%32,l2=0x2000+c2/32*0x400+row*32+c2%32;
        if(presentation_nametable(nt)!=224||presentation_nametable(l1)!=174||presentation_nametable(l2)!=172||
           presentation_nametable(nt+32)!=225||presentation_nametable(nt-32)==225)continue;
        int rx=(col*8-presentation_line_scroll_x(0)+512)%512-59;
        if(rx>384)rx-=512;
        int ry=(row*8-presentation_line_scroll_y(0)%240+240)%240-80,support=0;
        /* Stale or preloaded boss rows must not paint over a temple corridor. */
        for(int yy=ry<0?0:ry;yy<ry+192&&yy<208;yy+=8)for(int xx=rx<0?0:rx;xx<rx+128&&xx<256;xx+=8) {
            int vx=(xx+presentation_line_scroll_x(yy))%512,vy=presentation_line_scroll_y(yy)%240;
            int a=0x2000+vx/256*0x400+vy/8*32+vx%256/8,t=presentation_nametable(a);
            if(t>=128&&valid[t])support++;
        }
        if(support>=3){root_x=rx;root_y=ry;boss_visible=true;}
    }
    bool eye_open[2]={false,false};
    for(int i=0;i<lion_boss_count;i++)if(lion_bosses[i].palette==32&&lion_bosses[i].pose==1)
        eye_open[lion_bosses[i].y-root_y<100?0:1]=true;

    for(int y=0;y<208;y++) {
        int sy=presentation_line_scroll_y(y)%480;if(!(presentation_line_mask(y)&8)||presentation_line_bg_table(y)!=0x1000)continue;
        for(int x=0;x<256;x++) {
            if(sprite_guard[y*256+x]||(x<8&&!(presentation_line_mask(y)&2)))continue;
            int sx=(presentation_line_scroll_x(y)+x)%512;
            int nt=0x2000+(sx/256+sy/240*2)*0x400+sy%240/8*32+sx%256/8;
            int t=presentation_nametable(nt),px=x-root_x,py=y-root_y;
            bool body=boss_visible&&px>=0&&px<128&&py>=0&&py<192;
            bool old_body=t>=128&&valid[t];
            /* Temple motifs have already been drawn in their native footprints. */
            if(!body&&!old_body)continue;
            for(int yy=0;yy<2;yy++)for(int xx=0;xx<2;xx++) {
                if(body||old_body) {
                    uint32_t base=horizontal_atmosphere(presentation_color(0),x,y);
                    canvas[(y*2+yy)*512+x*2+xx]=base;
                    if(!body)continue;
                    int tx=(px*2+xx)*mechanical_boss.width/256;
                    /* Align upper eye, grille and lower eye with native centers. */
                    int yp=py*2+yy,ty;
                    if(yp<82)ty=20+yp*358/82;
                    else if(yp<194)ty=378+(yp-82)*402/112;
                    else if(yp<274)ty=780+(yp-194)*365/80;
                    else ty=1145+(yp-274)*365/110;
                    ty=ty*mechanical_boss.height/1536;
                    HeroArt *body_art=mechanical_body.pixels?&mechanical_body:&mechanical_boss;
                    int eye=py>=25&&py<58?0:py>=121&&py<154?1:-1;
                    if(eye>=0&&px>=82&&px<114&&!eye_open[eye]&&mechanical_closed.pixels)body_art=&mechanical_closed;
                    int bx=tx*body_art->width/mechanical_boss.width,by=ty*body_art->height/mechanical_boss.height;
                    const unsigned char *q=body_art->pixels+(by*body_art->width+bx)*4;
                    if(q[3]<192)continue; /* Crisp silhouette; omit generated glow. */
                    canvas[(y*2+yy)*512+x*2+xx]=0xff000000u|((uint32_t)(q[0]*fade/236)<<16)|((uint32_t)(q[1]*fade/236)<<8)|q[2]*fade/236;
                }
            }
        }
    }
}
static void frame_begin(void *ctx) {
    (void)ctx;static bool held,background_held;
    if(debug_no_damage&&(known_flight_art()||known_ground_art())) {
        /* Native post-respawn collision grace ($5A/$5B), indexed by player.
           Keep grace below the death-animation range and disable visual blink.
           Normal controls and weapons remain active. */
        for(int i=0;i<2;i++) {
            int phase=cyc_mod_peek(0x78+i)&127;
            /* Entry and upgrade transitions must finish their native countdown. */
            if(phase!=5&&phase!=6){cyc_mod_poke(0x5a+i,2);cyc_mod_poke(0x56+i,0);}
        }
    }
    /* Opt-in artwork capture aid. $58 was verified as the player-one life
     * counter in both flight perspectives; the indexed decrement at $B19A
     * was inspected for the horizontal scene.
     * Never enable this implicitly or apply it to an unrecognized scene. */
    if(capture_infinite_lives&&(known_flight_art()||known_ground_art())) {
        uint8_t lives=cyc_mod_peek(0x58);
        if(lives>0&&lives<3)cyc_mod_poke(0x58,3);
    }
    if(!(SDL_WasInit(SDL_INIT_VIDEO)&SDL_INIT_VIDEO))return;
    const uint8_t *keys=SDL_GetKeyboardState(NULL);
    bool now=keys[SDL_SCANCODE_F6]!=0;
    if(now&&!held)enhanced=!enhanced;
    held=now;
    bool background_now=keys[SDL_SCANCODE_F7]!=0;
    if(background_now&&!background_held)background_art=!background_art;
    background_held=background_now;
    bool water_now=keys[SDL_SCANCODE_F5]!=0;
    if(water_now&&!water_held)water_motion=!water_motion;
    water_held=water_now;
}
#include "bonus-presentation.inc"
static const uint32_t *present(void *ctx,int *width,int *height) {
    (void)ctx;const uint32_t *src=cyc_frame_argb();
    if(!enhanced){*width=256;*height=240;return src;}
    snapshot_presentation();
    load_asset();bool flight=known_flight_art(),replace=find_player();find_bonus_treasures();find_flying_masks();find_orbs();find_bolts();find_wyverns();find_pods();find_pellets();find_guardians();
    find_greens();find_spring_jumpers();
    find_serpents();
    find_bursts();
    find_cycling();
    find_flashes();
    find_swirls();
    find_halos();
    find_chambers();
    find_extra_effects();
    find_turrets();
    find_missiles();
    find_hud();
    find_guardian_darts();
    find_late_guardians();
    find_dragon_boss();
    find_lion_bosses();
    find_lion_bolts();
    memcpy(clean,src,sizeof(clean));
    if(lion_bolt_count||lion_boss_count||green_count||dragon_count||late_guardian_count||replace||bonus_count||mask_count||orb_count||bolt_count||wyvern_count||pod_count||pellet_count||guardian_count||burst_count||cycling_count||flash_count||swirl_count||halo_count||chamber_count||turret_count||missile_count||ground_scene) {
        memset(sprites,0,sizeof(sprites));
        cyc_render_sprites(sprites,256,240,0,cyc_frame_bg_opaque(),keep_sprite,NULL);
        const uint8_t *oam=cyc_render_oam();
        for(int slot=0;slot<64;slot++)if(player_slots[slot])
        for(int y=oam[slot*4]+1;y<oam[slot*4]+9;y++)for(int x=oam[slot*4+3];x<oam[slot*4+3]+8;x++) {
            if(x<0||x>=256||y<0||y>=240)continue;
            clean[y*256+x]=sprites[y*256+x]?sprites[y*256+x]:background_pixel(x,y);
        }
    }
    *width=512;*height=480;
    for(int y=0;y<240;y++)for(int x=0;x<256;x++) {
        uint32_t e=clean[y*256+x],b=clean[(y?y-1:y)*256+x],d=clean[y*256+(x?x-1:x)];
        uint32_t f=clean[y*256+(x<255?x+1:x)],h=clean[(y<239?y+1:y)*256+x];
        uint32_t v[4]={e,e,e,e};
        if(b!=h&&d!=f){v[0]=d==b?d:e;v[1]=b==f?f:e;v[2]=d==h?d:e;v[3]=h==f?f:e;}
        int at=y*1024+x*2;
        for(int k=0;k<4;k++)canvas[at+(k/2)*512+k%2]=flight?atmosphere(v[k],x,y):
            ground_scene&&!sprites[y*256+x]?horizontal_atmosphere(v[k],x,y):v[k];
    }
    prepare_sprite_guard();
    paint_cavern_background();
    paint_temple_background();
    paint_bonus_background();
    paint_flight_terrain();
    paint_flight_gateway();
    paint_late_arena();
    paint_lion_wall();
    warning_screen=paint_danger_screen();
    paint_ui_text();
    artwork_priority_prepare(artwork_owner,player_slots);
    int lion_fade=(lion_boss_count||lion_bolt_count)?lion_boss_fade():236;
    for(int i=0;i<lion_boss_count;i++) {
        const HeroPoseMatch *m=&lion_bosses[i];int size=m->palette,fade=lion_fade;
        if(mechanical_boss.pixels&&size==32)continue; /* Eyelids are body-mounted. */
        for(int y=0;y<size;y++)for(int x=0;x<size;x++) {
            int dx=m->x*2+x,dy=m->y*2+y;if(dx<0||dx>=512||dy<0||dy>=416)continue;
            const unsigned char *q=(size==64?boss_cannon_pixels[m->pose]:boss_cannon_small[m->pose])+(y*size+x)*4;
            if(mechanical_boss.pixels) {
                int ax=size==64?225:694,ay=size==64?650:295,aw=size==64?265:174,ah=size==64?260:174;
                int tx=(ax+x*aw/size)*mechanical_boss.width/1024,ty=(ay+y*ah/size)*mechanical_boss.height/1536;
                q=mechanical_boss.pixels+(ty*mechanical_boss.width+tx)*4;
            }
            if(q[3]<192||!artwork_priority_claim(artwork_owner,first_slot(m),dx,dy))continue;
            uint32_t old=canvas[dy*512+dx];int a=q[3];
            unsigned r=(q[0]*fade/236*a+((old>>16)&255)*(255-a))/255,g=(q[1]*fade/236*a+((old>>8)&255)*(255-a))/255,b=(q[2]*fade/236*a+(old&255)*(255-a))/255;
            canvas[dy*512+dx]=0xff000000u|(r<<16)|(g<<8)|b;
        }
    }

    for(int i=0;i<lion_bolt_count;i++)for(int y=0;y<16;y++)for(int x=0;x<16;x++) {
        const HeroPoseMatch *m=&lion_bolts[i];int dx=m->x*2+x,dy=m->y*2+y;
        if(dx<0||dx>=512||dy<0||dy>=416)continue;
        int px=x*2-15,py=y*2-15,d=px*px+py*py;if(d>225)continue;
        if(!artwork_priority_claim(artwork_owner,first_slot(m),dx,dy))continue;
        int a=d>160?(225-d)*255/65:255,fade=lion_fade;
        int r=d<45?255:195,g=d<45?230:d<110?90:35,b=255;
        uint32_t old=canvas[dy*512+dx];r=(r*fade/236*a+((old>>16)&255)*(255-a))/255;
        g=(g*fade/236*a+((old>>8)&255)*(255-a))/255;b=(b*fade/236*a+(old&255)*(255-a))/255;
        canvas[dy*512+dx]=0xff000000u|((uint32_t)r<<16)|((uint32_t)g<<8)|b;
    }
    for(int index=0;index<dragon_count;index++)for(int y=0;y<144;y++)for(int x=0;x<160;x++) {
        const HeroPoseMatch *m=&dragon_matches[index];
        int dx=m->x*2-32+x,dy=m->y*2+y;
        if(dx<0||dx>=512||dy<0||dy>=480)continue;
        const unsigned char *q=dragon_filtered[m->pose]+(y*160+x)*4;
        if(!q[3]||!artwork_priority_claim(artwork_owner,first_slot(m),dx,dy))continue;
        uint32_t old=canvas[dy*512+dx];int a=q[3];
        unsigned r=(q[0]*a+((old>>16)&255)*(255-a))/255;
        unsigned g=(q[1]*a+((old>>8)&255)*(255-a))/255;
        unsigned b=(q[2]*a+(old&255)*(255-a))/255;
        canvas[dy*512+dx]=0xff000000u|(r<<16)|(g<<8)|b;
    }

    static const int burst_cells[9]={0,1,2,3,2,3,4,5,6};
    for(int index=0;index<burst_count;index++)for(int y=0;y<32;y++)for(int x=0;x<32;x++) {
        const HeroPoseMatch *match=&bursts[index];int size=match->pose==0||match->pose>=7?16:32;
        if(x>=size||y>=size)continue;
        int dx=match->x*2+x,dy=match->y*2+y;
        if(dx<0||dx>=512||dy<0||dy>=416)continue;
        int px=x,py=y;
        if(match->pose==3){px=31-y;py=x;}
        if(match->pose==4){px=31-x;py=31-y;}
        if(match->pose==5){px=y;py=31-x;}
        const int *bounds=burst_bounds[burst_cells[match->pose]];
        int sx=bounds[0]+px*(bounds[2]-bounds[0])/size,sy=bounds[1]+py*(bounds[3]-bounds[1])/size;
        const unsigned char *p=burst.pixels+(sy*burst.width+sx)*4;
        if(p[3]<128||!artwork_priority_claim(artwork_owner,first_slot(match),dx,dy))continue;
        canvas[dy*512+dx]=0xff000000u|((uint32_t)p[0]<<16)|((uint32_t)p[1]<<8)|p[2];
    }
    for(int index=0;index<serpent_count;index++)for(int y=0;y<32;y++)for(int x=0;x<48;x++) {
        const HeroPoseMatch *match=&serpents[index];int w=match->pose?32:48;
        if(x>=w)continue;
        int dx=match->x*2+x,dy=match->y*2+y;
        if(dx<0||dx>=512||dy<0||dy>=416)continue;
        const int *bounds=serpent_bounds[match->pose];
        int sx=bounds[0]+x*(bounds[2]-bounds[0])/w,sy=bounds[1]+y*(bounds[3]-bounds[1])/32;
        const unsigned char *p=serpent.pixels+(sy*serpent.width+sx)*4;
        if(p[3]<128||!artwork_priority_claim(artwork_owner,first_slot(match),dx,dy))continue;
        canvas[dy*512+dx]=0xff000000u|((uint32_t)p[0]<<16)|((uint32_t)p[1]<<8)|p[2];
    }
    for(int index=0;index<spring_count;index++) {
        const HeroPoseMatch *m=&spring_matches[index];const int *b=spring_bounds[m->pose];int w=32,h=(m->pose+2)*16;
        for(int y=0;y<h;y++)for(int x=0;x<w;x++) {
            int dx=m->x*2+x,dy=m->y*2+y;if(dx<0||dx>=512||dy<0||dy>=416)continue;
            int tx=b[0]+x*(b[2]-b[0]+1)/w,ty=b[1]+(m->flip?h-1-y:y)*(b[3]-b[1]+1)/h;
            const unsigned char *q=spring_jumper.pixels+(ty*spring_jumper.width+tx)*4;
            if(q[3]<192||!artwork_priority_claim(artwork_owner,first_slot(m),dx,dy))continue;
            canvas[dy*512+dx]=0xff000000u|((uint32_t)q[0]<<16)|((uint32_t)q[1]<<8)|q[2];
        }
    }
    for(int index=0;index<chamber_count;index++) {
        const HeroPoseMatch *m=&chambers[index];const int *b=chamber_bounds[m->pose],*n=chamber_native[m->pose];
        int w=(n[2]-n[0]+1)*2,h=(n[3]-n[1]+1)*2;
        for(int y=0;y<h;y++)for(int x=0;x<w;x++) {
            int dx=(m->x+n[0])*2+x,dy=(m->y+n[1])*2+y;
            if(dx<0||dx>=512||dy<0||dy>=416)continue;
            int sx=b[0]+x*(b[2]-b[0]+1)/w,sy=b[1]+y*(b[3]-b[1]+1)/h;
            const unsigned char *p=chamber_art.pixels+(sy*chamber_art.width+sx)*4;
            if(p[3]<128||!artwork_priority_claim(artwork_owner,first_slot(m),dx,dy))continue;
            canvas[dy*512+dx]=energy_color(p,0);
        }
    }
    for(int index=0;index<turret_count;index++) {
        const HeroPoseMatch *m=&turrets[index];const int *b=turret_bounds[m->pose];
        int h=m->pose?24:22;
        for(int y=0;y<h;y++)for(int x=0;x<48;x++) {
            int dx=m->x*2+x,dy=m->y*2+y;if(dx<0||dx>=512||dy<0||dy>=416)continue;
            int source_slot=-1;const uint8_t *oam=cyc_render_oam();
            for(int slot=0;slot<64;slot++)if(m->slots[slot]&&dx/2>=oam[slot*4+3]&&dx/2<oam[slot*4+3]+8&&dy/2>=oam[slot*4]+1&&dy/2<oam[slot*4]+9){source_slot=slot;break;}
            if(source_slot<0)continue;
            int sx=b[0]+x*(b[2]-b[0]+1)/48,sy=b[1]+y*(b[3]-b[1]+1)/h;
            const unsigned char *p=turret_art.pixels+(sy*turret_art.width+sx)*4;
            if(p[3]>=128&&artwork_priority_claim(artwork_owner,source_slot,dx,dy))canvas[dy*512+dx]=0xff000000u|((uint32_t)p[0]<<16)|((uint32_t)p[1]<<8)|p[2];
        }
    }
    for(int index=0;index<missile_count;index++) {
        const HeroPoseMatch *m=&missiles[index];int h=m->pose*16;
        for(int y=0;y<h;y++)for(int x=0;x<8;x++) {
            int dx=m->x*2+4+x,dy=m->y*2+y;if(dx<0||dx>=512||dy<0||dy>=416)continue;
            int palette=0,segment_slot=first_slot(m);const uint8_t *oam=cyc_render_oam();
            for(int slot=0;slot<64;slot++)if(m->slots[slot]&&oam[slot*4]+1<=dy/2&&oam[slot*4]+9>dy/2){palette=oam[slot*4+2]&3;segment_slot=slot;break;}
            int sx=flash_bounds[0]+x*(flash_bounds[2]-flash_bounds[0]+1)/8;
            int sy=flash_bounds[1]+(y%16)*(flash_bounds[3]-flash_bounds[1]+1)/16;
            const unsigned char *p=flash_art.pixels+(sy*flash_art.width+sx)*4;
            if(p[3]<128||!artwork_priority_claim(artwork_owner,segment_slot,dx,dy))continue;
            canvas[dy*512+dx]=energy_color(p,palette);
        }
    }
    for(int index=0;index<halo_count;index++)for(int y=0;y<16;y++)for(int x=0;x<32;x++) {
        const HeroPoseMatch *m=&halos[index];int dx=m->x*2+x,dy=m->y*2+y;
        if(dx<0||dx>=512||dy<0||dy>=416)continue;
        const int *b=halo_bounds;
        int sx=b[0]+x*(b[2]-b[0]+1)/32,sy=b[1]+y*(b[3]-b[1]+1)/16;
        const unsigned char *p=halo_art.pixels+(sy*halo_art.width+sx)*4;
        if(p[3]<128||!artwork_priority_claim(artwork_owner,first_slot(m),dx,dy))continue;
        uint32_t light=presentation_color(27);int brightness=(light>>16)&255;
        if(((light>>8)&255)>brightness)brightness=(light>>8)&255;if((light&255)>brightness)brightness=light&255;
        canvas[dy*512+dx]=0xff000000u|((uint32_t)(p[0]*brightness/255)<<16)|
            ((uint32_t)(p[1]*brightness/255)<<8)|(p[2]*brightness/255);
    }
    for(int index=0;index<swirl_count;index++) {
        const HeroPoseMatch *m=&swirls[index];int cell=m->pose<2?m->pose:2,size=(cell+1)*16;
        const int *b=swirl_bounds[cell];const uint8_t *oam=cyc_render_oam();
        uint32_t light=presentation_color(31);int brightness=(light>>16)&255;
        if(((light>>8)&255)>brightness)brightness=(light>>8)&255;if((light&255)>brightness)brightness=light&255;
        for(int y=0;y<size;y++)for(int x=0;x<size;x++) {
            int dx=m->x*2+x,dy=m->y*2+y;if(dx<0||dx>=512||dy<0||dy>=416)continue;
            bool present=false;
            for(int slot=0;slot<64;slot++)if(m->slots[slot]&&
                oam[slot*4+3]==m->x+(x/16)*8&&oam[slot*4]+1==m->y+(y/16)*8){present=true;break;}
            if(!present)continue;
            int px=m->pose==3?size-1-y:x,py=m->pose==3?x:y;
            if(m->flip){px=size-1-px;py=size-1-py;}
            int sx=b[0]+px*(b[2]-b[0]+1)/size,sy=b[1]+py*(b[3]-b[1]+1)/size;
            const unsigned char *p=swirl_art.pixels+(sy*swirl_art.width+sx)*4;
            if(p[3]<128||!artwork_priority_claim(artwork_owner,first_slot(m),dx,dy))continue;
            canvas[dy*512+dx]=0xff000000u|((uint32_t)(p[0]*brightness/255)<<16)|
                ((uint32_t)(p[1]*brightness/255)<<8)|(p[2]*brightness/255);
        }
    }
    for(int index=0;index<flash_count;index++) {
        const HeroPoseMatch *m=&flashes[index];int height=m->pose*16;
        for(int y=0;y<height;y++)for(int x=0;x<16;x++) {
            int dx=m->x*2+x,dy=m->y*2+y;
            if(dx<0||dx>=512||dy<0||dy>=416)continue;
            int sx=flash_bounds[0]+x*(flash_bounds[2]-flash_bounds[0]+1)/16;
            int sy=flash_bounds[1]+y*(flash_bounds[3]-flash_bounds[1]+1)/height;
            const unsigned char *p=flash_art.pixels+(sy*flash_art.width+sx)*4;
            if(p[3]<128||!artwork_priority_claim(artwork_owner,first_slot(m),dx,dy))continue;
            uint32_t light=presentation_color(27);
            int brightness=((light>>16)&255);int g=(light>>8)&255,b=light&255;
            if(g>brightness)brightness=g;if(b>brightness)brightness=b;
            canvas[dy*512+dx]=0xff000000u|((uint32_t)(p[0]*brightness/255)<<16)|
                ((uint32_t)(p[1]*brightness/255)<<8)|(p[2]*brightness/255);
        }
    }
    for(int index=0;index<cycling_count;index++)for(int y=0;y<32;y++)for(int x=0;x<32;x++) {
        const HeroPoseMatch *m=&cycling[index];int dx=m->x*2+x,dy=m->y*2+y;
        if(dx<0||dx>=512||dy<0||dy>=416)continue;
        int palette=m->palette;bool present=false;
        const uint8_t *oam=cyc_render_oam();
        for(int slot=0;slot<64;slot++)if(m->slots[slot]&&
            oam[slot*4+3]==m->x+(x>=16?8:0)&&oam[slot*4]+1==m->y+(y>=16?8:0)){palette=oam[slot*4+2]&3;present=true;}
        if(!present)continue;
        const int *b=power_bounds;
        int sx=m->pose?b[0]+x*(b[2]-b[0]+1)/32:blue_crop_x+x*blue_crop_w/32;
        int sy=m->pose?b[1]+y*(b[3]-b[1]+1)/32:blue_crop_y+y*blue_crop_h/32;
        const HeroArt *art=m->pose?&power_art:&blue_orb;
        const unsigned char *p=art->pixels+(sy*art->width+sx)*4;
        if(p[3]<128||!artwork_priority_claim(artwork_owner,first_slot(m),dx,dy))continue;
        uint32_t color=energy_color(p,palette);
        if(m->pose&&p[0]>=160&&p[1]>=160&&p[2]>=160&&(color&0xffffff))
            color=0xff000000u|((uint32_t)p[0]<<16)|((uint32_t)p[1]<<8)|p[2];
        canvas[dy*512+dx]=color;
    }
    for(int index=0;index<green_count;index++)for(int y=0;y<64;y++)for(int x=0;x<64;x++) {
        const HeroPoseMatch *match=&greens[index];
        int size=match->pose>=2?32:64;if(x>=size||y>=size)continue;
        int dx=match->x*2+x,dy=match->y*2+y;
        if(dx<0||dx>=512||dy<0||dy>=416)continue;
        /* Match both native sizes; the second native pose changes orientation. */
        int px=match->pose%2?size-1-y:x,py=match->pose%2?x:y;
        if(match->flip)px=size-1-px;
        if(match->palette&128)py=size-1-py;
        int sx=green_crop_x+px*green_crop_w/size,sy=green_crop_y+py*green_crop_h/size;
        const unsigned char *p=green_orb.pixels+(sy*green_orb.width+sx)*4;
        if(p[3]<128||!artwork_priority_claim(artwork_owner,first_slot(match),dx,dy))continue;
        canvas[dy*512+dx]=0xff000000u|((uint32_t)p[0]<<16)|((uint32_t)p[1]<<8)|p[2];
    }
    for(int index=guardian_count-1;index>=0;index--)for(int y=0;y<32;y++)for(int x=0;x<32;x++) {
        const HeroPoseMatch *match=&guardians[index];int dx=match->x*2+x,dy=match->y*2+y;
        if(dx<0||dx>=512||dy<0||dy>=416||
           (match->pose<3&&cyc_frame_bg_opaque()[(dy/2)*256+dx/2]))continue;
        int sy,sx;const HeroArt *art=&guardian;
        if(match->pose==3) {
            art=&blue_orb;sx=blue_crop_x+x*blue_crop_w/32;sy=blue_crop_y+y*blue_crop_h/32;
        } else {
            const int *bounds=guardian_bounds[match->pose];
            if(match->pose==2) {
                sx=bounds[0]+x*(bounds[2]-bounds[0]+1)/32;
                sy=bounds[1]+y*(bounds[3]-bounds[1]+1)/32;
            } else {
                int center=match->pose?362:418;
                sx=center+(int)((x-15.5)*20.6);
                sy=660-(int)((31-(match->flip?31-y:y))*20.6);
            }
            if(sx<bounds[0]||sx>bounds[2]||sy<bounds[1]||sy>bounds[3])continue;
            sx+=match->pose*724;
        }
        const unsigned char *p=art->pixels+(sy*art->width+sx)*4;
        if(p[3]<128||!artwork_priority_claim(artwork_owner,first_slot(match),dx,dy))continue;
        canvas[dy*512+dx]=0xff000000u|((uint32_t)p[0]<<16)|((uint32_t)p[1]<<8)|p[2];
    }
    for(int index=0;index<pellet_count;index++)for(int y=0;y<16;y++)for(int x=0;x<16;x++) {
        const OrbMatch *effect=&pellets[index];
        if(effect->pose==7) {
            if(x>=4)continue;
            int dx=effect->x*2+6+x,dy=effect->y*2+y;if(dx>=512||dy>=416)continue;
            int sx=flash_bounds[0]+x*(flash_bounds[2]-flash_bounds[0]+1)/4;
            int sy=flash_bounds[1]+y*(flash_bounds[3]-flash_bounds[1]+1)/16;
            const unsigned char *p=flash_art.pixels+(sy*flash_art.width+sx)*4;
            if(p[3]>=128&&artwork_priority_claim(artwork_owner,effect->slot,dx,dy))canvas[dy*512+dx]=energy_color(p,2);
            continue;
        }
        if(effect->pose>=4) {
            int size=effect->pose==6?12:16;
            if(x>=size||y>=size)continue;
            int offset=effect->pose==6?2:0;
            int dx=effect->x*2+x+offset,dy=effect->y*2+y+offset;if(dx>=512||dy>=416)continue;
            const HeroArt *art=effect->pose==4?&pellet:&impact;
            bool bead=effect->pose!=5;art=bead?&pellet:&impact;
            int sx=bead?pellet_crop_x+x*pellet_crop_w/size:impact_crop_x+x*impact_crop_w/size;
            int sy=bead?pellet_crop_y+y*pellet_crop_h/size:impact_crop_y+y*impact_crop_h/size;
            const unsigned char *p=art->pixels+(sy*art->width+sx)*4;
            if(p[3]<128||!artwork_priority_claim(artwork_owner,effect->slot,dx,dy))continue;
            canvas[dy*512+dx]=energy_color(p,effect->pose==6?1:0);continue;
        }
        if(effect->pose==3) {
            int dx=effect->x*2+x,dy=effect->y*2+y;
            if(dx>=512||dy>=416)continue;
            bool right=(cyc_render_oam()[effect->slot*4+2]&64)!=0;
            int sx=impact_crop_x+(right?x:15-x)*impact_crop_w/16;
            int sy=impact_crop_y+y*impact_crop_h/16;
            const unsigned char *p=impact.pixels+(sy*impact.width+sx)*4;
            if(p[3]<128||!artwork_priority_claim(artwork_owner,effect->slot,dx,dy))continue;
            canvas[dy*512+dx]=0xff000000u|((uint32_t)p[0]<<16)|((uint32_t)p[1]<<8)|p[2];
            continue;
        }
        if(x>=12||y>=12)continue;
        const OrbMatch *match=&pellets[index];int w=match->pose==1?8:12;
        if(x>=w||(match->pose==2&&y>=8))continue;
        int dx=match->x*2+x+(match->pose==1?4:2),dy=match->y*2+y+(match->pose==2?4:2);
        if(dx>=512||dy>=416)continue;
        const HeroArt *art=match->pose?&shot:&pellet;
        int sx=match->pose?shot_crop_x+x*shot_crop_w/w:pellet_crop_x+x*pellet_crop_w/w;
        int sy=match->pose?shot_crop_y+y*shot_crop_h/12:pellet_crop_y+y*pellet_crop_h/12;
        if(match->pose==2) {
            sx=shot_crop_x+y*shot_crop_w/8;
            bool right=(cyc_render_oam()[match->slot*4+2]&64)!=0;
            sy=shot_crop_y+(right?11-x:x)*shot_crop_h/12;
        }
        const unsigned char *p=art->pixels+(sy*art->width+sx)*4;
        if(p[3]<128||!artwork_priority_claim(artwork_owner,match->slot,dx,dy))continue;
        canvas[dy*512+dx]=0xff000000u|((uint32_t)p[0]<<16)|((uint32_t)p[1]<<8)|p[2];
    }
    for(int index=0;index<bolt_count;index++)for(int y=0;y<32;y++)for(int x=0;x<16;x++) {
        int dx=bolts[index].x*2+x,dy=bolts[index].y*2+y;
        if(dx<0||dx>=512||dy<0||dy>=416)continue;
        int sx=bolt_crop_x+x*bolt_crop_w/16,sy=bolt_crop_y+y*bolt_crop_h/32;
        const unsigned char *p=bolt.pixels+(sy*bolt.width+sx)*4;if(p[3]<128||!artwork_priority_claim(artwork_owner,bolts[index].slot,dx,dy))continue;
        canvas[dy*512+dx]=0xff000000u|((uint32_t)p[0]<<16)|((uint32_t)p[1]<<8)|p[2];
    }
    for(int index=0;index<orb_count;index++)for(int y=0;y<32;y++)for(int x=0;x<32;x++) {
        const OrbMatch *match=&orbs[index];int dx=match->x*2+x,dy=match->y*2+y;
        if(dx<0||dx>=512||dy<0||dy>=416)continue;
        int sx=match->pose*(orb.width/2)+orb_crop_x+x*orb_crop_w/32;
        int sy=orb_crop_y+y*orb_crop_h/32;
        const unsigned char *p=orb.pixels+(sy*orb.width+sx)*4;if(p[3]<128||!artwork_priority_claim(artwork_owner,match->slot,dx,dy))continue;
        canvas[dy*512+dx]=0xff000000u|((uint32_t)p[0]<<16)|((uint32_t)p[1]<<8)|p[2];
    }
    for(int index=pod_count-1;index>=0;index--)for(int y=0;y<32;y++)for(int x=0;x<32;x++) {
        const HeroPoseMatch *match=&pods[index];int dx=match->x*2+x,dy=match->y*2+y;
        if(dx<0||dx>=512||dy<0||dy>=416)continue;
        int source_slot=-1;const uint8_t *oam=cyc_render_oam();
        for(int slot=0;slot<64;slot++)if(match->slots[slot]&&dx/2>=oam[slot*4+3]&&dx/2<oam[slot*4+3]+8&&dy/2>=oam[slot*4]+1&&dy/2<oam[slot*4]+9){source_slot=slot;break;}
        if(source_slot<0)continue;
        int sx=match->pose*(pod.width/2)+pod_crop_x+(match->flip?31-x:x)*pod_crop_w/32;
        int sy=pod_crop_y+y*pod_crop_h/32;
        const HeroArt *art=&pod;
        if(match->pose==2) {
            art=&spindle;sx=spindle_crop_x+x*spindle_crop_w/32;sy=spindle_crop_y+y*spindle_crop_h/32;
        }
        const unsigned char *p=art->pixels+(sy*art->width+sx)*4;if(p[3]<128||!artwork_priority_claim(artwork_owner,source_slot,dx,dy))continue;
        canvas[dy*512+dx]=0xff000000u|((uint32_t)p[0]<<16)|((uint32_t)p[1]<<8)|p[2];
    }
    for(int index=wyvern_count-1;index>=0;index--)for(int y=0;y<72;y++)for(int x=0;x<64;x++) {
        const HeroPoseMatch *match=&wyverns[index];
        int dx=match->x*2+x-(match->flip?16:0),dy=match->y*2+y-8;
        if(dx<0||dx>=512||dy<0||dy>=416)continue;
        int local_x=match->flip?63-x:x;
        int sx=wyvern_eye_x[match->pose]+(int)((local_x-8)*9.55);
        int sy=wyvern_eye_y[match->pose]+(int)((y-31)*9.55);
        const int *bounds=wyvern_bounds[match->pose];
        if(sx<bounds[0]||sx>bounds[2]||sy<bounds[1]||sy>bounds[3])continue;
        const unsigned char *p=wyvern.pixels+(sy*wyvern.width+sx)*4;if(p[3]<128||!artwork_priority_claim(artwork_owner,first_slot(match),dx,dy))continue;
        canvas[dy*512+dx]=0xff000000u|((uint32_t)p[0]<<16)|((uint32_t)p[1]<<8)|p[2];
    }
    paint_flying_masks();
    paint_bonus_treasures();
    int player_w=80,player_h=72;
    if(replace)for(int index=player_count-1;index>=0;index--)for(int y=0;y<player_h;y++)for(int x=0;x<player_w;x++) {
        const HeroPoseMatch *player=&players[index];
        if(!ground_scene&&player->flash==8&&phoenix.pixels) {
            if(x>=64||y>=64)continue;
            int dx=player->x*2+x,dy=player->y*2+y;if(dx<0||dx>=512||dy<0||dy>=480)continue;
            int cell=phoenix.width/3,sx=player->pose*cell+(x/2)*cell/32,sy=(y/2)*phoenix.height/32;
            const unsigned char *p=phoenix.pixels+(sy*phoenix.width+sx)*4;
            if(p[3]>=192&&artwork_priority_claim(artwork_owner,first_slot(player),dx,dy))canvas[dy*512+dx]=0xff000000u|((uint32_t)p[0]<<16)|((uint32_t)p[1]<<8)|p[2];
            continue;
        }
        bool on_ground=ground_scene&&player->pose>=3;
        if(on_ground&&(x>=48||y>=((player->pose==4||player->pose==9)?46:62)))continue;
        int dx=player->x*2+x,dy=player->y*2+y;
        if(!ground_scene){dx-=8;dy-=4;} /* Keep the native center as artwork grows. */
        if(ground_scene&&!on_ground){dx-=player->flip?0:32;dy-=8;}
        /* Players can occupy the lower screen. HUD protection for other
         * replacements must not make a living player disappear here. */
        if(dx<0||dx>=512||dy<0||dy>=480)continue;
        int sx=crop_x+x*crop_w/64,sy=crop_y+y*crop_h/64;
        const HeroArt *art=ground_scene?&ground:&hero[player->pose];
        if(on_ground) {
            int pose=player->pose==4?1:0,h=pose?46:62;
            const int *bounds=grounded_bounds[pose];art=&grounded;
            sx=bounds[0]+(player->flip?47-x:x)*(bounds[2]-bounds[0])/48;
            sy=bounds[1]+y*(bounds[3]-bounds[1])/h;
            if(player->pose>=8) {
                bounds=jump_bounds[player->pose-8];art=&jumping;
                int h=player->pose==9?46:62;
                sx=bounds[0]+(player->flip?47-x:x)*(bounds[2]-bounds[0]+1)/48;
                sy=bounds[1]+y*(bounds[3]-bounds[1]+1)/h;
            } else if(player->pose>=5) {
                art=&walking;
                sx=walk_left[player->pose-5]+(player->flip?47-x:x)*600/48;
                sy=24+y*667/62;
            }
        } else if(ground_scene) {
            int local_x=player->flip?79-x:x;
            sx=ground_head_x[player->pose]+(int)((local_x-56)*8.6);
            sy=ground_foot_y[player->pose]+(int)((y-71)*8.6);
            const int *bounds=ground_bounds[player->pose];
            if(sx<bounds[0]||sx>bounds[2]||sy<bounds[1]||sy>bounds[3])continue;
        }
        const unsigned char *p=ground_scene?art->pixels+(sy*art->width+sx)*4:flight_filtered[player->pose]+(y*80+x)*4;
        if(p[3]<128)continue;
        /* Power flashes retain the complete enhanced silhouette. */
        
        int r=p[0],g=p[1],b=p[2];
        /* Preserve the original two-player palette distinction. Only the
         * saturated red costume pixels become blue; gold wings stay gold. */
        if(player->palette&&r>70&&g<70&&b>20&&r>g*2) {
            int old=r;r=b/2;g=old/2;b=old;
        }
        if(player->flash&&player->flash<6&&!ground_scene) {
            static const int white_regions[6]={0,7,7,3,5,6};
            int region=y<18?1:y<36?2:y<54?4:8;
            if(white_regions[player->flash]&region) {
                int shade=(r+g+b)/3;
                r=60+shade*3/4;g=42+shade*3/4;b=12+shade*2/3;
            }
        }
        if(ground_scene&&player->flash==1&&sy>150&&sy<235&&
           sx>ground_head_x[player->pose]-50&&sx<ground_head_x[player->pose]+50) {
            int shade=(r+g+b)/3;r=60+shade*3/4;g=42+shade*3/4;b=12+shade*2/3;
        }
        bool white_flash=player->flash==3||(player->flash==4&&y<56)||
            (player->flash==5&&(y<40||y>=56))||(player->flash==6&&(y<24||y>=40))||
            (player->flash==7&&y>=24);
        if(ground_scene&&player->flash==8&&player->palette==2){r=r*3/4+55;g=g*3/4+38;b=b*3/4+12;}
        if(ground_scene&&white_flash) {
            int shade=(r+g+b)/3;r=60+shade*3/4;g=48+shade*3/4;b=20+shade*2/3;
        }
        if(artwork_priority_claim(artwork_owner,first_slot(player),dx,dy))
            canvas[dy*512+dx]=0xff000000u|((uint32_t)r<<16)|((uint32_t)g<<8)|b;
    }
    /* Retain exact native max-power ink and palettes, including flashes. */
    if(native_power_active) {
        const uint8_t *o=cyc_render_oam();
        for(int slot=0;slot<64;slot++)if(native_power_slots[slot])
        for(int y=o[slot*4]+1;y<o[slot*4]+9&&y<240;y++)
        for(int x=o[slot*4+3];x<o[slot*4+3]+8&&x<256;x++) {
            if(!sprites[y*256+x])continue;uint32_t c=src[y*256+x];
            for(int yy=0;yy<2;yy++)for(int xx=0;xx<2;xx++) {
                int at=(y*2+yy)*512+x*2+xx;
                if(artwork_owner[at]==slot)canvas[at]=c;
            }
        }
    }
    paint_guardian_darts();
    paint_hud();
    paint_title();
    return canvas;
}
static void frame_end(void *ctx) {
    (void)ctx;frames++;
    if(presentation_benchmark&&frames==1) {
        int w,h;present(NULL,&w,&h);
        Uint64 start=SDL_GetPerformanceCounter();
        for(int i=0;i<presentation_benchmark;i++)present(NULL,&w,&h);
        double ms=(SDL_GetPerformanceCounter()-start)*1000.0/SDL_GetPerformanceFrequency()/presentation_benchmark;
        fprintf(stderr,"presentation benchmark: %.3f ms/frame (%d warm renders, enhanced=%d background=%d)\n",ms,presentation_benchmark,enhanced,background_art);
    }
    if(shot_path[0]&&frames==shot_frame){int w,h;const uint32_t *pic=present(NULL,&w,&h);
        if(!cyc_write_png(shot_path,pic,w,h))fprintf(stderr,"Cannot save enhanced capture\n");
        if(background_lines_path[0]) {
            FILE *f=fopen(background_lines_path,"wb");
            if(f){for(int y=0;y<240;y++)fprintf(f,"%d,%d,%d,%u,%u\n",y,cyc_render_line_scroll_x(y),cyc_render_line_scroll_y(y),cyc_render_line_bg_table(y),cyc_render_line_mask(y));fclose(f);}
            else fprintf(stderr,"Cannot save background line diagnostics\n");
        }
    }
}
static bool option(void *ctx,const char *name,const char *value) {
    (void)ctx;
    if(!strcmp(name,"--presentation-benchmark"))presentation_benchmark=atoi(value);
    else if(!strcmp(name,"--static-water"))water_motion=false;
    else if(!strcmp(name,"--moving-water"))water_motion=true;
    else if(!strcmp(name,"--original-graphics"))enhanced=false;
    else if(!strcmp(name,"--enhanced-graphics"))enhanced=true;
    else if(!strcmp(name,"--enhanced-shot"))snprintf(shot_path,sizeof(shot_path),"%s",value);
    else if(!strcmp(name,"--enhanced-shot-frame"))shot_frame=atol(value);
    else if(!strcmp(name,"--no-damage"))debug_no_damage=true;
    else if(!strcmp(name,"--normal-damage"))debug_no_damage=false;
    else if(!strcmp(name,"--capture-infinite-lives"))capture_infinite_lives=true;
    else if(!strcmp(name,"--background-art"))background_art=true;
    else if(!strcmp(name,"--no-background-art"))background_art=false;
    else if(!strcmp(name,"--background-lines"))snprintf(background_lines_path,sizeof(background_lines_path),"%s",value);
    else return false;return true;
}
static void load(void *ctx,const char *key,const char *value) {
    (void)ctx;if(!strcmp(key,"EnhancedGraphics"))enhanced=strcmp(value,"0")!=0;
    else if(!strcmp(key,"WaterMotion"))water_motion=strcmp(value,"0")!=0;
    else if(!strcmp(key,"BackgroundArtwork"))background_art=strcmp(value,"0")!=0;
}
static void save(void *ctx,FILE *file){(void)ctx;fprintf(file,"EnhancedGraphics = %d\nBackgroundArtwork = %d\nWaterMotion = %d\n",enhanced?1:0,background_art?1:0,water_motion?1:0);}
static const CycHostOption options[]={
    {"--no-damage",false,"Debug invulnerability for both players"},
    {"--normal-damage",false,"Normal damage (default)"},
    {"--static-water",false,"Keep ocean artwork still"},
    {"--moving-water",false,"Animate ocean artwork"},
    {"--presentation-benchmark",true,"Measure warm rendering at the first frame (diagnostic only)"},
    {"--original-graphics",false,"Original presentation"},
    {"--enhanced-graphics",false,"Enhanced player artwork and atmospheric backdrop"},
    {"--enhanced-shot",true,"Save enhanced presentation PNG"},
    {"--enhanced-shot-frame",true,"Capture frame, one-based (default 301)"},
    {"--capture-infinite-lives",false,"Opt-in recognized-scene life refill for artwork capture"},
    {"--background-art",false,"Enable mapped replacement background artwork"},
    {"--no-background-art",false,"Disable mapped replacement background artwork"},
    {"--background-lines",true,"Capture per-line background diagnostics alongside enhanced shot"}
};
const CycHostExtras *cyc_host_extras(void) {
    static const CycHostExtras extras={.present=present,.frame_begin=frame_begin,.frame_end=frame_end,.load_setting=load,
        .save_settings=save,.options=options,.option_count=sizeof(options)/sizeof(options[0]),.option=option};return &extras;
}














