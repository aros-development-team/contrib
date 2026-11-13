#!/usr/bin/env python3
"""Actual retained EL implementation: wide pairs and truncated short lines."""
from pathlib import Path
from hyperlink_fixture import hyperlink_source
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'charmapconclass.c').read_text()
helper='static VOID charmap_repair_wide_cells('+s.split('static VOID charmap_repair_wide_cells(',1)[1].split('static BOOL charmap_delete_char(',1)[0]
el='static VOID charmap_erase_line('+s.split('static VOID charmap_erase_line(',1)[1].split('/* DoCommand restores',1)[0]
copy=s[s.index('static ULONG charmap_copy_columns('):s.index('static VOID charmap_selection_span(')]
support=(r/'support.c').read_text();width='WORD con_cp_width('+support.split('WORD con_cp_width(',1)[1].split('/* Cells (columns)',1)[0]
prefix=r'''
#define CHARMAP_ERASED 0x21
#include <assert.h>
#define CHARMAP_ROLES(o) 0x0102
#include <stdint.h>
#include <string.h>
typedef void VOID;typedef void Class;typedef intptr_t IPTR;
typedef unsigned long ULONG;typedef short WORD;
struct charmap_line {ULONG size;ULONG *text;signed char *fgpen,*bgpen,*flags;unsigned long long *roles; unsigned long wrapColumns; unsigned char wrapPadding;struct con_link **links;ULONG *underlineColors;};
typedef struct {int max,fg,bg,flags;} Object;
static struct charmap_line fixture;
#define CU(o) (o)
#define cu_FgPen fg
#define cu_BgPen bg
#define cu_TxFlags flags
#define CHAR_XMAX(o) ((o)->max)
#define CON_IS_UTF8(o) 1
#define CON_TEXT_IS_UTF8(o) CON_IS_UTF8(o)
#define CELL_WIDE_CONT 0xFFFFFFFEUL
#define MIN(a,b) ((a)<(b)?(a):(b))
#define SetMem(p,v,n) memset(p,v,n)
#define charmapcon_find_line(cl,o,y) (&fixture)
#define charmap_resize(base,line,n) ((line)->size=(n))
'''
main=r'''
int main(void) {
 Object o={7,4,6,0};
 for(int initial=0;initial<=8;initial++)for(int pair=-1;pair<(initial?initial-1:0);pair++)
 for(int x=0;x<8;x++)for(int mode=0;mode<3;mode++) {
  ULONG text[8],expected[8];signed char fg[8],bg[8],flags[8];unsigned long long roles[8]={0};
  for(int i=0;i<8;i++){text[i]='A'+i;expected[i]=i<initial?text[i]:' ';fg[i]=1;bg[i]=2;flags[i]=0;}
  if(pair>=0){text[pair]=expected[pair]=0x2615;text[pair+1]=expected[pair+1]=CELL_WIDE_CONT;}
  int lo=mode==0?x:0,hi=mode==1?x+1:8;
  for(int i=lo;i<hi;i++)expected[i]=' ';
  if(pair>=0 && lo<pair+2 && hi>pair)expected[pair]=expected[pair+1]=' ';
  fixture=(struct charmap_line){initial,text,fg,bg,flags,roles};
  charmap_erase_line(0,&o,x,0,mode);
  assert(fixture.size==8);
  assert(charmap_copy_columns(&fixture)<=8);
  if(mode==2)assert(charmap_copy_columns(&fixture)==0);
  for(int i=lo;i<hi;i++){assert(bg[i]==6&&fg[i]==4&&flags[i]==CHARMAP_ERASED&&roles[i]==0x0102);}
  for(int i=0;i<8;i++)assert((i<fixture.size?text[i]:' ')==expected[i]);
 }
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';b=Path(d)/'test';p.write_text(prefix+hyperlink_source()+width+helper+el+copy+main)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
 subprocess.run([str(b)],check=True)
print('EL PASS: all modes, short lines, cursor positions and valid wide-pair positions')
