#!/usr/bin/env python3
"""Execute actual palette transaction with failure at every allocation position."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'stdconclass.c').read_text();h=(r/'include/consoleng.h').read_text()
h=h[h.index('#define CD_GETCURSORPREFS'):h.index('#endif /* DEVICES_CONSOLENG_H */')]
a=s.index('static BOOL stdcon_colorprefs(');body=s[a:s.index('/* DECSCUSR pairs',a)]
source=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
typedef uint32_t ULONG;typedef int32_t LONG;typedef unsigned char UBYTE;typedef unsigned short UWORD;typedef int BOOL;typedef void Class;
#define CMD_NONSTD 9
#define TRUE 1
#define FALSE 0
#define CONUNIT_PEN_DEFFG 8
#define CONUNIT_PEN_DEFBG 9
struct Library {int unused;};
struct stdcondata {struct Library *scd_GfxBase;void *cm;ULONG schemeRGB,defrgb[2],ansirgb[16];LONG defpen[2],ansipen[16];UWORD *penmap[10];};
typedef struct {struct stdcondata data;UBYTE fg,bg;ULONG conFlags;} Object;
#define INST_DATA(cl,o) (&(o)->data)
#define ICU(o) (o)
#define CF_COLOR_SCHEME_NOTIFY 8
static void stdcon_schemereply(Class*cl,Object*o){assert(0 && "unnegotiated report");}
static int refs[256],allocation,failat;
static LONG stdcon_bestpen(struct stdcondata *d,int r,int g,int b){int n=allocation++;if(n==failat)return -1;refs[100+n]++;return 100+n;}
#define ReleasePen(cm,p) do{assert(GfxBase==0);assert(refs[p]>0);refs[p]--;}while(0)
static ULONG stdcon_penrgb(struct stdcondata *d,UBYTE p){return 0x01000000|p;}
static ULONG stdcon_slotrgb(struct stdcondata *d,UWORD n){return d->ansirgb[n]?d->ansirgb[n]:0x01000000|n;}
static void Console_GetColorPen(Object *o,int n,UBYTE *p){int i=n==9;*p=o->data.defpen[i]>=0?o->data.defpen[i]:*o->data.penmap[n];}
static void stdcon_followdefault(Object *o,UBYTE of,UBYTE ob,UBYTE nf,UBYTE nb){if(o->fg==of)o->fg=nf;if(o->bg==ob)o->bg=nb;}
'''+h+body+r'''
int main(void){
 for(int failure=0;failure<=18;failure++){
  Object o={0};UWORD baseline[10];o.data.cm=&o;memset(refs,0,sizeof(refs));
  for(int i=0;i<10;i++){baseline[i]=i;o.data.penmap[i]=&baseline[i];}
  for(int i=0;i<18;i++){if(i<2){o.data.defpen[i]=i+1;o.data.defrgb[i]=0x01000000|i;}else{o.data.ansipen[i-2]=i+1;o.data.ansirgb[i-2]=0x01000000|i;}refs[i+1]=1;}
  o.fg=1;o.bg=2;Object old=o;
  struct ConsolengColorPrefs p={84,1,CONSOLENG_COLOR_MASK,{0}};
  for(int i=0;i<18;i++)p.cp_RGB[i]=0x203040+i;
  allocation=0;failat=failure;
  int ok=stdcon_colorprefs(0,&o,&p,TRUE);assert(ok==(failure==18));
  if(!ok){assert(!memcmp(&o,&old,sizeof(o)));for(int i=0;i<18;i++)assert(refs[i+1]==1&&!refs[100+i]);}
  else {
   assert(o.fg==100&&o.bg==101);
   struct ConsolengColorPrefs got;assert(stdcon_colorprefs(0,&o,&got,FALSE));assert(!memcmp(&p,&got,sizeof(p)));
   p.cp_Mask=0;assert(stdcon_colorprefs(0,&o,&p,TRUE));assert(o.fg==8&&o.bg==9);
   assert(stdcon_colorprefs(0,&o,&got,FALSE)&&!got.cp_Mask&&got.cp_RGB[0]==8&&got.cp_RGB[1]==9);
   for(int i=0;i<256;i++)assert(!refs[i]);
  }
 }
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';exe=Path(d)/'test';p.write_text(source)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('Color prefs PASS: all18 allocation failures leave palette/refcounts unchanged; success/get/reset/current defaults')
