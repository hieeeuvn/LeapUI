#include "theme.h"
#include "render.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static uint16_t col_housing = C_HOUSING;
static uint16_t col_recess  = C_RECESS;
static uint16_t col_opening = C_OPENING;
static uint16_t col_edge    = C_EDGE;

static uint32_t parse_hex(const char *s){
    if(s[0]=='#') s++;
    return (uint32_t)strtoul(s,NULL,16);
}
void theme_load(const char *assets_dir){
    char path[512]; snprintf(path,sizeof(path),"%s/theme.txt", assets_dir);
    FILE *f=fopen(path,"r");
    if(!f) return;
    char line[128];
    while(fgets(line,sizeof(line),f)){
        char key[32], val[32];
        if(sscanf(line,"%31s %31s", key,val)!=2) continue;
        uint32_t hex=parse_hex(val);
        uint16_t c=rgb_hex(hex);
        if(strcmp(key,"housing")==0) col_housing=c;
        else if(strcmp(key,"recess")==0) col_recess=c;
        else if(strcmp(key,"opening")==0) col_opening=c;
        else if(strcmp(key,"edge")==0) col_edge=c;
    }
    fclose(f);
}
uint16_t theme_housing(void){ return col_housing; }
uint16_t theme_recess(void){ return col_recess; }
uint16_t theme_opening(void){ return col_opening; }
uint16_t theme_edge(void){ return col_edge; }
