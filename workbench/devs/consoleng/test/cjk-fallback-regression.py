#!/usr/bin/env python3
"""Actual CJK fallback mask and engine routing, bounds, failures and ownership."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'consolengglyph.c').read_text();s=s[s.index('static BOOL ConsolengCJKCodepoint('):s.index('/****** drawing')]
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef uint32_t ULONG;typedef uint64_t UQUAD;typedef uint8_t UBYTE;typedef uintptr_t IPTR;typedef int BOOL;typedef int32_t LONG;typedef void*PLANEPTR;typedef const char*CONST_STRPTR;
#define TRUE 1
#define FALSE 0
#define MEMF_ANY 0
#define OT_Level1 4096
#define OT_Spec1 1
#define OT_Indirect 128
#define OT_DeviceDPI 2
#define OT_PointHeight 3
#define OT_GlyphCode 4
#define OT_GlyphMap 5
#define TAG_DONE 0
#define OTERR_Success 0
struct TagItem{ULONG ti_Tag;IPTR ti_Data;};struct Library{int x;};struct GlyphEngine{int x;};
struct GlyphMap{void*glm_BitMap;ULONG glm_BlackWidth,glm_BMRows,glm_BMModulo,glm_BlackLeft;};
struct TextFont{ULONG tf_XSize,tf_YSize,tf_Baseline;};struct RastPort{struct TextFont*Font;};
struct ConsoleBase{struct Library*cjkEngineBase;struct GlyphEngine*cjkEngine;BOOL cjkFailed;UBYTE*glyphMask;ULONG glyphMaskSize;};
static int live,fail_alloc,fail_lib,fail_engine,fail_set,missing,libs,engines,obtained,draws;static ULONG current_cp;
static struct Library lib;static struct GlyphEngine engine;static struct GlyphMap map;
static void*AllocVec(ULONG n,int flags){(void)flags;if(fail_alloc)return NULL;live++;return calloc(1,n);}
static void FreeVec(void*p){assert(p&&live);live--;free(p);}
static int con_cp_width(ULONG cp){return cp==0xff61?1:2;}
static struct Library*OpenLibrary(CONST_STRPTR n,int v){assert(!strcmp(n,"freetype2.library")&&!v);if(fail_lib)return NULL;libs++;return &lib;}
static void CloseLibrary(struct Library*p){assert(p==&lib&&libs);libs--;}
static struct GlyphEngine*OpenEngine(void){if(fail_engine)return NULL;engines++;return &engine;}
static void CloseEngine(struct GlyphEngine*p){assert(p==&engine&&engines);engines--;}
static ULONG SetInfoA(struct GlyphEngine*p,struct TagItem*t){assert(p==&engine);if(fail_set)return 1;if(t[0].ti_Tag==OT_PointHeight){current_cp=t[1].ti_Data;assert(t[1].ti_Tag==OT_GlyphCode);}else { assert(!strcmp((char*)t[0].ti_Data,"FONTS:TrueType/NotoSansCJK-Regular.ttc"));assert(t[1].ti_Tag==(OT_Level1|0x104)&&t[1].ti_Data==1); }return 0;}
static void ObtainInfoA(struct GlyphEngine*p,struct TagItem*t){assert(p==&engine&&t[0].ti_Tag==OT_GlyphMap);*(struct GlyphMap**)t[0].ti_Data=missing?NULL:&map;if(!missing)obtained++;}
static void ReleaseInfoA(struct GlyphEngine*p,struct TagItem*t){assert(p==&engine&&t[0].ti_Data==(IPTR)&map&&obtained==1);obtained--;}
static void BltTemplate(PLANEPTR p,int bit,ULONG pitch,struct RastPort*rp,LONG x,LONG y,ULONG w,ULONG h){assert(p&&!bit&&pitch>=((w+7)/8));assert(x>=100&&y>=200&&x+w<=100+2*rp->Font->tf_XSize&&y+h<=200+rp->Font->tf_YSize);draws++;}
'''
main=r'''
static void cleanup(struct ConsoleBase*d){if(d->cjkEngine)CloseEngine(d->cjkEngine);if(d->cjkEngineBase)CloseLibrary(d->cjkEngineBase);if(d->glyphMask)FreeVec(d->glyphMask);memset(d,0,sizeof(*d));assert(!live&&!libs&&!engines&&!obtained);}
int main(void){struct ConsoleBase d={0};unsigned cases=0;
 for(ULONG bw=1;bw<=65;bw+=4)for(ULONG bh=1;bh<=65;bh+=4)for(ULONG left=0;left<8;left++)for(ULONG cell=8;cell<=32;cell*=2){
  ULONG stride=(left+bw+7)/8;UBYTE *bits=calloc(stride,bh);assert(bits);
  for(ULONG y=0;y<bh;y++)for(ULONG x=0;x<bw;x++)if((x*3+y)%5<2)bits[y*stride+((left+x)>>3)]|=0x80>>((left+x)&7);
  map=(struct GlyphMap){bits,bw,bh,stride,left};ULONG w,h,pitch;assert(ConsolengCJKMask(&d,&map,cell*2,cell,&w,&h,&pitch));assert(w&&h&&w<=cell*2&&h<=cell&&w<=bw&&h<=bh);
  for(ULONG y=0;y<h;y++)for(ULONG x=0;x<pitch*8;x++){int want=x<w&&(((x*bw/w)*3+y*bh/h)%5<2);assert(!!(d.glyphMask[y*pitch+x/8]&(0x80>>(x%8)))==want);}
  free(bits);cases++;
 }
 cleanup(&d);UBYTE bits[32*32];memset(bits,255,sizeof(bits));map=(struct GlyphMap){bits,32,32,4,0};
 for(int fault=0;fault<7;fault++){
  struct TextFont font={8,16,12};struct RastPort rp={&font};
  fail_lib=fault==1;fail_engine=fault==2;fail_set=fault==3;missing=fault==4;fail_alloc=fault==5;draws=0;
  if(fault==6)map.glm_BlackWidth=0;
  assert(ConsolengCJKDraw(NULL,&d,&rp,0x4e2d,100,212)==(fault==0));assert(!obtained);assert(draws==(fault==0));
  if(fault>=1&&fault<=3){assert(d.cjkFailed);fail_lib=fail_engine=fail_set=0;assert(!ConsolengCJKDraw(NULL,&d,&rp,0x4e2d,100,212));}
  cleanup(&d);map.glm_BlackWidth=32;
 }
 fail_alloc=missing=0;
 struct TextFont font={8,16,12};struct RastPort rp={&font};
 const ULONG rejected[]={0x41,0x3a9,0x1f431,0xff61,0x10ffff};
 for(unsigned i=0;i<sizeof(rejected)/sizeof(*rejected);i++){assert(!ConsolengCJKDraw(NULL,&d,&rp,rejected[i],100,212));assert(!libs&&!engines);}
 const ULONG covered[]={0x1100,0x4e2d,0x65e5,0x8a9e,0xac00,0xd55c,0xf900,0xfe10,0xff01,0x20000};
 for(unsigned i=0;i<sizeof(covered)/sizeof(*covered);i++){assert(ConsolengCJKDraw(NULL,&d,&rp,covered[i],100,212));assert(current_cp==covered[i]&&libs==1&&engines==1&&!obtained);}
 cleanup(&d);printf("CJK PASS: %u mask cases, fixed cell bounds, pitch/offset, engine reuse, missing fonts/maps, OOM and map ownership\n",cases);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';e=Path(d)/'t';p.write_text(prefix+s+main)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(e)],check=True);subprocess.run([str(e)],check=True)
 for label,broken in [('ignored bit origin',s.replace('gm->glm_BlackLeft + (UQUAD)x','0 + (UQUAD)x')),('bad map release',s.replace('(IPTR)gm}', '(IPTR)&gm}'))]:
  p.write_text(prefix+broken+main);subprocess.run(['cc','-Wall','-Werror',str(p),'-o',str(e)],check=True);assert subprocess.run([str(e)],capture_output=True).returncode!=0
  print('Negative control rejected:',label)
