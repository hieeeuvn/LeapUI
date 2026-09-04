#include "render.h"
#include "font.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

uint16_t rgb_hex(uint32_t h){ uint8_t r=(h>>16)&0xFF,g=(h>>8)&0xFF,b=h&0xFF; return (r>>3)<<11|(g>>2)<<5|(b>>3); }
void render_clear(uint16_t *fb){ uint16_t c = C_BG_TOP; // nen dong nhat 1 mau (bo band xanh nua duoi)
    for(int y=0;y<SCREEN_H;y++) for(int x=0;x<SCREEN_W;x++) fb[y*SCREEN_W+x]=c; }
void render_fill(uint16_t *fb,int x,int y,int w,int h,uint16_t c){ for(int py=y;py<y+h;py++) for(int px=x;px<x+w;px++) if(px>=0&&px<SCREEN_W&&py>=0&&py<SCREEN_H) fb[py*SCREEN_W+px]=c; }
void render_rect_border(uint16_t *fb,int x,int y,int w,int h,uint16_t c){
    render_fill(fb,x,y,w,1,c); render_fill(fb,x,y+h-1,w,1,c);
    render_fill(fb,x,y,1,h,c); render_fill(fb,x+w-1,y,1,h,c);
}
void render_header(uint16_t *fb){
    // top bar
    font_draw(fb,SCREEN_W,SCREEN_H, 6, 4, "LeapUI", C_BORDER);
    render_fill(fb,0,14,SCREEN_W,1,C_BORDER);
}
void render_footer(uint16_t *fb){
    int y=SCREEN_H-14;
    render_fill(fb,0,y,SCREEN_W,1,C_BORDER);
    font_draw(fb,SCREEN_W,SCREEN_H,6,y+4,"A - Start Game",C_BORDER);
    const char *r="L/R - Switch Game";
    int tw=font_measure(r);
    font_draw(fb,SCREEN_W,SCREEN_H,SCREEN_W-tw-6,y+4,r,C_BORDER);
}
void render_backdrop(uint16_t *fb){
    render_clear(fb);
    render_header(fb);
    render_footer(fb);
    // chan mo hinh
}
static int anim_tick=0;
void render_center_banner(uint16_t *fb, Thumb *th, const char *label){
    anim_tick++;
    /* khung cart theo ti le sticker GBA 43:22 (508x260): long 196x100, banner 200x104 */
    int bw=200, bh=104;
    int bx=(SCREEN_W-bw)/2, by=40;
    // nhap nhay vien nhe
    uint16_t border = (anim_tick%60<30)? C_BORDER : 0x07E0;
    // animation truot nhe theo scroll
    int slide = (anim_tick%120<60)? 1 : -1;
    bx += slide;
    render_rect_border(fb,bx,by,bw,bh,border);
    if(th && th->data){
        thumb_draw_scaled(fb,th,bx+2,by+2,bw-4,bh-4);
    } else {
        const char *l1="GAMEBOY ADVANCED";
        int tw=font_measure(l1);
        font_draw(fb,SCREEN_W,SCREEN_H,bx+(bw-tw)/2, by+bh/2-6, l1, C_BORDER);
    }
    // ten rom - chi 1 dong 8px, cat gon
    if(label){
        char tmp[28]; strncpy(tmp,label,27); tmp[27]=0;
        // cat va chi hien 1 dong
        if(font_measure(tmp) > bw-8){
            tmp[18]='.'; tmp[19]='.'; tmp[20]='.'; tmp[21]=0;
        }
        int tw=font_measure(tmp);
        // box nho 10px cao, khong chiem nua man
        int tx=bx+(bw-tw)/2;
        int ty=by+bh+6;
        // nen mo cho chu de doc (cung mau nen -> chi con vien + chu)
        render_fill(fb, tx-2, ty-1, tw+4, 10, C_BG_TOP);
        render_rect_border(fb, tx-2, ty-1, tw+4, 10, C_BORDER);
        font_draw(fb,SCREEN_W,SCREEN_H,tx, ty, tmp, C_TEXT);
    }
}
void render_side_preview(uint16_t *fb,int side, Thumb *th, const char *label){
    int sw=32, sh=64;
    int sx = (side<0)? 2 : SCREEN_W-sw-2;
    int sy = (SCREEN_H-sh)/2;
    // animation nhe len xuong
    int bob = (anim_tick+ (side*20))%60;
    if(bob>30) bob=60-bob;
    sy += bob/10;
    render_rect_border(fb,sx,sy,sw,sh,C_BORDER);
    if(th && th->data){
        thumb_draw_scaled(fb,th,sx+2,sy+2,sw-4,sh-4);
    } else if(label){
        // chi hien ki tu dau, khong chu
        char c[2]={label[0],0};
        font_draw(fb,SCREEN_W,SCREEN_H,sx+12,sy+sh/2-4,c,C_DIM);
    }
}
bool thumb_load(const char *rom_path, Thumb *out){
    if(!rom_path||!out) return false;
    out->data=NULL; out->w=0; out->h=0;
    char tp[512];
    const char *sl=strrchr(rom_path,'/');
    if(!sl) return false;
    size_t dl=sl-rom_path;
    if(dl>=400) return false;
    strncpy(tp, rom_path, dl); tp[dl]=0;
    strcat(tp,"/.res/");
    const char *fn=sl+1;
    const char *dot=strrchr(fn,'.');
    size_t nl= dot? (size_t)(dot-fn): strlen(fn);
    strncat(tp, fn, nl);
    strcat(tp,".rgb565");
    FILE *fp=fopen(tp,"rb");
    if(!fp) return false;
    fseek(fp,0,SEEK_END); long sz=ftell(fp); fseek(fp,0,SEEK_SET);
    /* Kich thuoc chuan FrogUI (160x160/200x200, wide 250x200) + GBA label 43:22 (196x100/508x260...) + size khac */
    int known[][2]={{64,64},{128,128},{144,208},{160,160},{196,86},{196,100},{200,200},{220,120},{250,200},{320,240}};
    for(int i=0;i<10;i++){
        int w=known[i][0], h=known[i][1];
        if(sz==w*h*2){
            uint16_t *buf=(uint16_t*)malloc(sz);
            if(!buf){ fclose(fp); return false; }
            if((long)fread(buf,1,sz,fp)!=sz){ free(buf); fclose(fp); return false; }
            fclose(fp);
            out->data=buf; out->w=w; out->h=h; return true;
        }
    }
    fclose(fp); return false;
}
void thumb_free(Thumb *t){ if(t&&t->data){ free(t->data); t->data=NULL; t->w=0; t->h=0; } }
void thumb_draw_scaled(uint16_t *fb, Thumb *t,int x,int y,int w,int h){
    /* fit giu ty le (khong beo hinh), can giua trong o (w x h); pixel 0x0000 = trong suot */
    if(!fb||!t||!t->data) return;
    double k1=(double)w/t->w, k2=(double)h/t->h;
    double s = (k1<k2)? k1 : k2;
    int dw=(int)(t->w*s), dh=(int)(t->h*s);
    if(dw<1) dw=1; if(dh<1) dh=1;
    int ox=x+(w-dw)/2, oy=y+(h-dh)/2;
    for(int dy=0;dy<dh;dy++){
        int fy=oy+dy;
        if(fy<0||fy>=SCREEN_H) continue;
        int sy=(int)((dy*t->h)/(double)dh);
        for(int dx=0;dx<dw;dx++){
            int fx=ox+dx;
            if(fx<0||fx>=SCREEN_W) continue;
            int sx=(int)((dx*t->w)/(double)dw);
            uint16_t p=t->data[sy*t->w+sx];
            if(p!=0x0000) fb[fy*SCREEN_W+fx]=p;
        }
    }
}
