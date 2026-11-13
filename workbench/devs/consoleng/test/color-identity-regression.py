#!/usr/bin/env python3
"""Exercise actual cell allocation and logical-color remapping under sanitizers."""
from pathlib import Path
from hyperlink_fixture import hyperlink_source
import subprocess, tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'charmap.c').read_text()
allocation=s[s.index('struct charmap_line *charmap_dispose_line('):s.index('/* Move cell ownership')]
s=(root/'charmapconclass.c').read_text()
remap='static VOID charmap_remap_colors('+s.split('static VOID charmap_remap_colors(',1)[1].split('\n/* Cells keep',1)[0]
prefix=r'''
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
typedef uint64_t UQUAD;
typedef void VOID; typedef uint32_t ULONG; typedef uint16_t UWORD;
typedef signed char BYTE; typedef unsigned char UBYTE;
struct ConsoleBase { int unused; };
struct charmap_line {struct con_link **links; struct charmap_line *next,*prev; ULONG size,capacity; ULONG *text; BYTE *fgpen,*bgpen,*flags; UQUAD *roles;ULONG *underlineColors;  unsigned long wrapColumns; unsigned char wrapPadding;};
#define MEMF_ANY 0
#define CHARMAP_COLORPENS 34
#define SetMem(p,v,n) memset(p,v,n)
static int attempt,fail,live;
static void *allocate(size_t n) { if (++attempt==fail) return NULL; void *p=malloc(n); if(p)++live; return p; }
static void release(void *p) { if(p)--live; free(p); }
#define AllocMem(n,f) allocate(n)
#define FreeMem(p,n) ((void)(n),release(p))
'''
header=(root/'console_gcc.h').read_text()
prefix += header[header.index('#define CONUNIT_PEN_MAX'):header.index('#define CON_INPUTBUF_SIZE')]
prefix += next(line for line in header.splitlines() if line.startswith('#define CON_COLOR_ROLE'))+'\n'
prefix += header[header.index('#define CON_RETAINED_ROLE'):header.index('/* The conFlags */')]
prefix += r'''
typedef struct {ULONG fgColor,bgColor;unsigned char underlineStyle;} Object;
#define ICU(o) (o)
static ULONG resolved[2];static int resolves;
static void Console_GetColorPen(Object *o,ULONG key,UBYTE *out){(void)o;resolved[resolves++ % 2]=key;*out=42;}
'''
resolve=s[s.index('static VOID charmap_resolve_rgb('):s.index('/* A reflowed hard row')]
main=r'''
int main(void) {
 assert(CON_COLOR_ROLE(CONUNIT_PEN_DEFFG)==1 && CON_COLOR_ROLE(CONUNIT_PEN_DEFBG)==2);
 for(ULONG i=0;i<8;i++)assert(CON_COLOR_ROLE(i)==3+i);
 for(ULONG i=0;i<256;i++)assert(CON_COLOR_ROLE(CONPEN_XTERM(i))==(i<16?11+i:0));
 for(ULONG i=0;i<8;i++)assert(CON_COLOR_ROLE(CONPEN_BRIGHT(i))==27+i);
 assert(CON_COLOR_ROLE(CONPEN_RGB(32,62,76))==0);
 struct charmap_line *line=charmap_newline(NULL,NULL); assert(line);
 charmap_resize(NULL,line,4); assert(line->size==4 && live==6);
 for(int i=0;i<4;i++){line->text[i]='A'+i;line->roles[i]=0x0102+i;}
 for(int failure=1;failure<=5;failure++){
  struct charmap_line old=*line; attempt=0;fail=failure;
  charmap_resize(NULL,line,80);
  assert(!memcmp(&old,line,sizeof(old)) && live==6);
  for(int i=0;i<4;i++)assert(line->text[i]=='A'+(ULONG)i && line->roles[i]==0x0102+i);
 }
 fail=0;charmap_resize(NULL,line,80);assert(line->size==80 && live==6);
 for(int i=0;i<4;i++)assert(line->roles[i]==0x0102+i);
 for(int i=4;i<80;i++)assert(!line->roles[i]);
 charmap_resize(NULL,line,2); charmap_resize(NULL,line,80);
 for(int i=2;i<80;i++)assert(!line->roles[i]);
 UBYTE map[256],slots[34];for(int i=0;i<256;i++)map[i]=99;
 for(int i=0;i<34;i++)slots[i]=128+i;
 line->size=1;
 for(int style=0;style<4;style++)for(int fg=0;fg<=34;fg++)for(int bg=0;bg<=34;bg++){
  line->fgpen[0]=line->bgpen[0]=7;line->roles[0]=(fg<<8)|bg|((style&1)<<6)|((style&2)<<13);
  charmap_remap_colors(line,map,slots);
  assert((UBYTE)line->fgpen[0]==(fg?slots[fg-1]:7));
  assert((UBYTE)line->bgpen[0]==(bg?slots[bg-1]:7));
 }
 for(int style=0;style<4;style++)for(int i=0;i<1000;i++){
  Object state={CONPEN_RGB(i&255,(i>>2)&255,(i*7)&255),CONPEN_RGB((i*3)&255,(i*11)&255,(i*13)&255)};
  state.underlineStyle=style;
  UQUAD expected=CHARMAP_ROLES(&state);line->roles[0]=expected;
  charmap_resize(NULL,line,160);assert(line->roles[0]==expected);
  charmap_remap_colors(line,map,slots);assert(line->roles[0]==expected);
  resolves=0;charmap_resolve_rgb(&state,line,0);
  assert(resolves==2&&resolved[0]==state.fgColor&&resolved[1]==state.bgColor);
  charmap_resize(NULL,line,1);assert(line->roles[0]==expected);
 }
 for(int style=0;style<4;style++)for(int fg=16;fg<256;fg++)for(int bg=16;bg<256;bg++){
  Object state={CONPEN_XTERM(fg),CONPEN_XTERM(bg)};state.underlineStyle=style;line->roles[0]=CHARMAP_ROLES(&state);
  assert((line->roles[0]&0xbfbf)==0x8181);
  charmap_remap_colors(line,map,slots);resolves=0;charmap_resolve_rgb(&state,line,0);
  assert(resolves==2&&resolved[0]==state.fgColor&&resolved[1]==state.bgColor);
 }
 for(int style=0;style<4;style++)for(int index=16;index<256;index++)for(int reverse=0;reverse<2;reverse++){
  ULONG palette=CONPEN_XTERM(index),rgb=CONPEN_RGB(index,index^0x55,index^0xaa);
  Object state={reverse?palette:rgb,reverse?rgb:palette};
  state.underlineStyle=style;
  UQUAD expected=CHARMAP_ROLES(&state);line->roles[0]=expected;
  charmap_resize(NULL,line,2);assert(line->roles[0]==expected);
  charmap_remap_colors(line,map,slots);resolves=0;charmap_resolve_rgb(&state,line,0);
  assert(resolves==2&&resolved[0]==state.fgColor&&resolved[1]==state.bgColor);
  charmap_resize(NULL,line,1);assert(line->roles[0]==expected);
 }
 charmap_resize(NULL,line,0);assert(live==1 && !line->roles);
 charmap_dispose_line(line);assert(!live);return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';b=Path(d)/'test';p.write_text(prefix+hyperlink_source()+allocation+remap+resolve+main)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
 subprocess.run([str(b)],check=True)
print('Color identity PASS: all four decoration states across 1225 aliased/fixed role pairs, five transactional allocation failures, growth/shrink/disposal, 1000 retained RGB pairs and 57600 extended palette pairs and 480 mixed RGB/index pairs')
