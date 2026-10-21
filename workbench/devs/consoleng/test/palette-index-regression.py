#!/usr/bin/env python3
"""Actual OSC color parser and dispatcher reject overflow and malformed tokens."""
from pathlib import Path
import subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/'stdconclass.c').read_text()
colors=s[s.index('static LONG stdcon_hexval('):s.index('static ULONG stdcon_penrgb(')]
code=s[s.index('static BOOL stdcon_colorindex('):s.index('/* Reserve every replacement')]
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
typedef uint8_t UBYTE;typedef uint16_t UWORD;typedef int16_t WORD;typedef uint32_t ULONG;typedef int32_t LONG;typedef intptr_t IPTR;typedef int BOOL;typedef void VOID;typedef void Class;
#define TRUE 1
#define FALSE 0
#define CONPEN_RGB(r,g,b) (0x01000000u|((r)<<16)|((g)<<8)|(b))
#define CONUNIT_PEN_DEFFG 0
#define CONUNIT_PEN_DEFBG 1
struct stdcondata {ULONG cursorRGB,resetCursorRGB;void *cm;ULONG defrgb[2],ansirgb[256];LONG defpen[2],ansipen[256];};
typedef struct {struct stdcondata data;ULONG extendedPaletteRevision;} Object;
#define ICU(o) (o)
#define INST_DATA(cl,o) (&(o)->data)
#define Console_GetColorPen(o,key,out) (*(out)=(key))
#define Console_UnRenderCursor(o) ((void)0)
#define Console_RenderCursor(o) ((void)0)
static unsigned changes,queries;static LONG lastquery;static ULONG queryrgb;
static ULONG stdcon_penrgb(struct stdcondata*d,UBYTE pen){(void)d;return pen;}
static ULONG stdcon_slotrgb(struct stdcondata*d,UWORD n){(void)d;assert(n<=255);return n;}
static void stdcon_colorreply(Class*c,Object*o,ULONG code,LONG index,ULONG rgb,BOOL st){(void)c;(void)o;(void)code;(void)rgb;(void)st;assert(index>=-1&&index<=255);lastquery=index;queryrgb=rgb;queries++;}
static void stdcon_setslot(struct stdcondata*d,ULONG*rgb,LONG*pen,ULONG value){(void)d;*rgb=value;*pen=value?1:-1;changes++;}
static void stdcon_followdefault(Object*o,UBYTE a,UBYTE b,UBYTE c,UBYTE d){(void)o;(void)a;(void)b;(void)c;(void)d;}
'''
main=r'''
static void run(Object*o,int command,const char*s){IPTR p[]={command,(IPTR)s,0};stdcon_setcolor(NULL,o,p);}
int main(void){
 char token[300];ULONG got;
 for(unsigned n=0;n<10000;n++){snprintf(token,sizeof(token),"%u",n);got=999;assert(stdcon_colorindex((UBYTE*)token,(UBYTE*)token+strlen(token),&got)==(n<=255));assert(got==(n<=255?n:999));}
 const char*bad[]={"","-1","+1","1x","1 2","256","2147483648","4294967296","18446744073709551616","99999999999999999999999999999999999999999999"};
 Object o={0};o.data.cm=&o;
 for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++){
  got=999;assert(!stdcon_colorindex((UBYTE*)bad[i],(UBYTE*)bad[i]+strlen(bad[i]),&got)&&got==999);
  for(unsigned n=0;n<16;n++){o.data.ansirgb[n]=0x11223344;o.data.ansipen[n]=1;}
  struct stdcondata before=o.data;changes=queries=0;
  snprintf(token,sizeof(token),"%s;#123456",bad[i]);run(&o,4,token);assert(!changes&&!queries&&!memcmp(&before,&o.data,sizeof(before)));
  /* Empty OSC104 intentionally resets all; malformed nonempty tokens do not. */
  if(*bad[i]){run(&o,104,bad[i]);assert(!changes&&!memcmp(&before,&o.data,sizeof(before)));}
  snprintf(token,sizeof(token),"%s;#123456;2;#123456",bad[i]);run(&o,4,token);assert(changes==1&&o.data.ansirgb[2]==0x01123456);
 }
 changes=0;run(&o,104,"1x;2");assert(changes==1&&!o.data.ansirgb[2]);
 changes=0;run(&o,104,"4294967296;0003");assert(changes==1&&!o.data.ansirgb[3]);
 queries=0;run(&o,4,"999999999999999999999999;?;255;?");assert(queries==1&&lastquery==255);
 changes=0;run(&o,104,"");assert(changes==256);
 /* Default resets independently release just the requested override. */
 for(int order=0;order<2;order++)for(int repeat=0;repeat<3;repeat++){
  run(&o,10,"#123456;#abcdef");run(&o,4,"3;#987654");
  ULONG other=o.data.defrgb[!order];LONG otherpen=o.data.defpen[!order];
  run(&o,110+order,"");
  assert(!o.data.defrgb[order]&&o.data.defpen[order]==-1);
  assert(o.data.defrgb[!order]==other&&o.data.defpen[!order]==otherpen);
  assert(o.data.ansirgb[3]==0x01987654&&o.data.ansipen[3]==1);
  run(&o,110+order,"");assert(!o.data.defrgb[order]&&o.data.defpen[order]==-1);
  run(&o,111-order,"");assert(!o.data.defrgb[0]&&!o.data.defrgb[1]);
 }
 run(&o,12,"#123456");assert(o.data.cursorRGB==0x01123456);
 queries=0;run(&o,12,"?");assert(queries==1&&o.data.cursorRGB==0x01123456);
 run(&o,12,"#zzzzzz");assert(o.data.cursorRGB==0x01123456);
 run(&o,112,"");assert(!o.data.cursorRGB);
 run(&o,10,"#102030;#405060;?");assert(queryrgb==0x01102030);
 run(&o,10,"#112233;#445566;#778899");assert(o.data.cursorRGB==0x01778899);
 assert(o.data.defrgb[0]==0x01112233&&o.data.defrgb[1]==0x01445566);
 puts("Default resets PASS: independent slots, repeated reset, ANSI preservation");
 for(unsigned n=0;n<256;n++){
  char command[60];ULONG revision=o.extendedPaletteRevision;
  snprintf(command,sizeof(command),"%u;#234567",n);run(&o,4,command);
  assert(o.data.ansirgb[n]==0x01234567&&o.data.ansipen[n]==1);
  assert(o.extendedPaletteRevision==revision+(n>=16));
  run(&o,4,command);assert(o.extendedPaletteRevision==revision+(n>=16));
  snprintf(command,sizeof(command),"%u",n);run(&o,104,command);
  assert(!o.data.ansirgb[n]&&o.data.ansipen[n]==-1);
  assert(o.extendedPaletteRevision==revision+2*(n>=16));
 }
 puts("Extended palette PASS: all256 set/reset slots, no-op revision stability");
 for(unsigned n=0;n<256;n++){
  snprintf(token,sizeof(token),"%u;#%06x",n,0x234500+n);run(&o,4,token);
 }
 for(unsigned n=0;n<256;n++)assert(o.data.ansirgb[n]==0x01234500+n);
 for(unsigned n=0;n<256;n+=2){snprintf(token,sizeof(token),"%u",n);run(&o,104,token);}
 for(unsigned n=0;n<256;n++)assert(o.data.ansirgb[n]==(n%2?0x01234500+n:0));
 run(&o,104,"");
 for(unsigned n=0;n<256;n++)assert(!o.data.ansirgb[n]&&o.data.ansipen[n]==-1);
 puts("Full logical palette PASS:256 simultaneous overrides, independent alternate resets, reset-all");
 const char*invalid[]={"rgb:1/2/3/4","rgb:1/2/3/","rgb:/2/3","rgb:1//3","rgb:1/2/","rgb:1/2","rgb:11111/2/3","rgb:1/2/g","#12","#12345"};
 for(unsigned i=0;i<sizeof(invalid)/sizeof(*invalid);i++)assert(!stdcon_parsecolor((UBYTE*)invalid[i],strlen(invalid[i])));
 assert(stdcon_parsecolor((UBYTE*)"rgb:1/2/3",9)==0x01112233);
 assert(stdcon_parsecolor((UBYTE*)"#123456",7)==0x01123456);
 /* Legacy # components preserve most significant bits; rgb: scales. */
 for(int width=1;width<=4;width++){
  unsigned maximum=(1u<<(4*width))-1;
  for(unsigned value=0;value<=maximum;value++){
   char spec[32];unsigned expected=width==1?value*16:value>>(4*(width-2));
   snprintf(spec,sizeof(spec),"#%0*x%0*x%0*x",width,value,width,value,width,value);
   assert(stdcon_parsecolor((UBYTE*)spec,strlen(spec))==CONPEN_RGB(expected,expected,expected));
   snprintf(spec,sizeof(spec),"rgb:%x/%x/%x",value,value,value);
   unsigned digits=value<16?1:value<256?2:value<4096?3:4;
   unsigned scaled=255*value/((1u<<(4*digits))-1);
   assert(stdcon_parsecolor((UBYTE*)spec,strlen(spec))==CONPEN_RGB(scaled,scaled,scaled));
  }
 }
 assert(stdcon_parsecolor((UBYTE*)"#123",4)==0x01102030);
 assert(stdcon_parsecolor((UBYTE*)"rgb:1/2/3",9)==0x01112233);
 puts("Color precision PASS:69904 component values in both hexadecimal and rgb formats");
 puts("Palette parser PASS:10000 indices, malformed/overflow rejection, valid-entry recovery, RGB component bounds");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';b=Path(d)/'t';p.write_text(prefix+colors+code+main)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True);subprocess.run([str(b)],check=True)
 broken=code.replace('if (value > (255 - digit) / 10) return FALSE;','/* missing range guard */')
 p.write_text(prefix+colors+broken+main)
 subprocess.run(['cc','-Wall','-Werror',str(p),'-o',str(b)],check=True)
 assert subprocess.run([str(b)],capture_output=True).returncode!=0
 print('Negative control PASS: missing range guard rejected')

 # Former scaling in legacy hexadecimal notation must fail independently.
 broken_colors=colors.replace('c[i] = n == 1 ? value << 4 : value >> (4 * (n - 2));','c[i] = stdcon_hexcomp(spec + 1 + i * n, n);')
 with tempfile.TemporaryDirectory() as d2:
  p=Path(d2)/'t.c';b=Path(d2)/'t';p.write_text(prefix+broken_colors+code+main)
  subprocess.run(['cc','-Wall','-Werror',str(p),'-o',str(b)],check=True)
  assert subprocess.run([str(b)],capture_output=True).returncode!=0
  print('Negative control PASS: former hexadecimal scaling rejected')

with tempfile.TemporaryDirectory() as d:
 p=Path(d)/"t.c";b=Path(d)/"t"
 # Resetting the wrong default must fail even when both slots are populated.
 broken_reset=code.replace('WORD slot = code - 110;', 'WORD slot = 111 - code;')
 p.write_text(prefix+colors+broken_reset+main)
 subprocess.run(['cc','-Wall','-Werror',str(p),'-o',str(b)],check=True)
 assert subprocess.run([str(b)],capture_output=True).returncode!=0
 print('Negative control PASS: swapped default reset slots rejected')
