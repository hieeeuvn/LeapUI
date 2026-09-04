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

int shelf_scan(LeapShelf *s, const char *gba_dir){
    s->count=0;
    DIR *d=opendir(gba_dir);
    if(!d) return -1;
    struct dirent *e;
    while((e=readdir(d))!=NULL && s->count < MAX_ROMS){
        if(e->d_name[0]=='.') continue;
        // only .gba
        const char *dot=strrchr(e->d_name,'.');
        if(!dot) continue;
        if(strcasecmp(dot,".gba")!=0) continue;
        LeapCart *c=&s->carts[s->count];
        snprintf(c->path, sizeof(c->path), "%s/%s", gba_dir, e->d_name);
        strncpy(c->name, e->d_name, sizeof(c->name)-1);
        // stem without ext
        size_t len=strlen(e->d_name);
        size_t ext=4;
        size_t slen = len>ext? len-ext : len;
        strncpy(c->stem, e->d_name, slen);
        c->stem[slen]=0;
        s->count++;
    }
    closedir(d);
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
