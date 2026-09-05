#include "shelf.h"
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>

void shelf_init(LeapShelf *s){ memset(s,0,sizeof(*s)); }

static int cmp_cart(const void *a,const void *b){
    const LeapCart *ca=a,*cb=b;
    return strcasecmp(ca->name, cb->name);
}

/* Recursively collect *.gba (flat shelf, subfolders included) up to MAX_ROMS total.
   Entry names are copied BEFORE any nested readdir because the device readdir uses
   a static buffer. Directories are probed with opendir() (no stat needed). */
static void scan_dir_r(LeapShelf *s, const char *dir_path, int depth){
    if(depth > 4) return;
    DIR *d=opendir(dir_path);
    if(!d) return;
    struct dirent *e;
    while((e=readdir(d))!=NULL){
        if(s->count >= MAX_ROMS) break;
        if(e->d_name[0]=='.') continue;   // hidden files and .res/
        char name[256];
        strncpy(name, e->d_name, sizeof(name)-1); name[sizeof(name)-1]=0;
        if(strcasecmp(name,"saves")==0 || strcasecmp(name,"save")==0) continue;
        s->scanned++;
        const char *dot=strrchr(name,'.');
        if(dot && strcasecmp(dot,".gba")==0){
            LeapCart *c=&s->carts[s->count];
            snprintf(c->path, sizeof(c->path), "%s/%s", dir_path, name);
            strncpy(c->name, name, sizeof(c->name)-1);
            // stem without ext
            size_t len=strlen(name);
            size_t ext=4;
            size_t slen = len>ext? len-ext : len;
            strncpy(c->stem, name, slen);
            c->stem[slen]=0;
            s->count++;
        } else {
            // recurse into subdirectories (opendir is the is-dir probe)
            char full[MAX_PATH];
            snprintf(full, sizeof(full), "%s/%s", dir_path, name);
            DIR *sub=opendir(full);
            if(sub){ scan_dir_r(s, full, depth+1); closedir(sub); }
        }
    }
    closedir(d);
}

int shelf_scan(LeapShelf *s, const char *gba_dir){
    s->count=0; s->scanned=0;
    DIR *probe=opendir(gba_dir);
    if(!probe) return -1;
    closedir(probe);
    scan_dir_r(s, gba_dir, 0);
    qsort(s->carts, s->count, sizeof(LeapCart), cmp_cart);
    s->index=0; s->scroll=0; s->vel=0;
    return s->count;
}

void shelf_move(LeapShelf *s, int dir){
    if(s->count==0) return;
    int n=s->count;
    int new_idx = s->index + dir;
    // wrap like Slot carousel infinite
    if(new_idx<0) new_idx=n-1;
    if(new_idx>=n) new_idx=0;
    s->index=new_idx;
}

void shelf_update(LeapShelf *s,float dt){
    float target=(float)s->index;
    float diff=target - s->scroll;
    // spring towards target
    float k=14.0f; // stiffness
    float d=8.0f;  // damping
    s->vel += diff*k*dt;
    s->vel *= (1.0f - d*dt*0.15f);
    if(s->vel>10) s->vel=10; if(s->vel<-10) s->vel=-10;
    s->scroll += s->vel*dt*8.0f;
    // snap if close
    if(fabsf(target - s->scroll) < 0.001f && fabsf(s->vel) < 0.01f){
        s->scroll=target; s->vel=0;
    }
    // handle wrap jump: if scroll far from target due to wrap, snap
    if(fabsf(diff) > s->count/2.0f){
        s->scroll=target; s->vel=0;
    }
}

const LeapCart* shelf_current(LeapShelf *s){
    if(s->count==0) return NULL;
    return &s->carts[s->index];
}

void shelf_set_index(LeapShelf *s,int idx){
    if(idx<0) idx=0; if(idx>=s->count) idx=s->count-1;
    s->index=idx; s->scroll=(float)idx; s->vel=0;
}
