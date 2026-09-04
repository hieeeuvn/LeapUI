/*
 * LeapUI - Minimal GBA-only frontend for NocturnalRTOS / DartOS
 * Inspired by FrogUI (directory / input / libretro wrapper) and Slot (shelf UX).
 *
 * - Scans <ROMS>/Game Boy Advance/*.gba, fallback <ROMS>/ (up to 512)
 * - Horizontal Slot-style carousel: L/R or Left/Right browses, center cart focused
 * - Tap A = resume (gpsp savestate), Hold A 500ms = clean boot
 * - Runs gpsp via DARTOS RETRO_ENVIRONMENT_RUN_EMULATOR
 * - 320x240 RGB565 direct framebuffer
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <time.h>
#include <ctype.h>
#include <math.h>
#include <unistd.h>

#include "libretro.h"
#include "font.h"
#include "render.h"
#include "shelf.h"
#include "theme.h"
#include "leapui.h"

#ifdef DARTOS
#include "core_api.h"
#include "dartos.h"
#include "frontend_functions.h"
// DARTOS helpers from fallback
extern char game_name_buf[256];
extern char core_name_buf[256];
extern int fallback_load_rgb565_image(const char* fn, uint16_t* fb,int w,int h);
extern bool fallback_is_zip_wqw_file(const char *p,bool *out);
extern void fallback_wrap_run_game(const char *path,int state);
#define WRAP_RUN_GAME fallback_wrap_run_game
#else
// unix fallback
static char game_name_buf[256];
static char core_name_buf[256];
static void WRAP_RUN_GAME(const char *a,int b){ (void)a;(void)b; }
#endif

#define TAG "LeapUI"

static retro_video_refresh_t video_cb=NULL;
static retro_audio_sample_t audio_cb=NULL;
static retro_audio_sample_batch_t audio_batch_cb=NULL;
retro_environment_t environ_cb=NULL;
static retro_input_poll_t input_poll_cb=NULL;
static retro_input_state_t input_state_cb=NULL;

static uint16_t *framebuffer=NULL;
static char roms_path[256]="/media/mmcblk0p2/ROMS";
static char gba_path[256]="/media/mmcblk0p2/ROMS/Game Boy Advance";
char assets_dir[256]="/media/mmcblk0p2/system/assets/LeapUI";
static char last_cart_stem[128]="";

static LeapShelf shelf;
static LeapPhase phase=PHASE_SHELF;
static float anim_t=0;
static bool core_ready=false;
static bool resumed_boot=false;
static char pending_cart_path[512]="";
static char pending_cart_stem[128]="";

static bool clean_boot=false; // hold-A flag
static uint32_t hold_start_ms=0;
static bool hold_active=false;
static bool game_queued=false; // FrogUI style

static Thumb cur_thumb={0};
static bool thumb_valid=false;
static int last_thumb_idx=-1;

// timing
static uint32_t now_ms(void){
#ifdef DARTOS
    return (uint32_t)xTaskGetTickCount();
#else
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts);
    return (uint32_t)(ts.tv_sec*1000 + ts.tv_nsec/1000000);
#endif
}
void leapui_log(const char *fmt, ...){
    // Follow the firmware convention: <root>/system/logs/leapui.log (next to Phobos.log).
    // <root> is the SD root, derived as the parent of the ROMS directory.
    char path[512];
    char root[256];
    strncpy(root, "/media/mmcblk0p2", sizeof(root)-1); root[sizeof(root)-1]=0;
    const char *slash=strrchr(roms_path,'/');
    if(slash && slash!=roms_path){
        size_t n=(size_t)(slash-roms_path);
        if(n<sizeof(root)-1){ memcpy(root, roms_path, n); root[n]=0; }
    }
    snprintf(path,sizeof(path),"%s/system/logs/leapui.log", root);
    FILE *f=fopen(path,"a");
    if(!f) return;
    va_list ap; va_start(ap,fmt); vfprintf(f,fmt,ap); va_end(ap);
    fprintf(f,"\n"); fclose(f);
}

// ---- paths ----
static void build_paths(void){
    const char *dir=NULL;
#ifdef RETRO_ENVIRONMENT_GET_ROMS_DIRECTORY
    if(environ_cb && environ_cb(RETRO_ENVIRONMENT_GET_ROMS_DIRECTORY, &dir) && dir){
        strncpy(roms_path, dir, sizeof(roms_path)-1);
    } else
#endif
    {
        const char *save=NULL;
        if(environ_cb && environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &save) && save){
            // e.g. save = /media/mmcblk0p2/Saves -> parent = /media/mmcblk0p2
            char tmp[256]; strncpy(tmp, save, sizeof(tmp)-1);
            char *slash=strrchr(tmp,'/');
            if(slash) *slash=0;
            snprintf(roms_path, sizeof(roms_path), "%s/ROMS", tmp);
        }
    }
    // trim trailing slash
    size_t l=strlen(roms_path); while(l>1 && roms_path[l-1]=='/'){ roms_path[l-1]=0; l--; }
    // DartOS convention: keep the "Game Boy Advance" default (matches console_mappings.opt); avoid stat() to dodge syscalls on HCRTOS
    snprintf(gba_path, sizeof(gba_path), "%s/Game Boy Advance", roms_path);

    const char *core_assets=NULL;
    if(environ_cb && environ_cb(RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY, &core_assets) && core_assets){
        strncpy(assets_dir, core_assets, sizeof(assets_dir)-1);
    } else {
        char *slash=strrchr(roms_path,'/');
        if(slash){
            char parent[256]; strncpy(parent, roms_path, slash-roms_path); parent[slash-roms_path]=0;
            char try_new[512], try_old[512];
            snprintf(try_new,sizeof(try_new),"%s/system/assets/LeapUI", parent);
            snprintf(try_old,sizeof(try_old),"%s/HCRTOS/assets/LeapUI", parent);
            struct stat st;
            if(stat(try_new,&st)==0) strncpy(assets_dir, try_new, sizeof(assets_dir)-1);
            else if(stat(try_old,&st)==0) strncpy(assets_dir, try_old, sizeof(assets_dir)-1);
            else strncpy(assets_dir, try_new, sizeof(assets_dir)-1);
        }
    }
}

// ---- persistence: remember last cart for resume ----
static void persist_save(void){
    char path[512]; snprintf(path,sizeof(path),"%s/last_cart.txt", assets_dir);
    // ensure dir exists (ignore fail)
    FILE *f=fopen(path,"w");
    if(f){ fprintf(f,"%s\n", last_cart_stem); fclose(f); }
}
static void persist_load(void){
    char path[512]; snprintf(path,sizeof(path),"%s/last_cart.txt", assets_dir);
    FILE *f=fopen(path,"r");
    if(!f) return;
    if(fgets(last_cart_stem,sizeof(last_cart_stem),f)){
        size_t n=strlen(last_cart_stem); while(n && (last_cart_stem[n-1]=='\n'||last_cart_stem[n-1]=='\r')) last_cart_stem[--n]=0;
    }
    fclose(f);
}

// ---- thumb ----
static void update_thumb(void){
    int idx=shelf.index;
    if(idx==last_thumb_idx && thumb_valid) return;
    if(thumb_valid){ thumb_free(&cur_thumb); thumb_valid=false; }
    const LeapCart *c=shelf_current(&shelf);
    if(!c){ last_thumb_idx=idx; return; }
    if(thumb_load(c->path, &cur_thumb)) thumb_valid=true;
    last_thumb_idx=idx;
}

// ---- launch ----
static void queue_insert(bool clean){
    const LeapCart *c=shelf_current(&shelf);
    if(!c) return;
    strncpy(game_name_buf, c->path, sizeof(game_name_buf)-1);
    strncpy(core_name_buf, "gpSP", sizeof(core_name_buf)-1);
    strncpy(last_cart_stem, c->stem, sizeof(last_cart_stem)-1);
    persist_save();
    leapui_log("queue_insert FrogUI clean=%d core=%s rom=%s", clean, core_name_buf, game_name_buf);
    // Note: gpsp savestate resume is automatic if file exists; clean flag could delete state
    // For clean boot we unlink the state file via frontend unlink if available
    if(clean){
        // gpsp states live in Saves/*.srm? DartOS uses /Saves/<stem>.state
        // Try unlink common locations, ignore errors
        char state_path[512];
        // derive the parent from roms_path, then try <root>/Saves/<stem>.state
        char *slash=strrchr(roms_path,'/');
        char parent[256]="";
        if(slash){ strncpy(parent, roms_path, slash-roms_path); parent[slash-roms_path]=0; }
        snprintf(state_path,sizeof(state_path),"%s/Saves/%s.state", parent, c->stem);
        unlink(state_path);
        snprintf(state_path,sizeof(state_path),"%s/Saves/%s.state.auto", parent, c->stem);
        unlink(state_path);
    }
    game_queued=true;
}

static void do_launch(void){
    leapui_log("do_launch core=%s rom=%s", core_name_buf, game_name_buf);
    phase=PHASE_PLAYING;
    WRAP_RUN_GAME("/media/mmcblk0p2/ROMS/menu;m.gba", 0);
}

// ---- input ----
static int btn_down(unsigned id){
    if(!input_state_cb) return 0;
    return input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, id);
}

static void handle_input_shelf(void){
    static int prev_l=0, prev_r=0, prev_a=0, prev_b=0, prev_sel=0;
    int l = btn_down(RETRO_DEVICE_ID_JOYPAD_L) || btn_down(RETRO_DEVICE_ID_JOYPAD_LEFT);
    int r = btn_down(RETRO_DEVICE_ID_JOYPAD_R) || btn_down(RETRO_DEVICE_ID_JOYPAD_RIGHT);
    int a = btn_down(RETRO_DEVICE_ID_JOYPAD_A);
    int b = btn_down(RETRO_DEVICE_ID_JOYPAD_B);
    int sel = btn_down(RETRO_DEVICE_ID_JOYPAD_SELECT) || btn_down(RETRO_DEVICE_ID_JOYPAD_START);

    if(l && !prev_l) { shelf_move(&shelf, -1); update_thumb(); }
    if(r && !prev_r) { shelf_move(&shelf, +1); update_thumb(); }

    // A hold detection: Slot's hold-A = clean boot
    if(a && !prev_a){
        hold_start_ms=now_ms();
        hold_active=true;
    }
    if(!a && prev_a){
        if(hold_active){
            uint32_t held = now_ms() - hold_start_ms;
            bool clean = held >= PLAY_HOLD_MS;
            if(shelf.count>0) queue_insert(clean);
        }
        hold_active=false;
    }
    // if held long enough, could show hint (not yet)
    if(hold_active && a){
        // optional auto-trigger after 800ms hold without release? we wait for release
    }

    if(b && !prev_b){
        // B on shelf does nothing (Slot uses MENU for About)
    }
    if(sel && !prev_sel){
        phase=PHASE_ABOUT;
    }
    prev_l=l; prev_r=r; prev_a=a; prev_b=b; prev_sel=sel;
}

static void handle_input_about(void){
    static int prev_a=0, prev_b=0;
    int a=btn_down(RETRO_DEVICE_ID_JOYPAD_A);
    int b=btn_down(RETRO_DEVICE_ID_JOYPAD_B);
    int sel=btn_down(RETRO_DEVICE_ID_JOYPAD_SELECT)||btn_down(RETRO_DEVICE_ID_JOYPAD_START);
    if((b && !prev_b) || (a && !prev_a) || (sel && !prev_b)){
        phase=PHASE_SHELF;
    }
    prev_a=a; prev_b=b;
}

// ---- shelf rendering ----
static void render_shelf(void){
    render_backdrop(framebuffer);
    if(shelf.count==0){
        render_center_banner(framebuffer, NULL, "No GBA ROMs");
        return;
    }
    const LeapCart *cur=shelf_current(&shelf);
    Thumb *th_center = thumb_valid? &cur_thumb : NULL;
    render_center_banner(framebuffer, th_center, cur?cur->stem:"");
    // left/right preview
    if(shelf.count>1){
        int left_idx = (shelf.index-1+shelf.count)%shelf.count;
        int right_idx = (shelf.index+1)%shelf.count;
        // no thumbs for the side tiles (keep it light); label only
        render_side_preview(framebuffer, -1, NULL, shelf.carts[left_idx].stem);
        render_side_preview(framebuffer,  1, NULL, shelf.carts[right_idx].stem);
    }
    if(hold_active){
        uint32_t held=now_ms()-hold_start_ms;
        if(held>300){
            int bar_w=(held*60)/PLAY_HOLD_MS; if(bar_w>60) bar_w=60;
            int bx=(SCREEN_W-60)/2, by=SCREEN_H-28;
            render_fill(framebuffer,bx,by,60,2,C_BORDER);
            render_fill(framebuffer,bx,by,bar_w,2,C_SELECT);
        }
    }
}
static void render_inserting(float seat){
    (void)seat;
    render_backdrop(framebuffer);
    const LeapCart *c=shelf_current(&shelf);
    Thumb *th=thumb_valid?&cur_thumb:NULL;
    render_center_banner(framebuffer, th, c?c->stem:"LOADING...");
}

static void render_about(void){
    render_backdrop(framebuffer);
    render_center_banner(framebuffer, NULL, "LeapUI v0.3 GBA-only");
    font_draw(framebuffer,SCREEN_W,SCREEN_H, 30, 170, "Slot-inspired  DartOS  gpSP", C_BORDER);
}

// ---- libretro ----
void retro_init(void){
    framebuffer=(uint16_t*)malloc(SCREEN_W*SCREEN_H*sizeof(uint16_t));
    build_paths();
    theme_load(assets_dir);
    font_init();
    leapui_log("retro_init roms=%s gba=%s assets=%s", roms_path, gba_path, assets_dir);
    persist_load();
    shelf_init(&shelf);
    int n=shelf_scan(&shelf, gba_path);
    if(n<=0){
        shelf_scan(&shelf, roms_path);
    }
    leapui_log("shelf_scan gba=%s n=%d count=%d", gba_path, n, shelf.count);
    // restore last selection
    if(last_cart_stem[0]){
        for(int i=0;i<shelf.count;i++) if(strcmp(shelf.carts[i].stem, last_cart_stem)==0){ shelf_set_index(&shelf,i); break; }
    }
    update_thumb();
    phase=PHASE_SHELF;
    anim_t=0;
}

void retro_deinit(void){
    if(thumb_valid) thumb_free(&cur_thumb);
    if(framebuffer){ free(framebuffer); framebuffer=NULL; }
}

unsigned retro_api_version(void){ return RETRO_API_VERSION; }
void retro_set_controller_port_device(unsigned p,unsigned d){ (void)p;(void)d; }
void retro_get_system_info(struct retro_system_info *info){
    memset(info,0,sizeof(*info));
    info->library_name="LeapUI";
    info->library_version="0.3";
    info->need_fullpath=true;
    info->valid_extensions="gba";
    info->block_extract=false;
}
void retro_get_system_av_info(struct retro_system_av_info *info){
    info->timing.fps=60.0;
    info->timing.sample_rate=44100.0;
    info->geometry.base_width=SCREEN_W;
    info->geometry.base_height=SCREEN_H;
    info->geometry.max_width=SCREEN_W;
    info->geometry.max_height=SCREEN_H;
    info->geometry.aspect_ratio=(float)SCREEN_W/SCREEN_H;
}
void retro_set_environment(retro_environment_t cb){
    environ_cb=cb;
    bool no_content=true; cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_content);
    enum retro_pixel_format fmt=RETRO_PIXEL_FORMAT_RGB565;
    cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);
}
void retro_set_audio_sample(retro_audio_sample_t cb){ audio_cb=cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb){ audio_batch_cb=cb; }
void retro_set_input_poll(retro_input_poll_t cb){ input_poll_cb=cb; }
void retro_set_input_state(retro_input_state_t cb){ input_state_cb=cb; }
void retro_set_video_refresh(retro_video_refresh_t cb){ video_cb=cb; }
void retro_reset(void){ phase=PHASE_SHELF; anim_t=0; }
void retro_cheat_reset(void){}
void retro_cheat_set(unsigned a,bool b,const char*c){(void)a;(void)b;(void)c;}
bool retro_load_game(const struct retro_game_info *info){ (void)info; return true; }
bool retro_load_game_special(unsigned a,const struct retro_game_info *b,size_t c){(void)a;(void)b;(void)c;return false;}
void retro_unload_game(void){}
unsigned retro_get_region(void){ return RETRO_REGION_NTSC; }
size_t retro_serialize_size(void){ return 0; }
bool retro_serialize(void *a,size_t b){(void)a;(void)b;return false;}
bool retro_unserialize(const void *a,size_t b){(void)a;(void)b;return false;}
void *retro_get_memory_data(unsigned a){(void)a;return NULL;}
size_t retro_get_memory_size(unsigned a){(void)a;return 0;}

void retro_run(void){
    if(input_poll_cb) input_poll_cb();

    // handle input per phase
    if(phase==PHASE_SHELF) handle_input_shelf();
    else if(phase==PHASE_ABOUT) handle_input_about();
    else if(phase==PHASE_INSERTING){
        // allow eject via B? Slot: hold MENU to eject. On SF2000 map SELECT+L/R? Keep simple: B cancels insert before launch
        if(btn_down(RETRO_DEVICE_ID_JOYPAD_B)){ phase=PHASE_SHELF; anim_t=0; }
    }

    // update shelf smooth
    shelf_update(&shelf, 1.0f/60.0f);

    // phase animation
    if(phase==PHASE_INSERTING){
        anim_t += 1.0f/60.0f;
        float seat = (resumed_boot? 1.0f : anim_t / SEATED_AT);
        if(seat>1) seat=1;
        render_inserting(seat);
        if(anim_t >= INSERT_S){
            // Slot waits for cart seated + core ready. We already set core buffers before animation,
            // so ready is immediate. In real hardware wait floor then launch.
            do_launch();
        }
    } else if(phase==PHASE_SHELF){
        render_shelf();
    } else if(phase==PHASE_ABOUT){
        render_about();
    } else if(phase==PHASE_PLAYING){
        // Should have been swapped out by WRAP_RUN_GAME; if still here render black
        render_fill(framebuffer,0,0,SCREEN_W,SCREEN_H,0x0000);
    }

    if(video_cb) video_cb(framebuffer, SCREEN_W, SCREEN_H, SCREEN_W*sizeof(uint16_t));

    if(game_queued){
        game_queued=false;
        FILE *tr=fopen(game_name_buf,"rb"); leapui_log("ROM check %s exists=%d", game_name_buf, tr!=NULL); if(tr) fclose(tr);
        FILE *tnew=fopen("/media/mmcblk0p2/system/Phobos/cores/gpSP.mars","rb"); leapui_log("CORE check gpSP.mars (system/Phobos, new) exists=%d", tnew!=NULL); if(tnew) fclose(tnew);
        FILE *told=fopen("/media/mmcblk0p2/HCRTOS/cores/gpSP.hcrtos","rb"); leapui_log("CORE check gpSP.hcrtos (HCRTOS, legacy) exists=%d", told!=NULL); if(told) fclose(told);
        leapui_log("WRAP_RUN_GAME core=%s rom=%s", core_name_buf, game_name_buf);
        WRAP_RUN_GAME("/media/mmcblk0p2/ROMS/menu;m.gba", 0);
        leapui_log("WRAP_RUN_GAME returned");
        return;
    }
    // After launch, the frontend will have swapped core to gpsp. No need to handle return.
}
