"""Actual firmware gifdec.c, synthetic valid GIF87a/89a; no private assets."""
from pathlib import Path
import subprocess,tempfile,json
root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as tmp:
 d=Path(tmp)
 (d/'esp_log.h').write_text('#pragma once\n#define ESP_LOGW(...) ((void)0)\n#define ESP_LOGE(...) ((void)0)\n')
 (d/'lvgl.h').write_text('''#pragma once
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <assert.h>
#define LV_USE_DRAW_SW_ASM 0
#define LV_DRAW_SW_ASM_HELIUM 99
#ifndef LV_GIF_CACHE_DECODE_DATA
#define LV_GIF_CACHE_DECODE_DATA 0
#endif
#define LV_FS_SEEK_SET 0
#define LV_FS_SEEK_CUR 1
#define LV_FS_MODE_RD 0
#define LV_FS_RES_OK 0
#define LV_ASSERT_NULL(x) assert(x)
typedef struct {int unused;} lv_fs_file_t;
typedef int lv_fs_res_t;
extern size_t live,peak;
static inline void* lv_malloc(size_t n){size_t*p=malloc(n+sizeof(size_t));if(!p)return NULL;*p=n;live+=n;if(live>peak)peak=live;return p+1;}
static inline void lv_free(void*p){if(p){size_t*q=(size_t*)p-1;live-=*q;free(q);}}
static inline void* lv_realloc(void*p,size_t n){if(!p)return lv_malloc(n);size_t*q=(size_t*)p-1;size_t old=*q;q=realloc(q,n+sizeof(size_t));if(!q)return NULL;*q=n;live=live-old+n;if(live>peak)peak=live;return q+1;}
static inline int lv_fs_open(lv_fs_file_t*f,const void*p,int m){(void)f;(void)p;(void)m;abort();}
static inline void lv_fs_read(lv_fs_file_t*f,void*p,size_t n,void*x){(void)f;(void)p;(void)n;(void)x;abort();}
static inline void lv_fs_seek(lv_fs_file_t*f,size_t p,int k){(void)f;(void)p;(void)k;abort();}
static inline void lv_fs_tell(lv_fs_file_t*f,uint32_t*p){(void)f;(void)p;abort();}
static inline void lv_fs_close(lv_fs_file_t*f){(void)f;abort();}
''')
 (d/'test.c').write_text(r'''
#include "gifdec.h"
#include <assert.h>
#include <stdio.h>
size_t live=0,peak=0;
int main(){
 unsigned char data[]={ 'G','I','F','8','9','a',1,0,1,0,0x80,0,0,0,0,0,255,255,255,0x2c,0,0,0,0,1,0,1,0,0,2,2,0x44,0x01,0,0x3b };
 for(int i=0;i<300;i++){
  data[4]=(i%2)?'7':'9';gd_GIF*g=gd_open_gif_data(data);assert(g&&g->width==1&&g->height==1);
  assert(gd_get_frame(g)==1);gd_render_frame(g,g->canvas);assert(gd_get_frame(g)==0);gd_rewind(g);assert(gd_get_frame(g)==1);gd_close_gif(g);assert(live==0);
 }
 printf("{\"cycles\":300,\"peak_bytes\":%zu,\"retained_bytes\":%zu}\n",peak,live);
}
''')
 for cache in [0,1]:
  b=d/f'gif-{cache}'
  subprocess.run(['clang','-std=c11','-fsanitize=address,undefined','-g',f'-DLV_GIF_CACHE_DECODE_DATA={cache}',f'-I{d}',f'-I{root}/main/display/lvgl_display/gif',str(root/'main/display/lvgl_display/gif/gifdec.c'),str(d/'test.c'),'-o',str(b)],check=True,capture_output=True,text=True)
  p=subprocess.run([str(b)],check=True,capture_output=True,text=True)
  print('cache',cache,p.stdout.strip())
