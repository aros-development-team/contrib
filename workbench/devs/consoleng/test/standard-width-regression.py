#!/usr/bin/env python3
"""Exercise the private plain-console width map against independent cell identities."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
code=r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef uint8_t UBYTE;
typedef uint32_t ULONG;
typedef int32_t LONG;
typedef int16_t WORD;
typedef int BOOL;
#define TRUE 1
#define FALSE 0
#define MEMF_CLEAR 1
static int fail_alloc;
static void *AllocVec(size_t n,int flags){(void)flags;return fail_alloc?NULL:calloc(1,n);}
#define FreeVec free
'''+(root/'standard_widths.h').read_text()+r'''
int main(void){unsigned cases=0;
/* Enumerate every tiling of a short row with narrow cells and distinct wide
 * identities. Edit the identity array independently, then reject any fragment
 * whose original partner is missing. This catches accidental head/tail merges. */
for(unsigned layout=0;layout<256;layout++)for(int x=0;x<8;x++)for(int n0=1;n0<=10;n0++)for(int op=-1;op<=1;op++){
 UBYTE row[8]={0},expected[8]={0};int ids[8],answer[8];
 for(int i=0;i<8;i++){ids[i]=100+i;if(i<7&&(layout&(1u<<i))){row[i]=1;row[i+1]=2;ids[i]=i+1;ids[i+1]=-(i+1);i++;}}
 int n=n0>8-x?8-x:n0;
 for(int i=0;i<8;i++){int from=i;if(i>=x){if(op==1)from=i<x+n?-1:i-n;else if(op==-1)from=i+n<8?i+n:-1;else if(i<x+n)from=-1;}answer[i]=from<0?0:ids[from];}
 for(int i=0;i<8;i++){if(answer[i]>0&&answer[i]<100&&i+1<8&&answer[i+1]==-answer[i])expected[i]=1;if(answer[i]<0&&i>0&&answer[i-1]==-answer[i])expected[i]=2;}
 con_width_cut(row,8,x);con_width_cut(row,8,op==1?8-n:x+n);con_width_edit(row,8,x,n,op);
 assert(!memcmp(row,expected,8));cases++;
}
struct con_width_grid g={0};assert(!con_width_resize(&g,8,4,8,16,FALSE));assert(con_width_resize(&g,8,4,8,16,TRUE));
for(int y=0;y<4;y++){UBYTE*r=con_width_row(&g,y);r[y]=1;r[y+1]=2;}
UBYTE snapshot[32];memcpy(snapshot,g.cells,32);con_width_scroll(&g,1,3,1);assert(!memcmp(g.cells,snapshot,8));assert(!memcmp(g.cells+8,snapshot+16,16));for(int i=24;i<32;i++)assert(!g.cells[i]);
memcpy(g.cells,snapshot,32);con_width_scroll(&g,0,2,-2);assert(!memcmp(g.cells+16,snapshot,8));assert(!memcmp(g.cells+24,snapshot+24,8));for(int i=0;i<16;i++)assert(!g.cells[i]);
memcpy(g.cells,snapshot,32);assert(con_width_resize(&g,10,5,8,16,FALSE));for(int y=0;y<4;y++)assert(!memcmp(g.cells+y*10,snapshot+y*8,8));
assert(con_width_resize(&g,10,5,8,18,FALSE));for(int i=0;i<50;i++)assert(!g.cells[i]);
g.cells[4]=1;con_width_clear(&g);assert(!g.cells[4]);fail_alloc=1;assert(!con_width_resize(&g,11,5,8,18,FALSE));assert(!g.cells&&!g.columns&&!g.rows);assert(!con_width_row(&g,-1));
fail_alloc=0;assert(con_width_resize(&g,8,4,8,16,TRUE));assert(!con_width_resize(&g,0,5,8,18,TRUE));assert(!g.cells&&!g.columns&&!g.rows);assert(!con_width_resize(&g,32769,5,8,18,TRUE));
printf("STANDARD_WIDTH_PASS edits=%u resize/scroll/clear/font/allocation\n",cases);return 0;}
'''
source=(root/'stdconclass.c').read_text()
helpers=source[source.index('static void stdcon_width_cut('):source.index('/* JAM2 text replaces')]
code=code.replace('int main(void){unsigned cases=0;','int map_tests(void){unsigned cases=0;')
code+=r'''
struct Library { int unused; };
struct RastPort { UBYTE FgPen,BgPen,DrawMode; };
typedef struct { struct RastPort rp; int x,y,cols,rows,cw,ch; UBYTE cu_BgPen; } Object;
struct stdcondata {struct con_width_grid widths;struct Library *scd_GfxBase;};
static int pixels[8],draws;
#define RASTPORT(o) (&(o)->rp)
#define CU(o) (o)
#define XCP (o->x)
#define YCP (o->y)
#define CHAR_XMAX(o) ((o)->cols-1)
#define CHAR_YMAX(o) ((o)->rows-1)
#define XRSIZE (o->cw)
#define YRSIZE (o->ch)
#define GFX_X(o,x) (x)
#define GFX_Y(o,y) (y)
#define JAM1 0
#define SetDrMd(r,m) ((r)->DrawMode=(m))
#define SetAPen(r,p) ((r)->FgPen=(p))
#define SetBPen(r,p) ((r)->BgPen=(p))
static void fill(struct RastPort*r,int a,int b,int c,int d){assert(b==0&&d==0&&a>=0&&c<8);assert(r->DrawMode==JAM1);for(int x=a;x<=c;x++)pixels[x]=r->FgPen;draws++;}
#define RectFill(r,a,b,c,d) ((void)GfxBase,fill(r,a,b,c,d))
'''+helpers+r'''
int main(void){assert(!map_tests());fail_alloc=0;
Object obj={{9,12,2},2,0,8,1,1,1,6};Object *o=&obj;struct stdcondata data={0};
stdcon_width_print(&data,o,FALSE,2,TRUE,FALSE);assert(!data.widths.cells);
stdcon_width_print(&data,o,TRUE,2,TRUE,FALSE);assert(data.widths.cells[2]==1&&data.widths.cells[3]==2);
for(int x=0;x<8;x++){pixels[x]=20+x;}draws=0;
stdcon_width_edit(&data,o,3,0,1,-1);assert(draws==1&&pixels[2]==6&&pixels[3]==6&&pixels[1]==21&&pixels[4]==24);
assert(o->rp.FgPen==9&&o->rp.BgPen==12&&o->rp.DrawMode==2);
assert(!data.widths.cells[2]&&!data.widths.cells[3]);
stdcon_width_print(&data,o,TRUE,2,TRUE,FALSE);o->x=3;draws=0;
stdcon_width_print(&data,o,TRUE,1,FALSE,TRUE);assert(draws==1&&!data.widths.cells[2]&&!data.widths.cells[3]&&!data.widths.cells[4]);
assert(o->rp.FgPen==9&&o->rp.BgPen==12&&o->rp.DrawMode==2);
FreeVec(data.widths.cells);puts("STANDARD_WIDTH_ADAPTER_PASS native draw-state restoration and derived-unit bypass");return 0;}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/'test.c';b=Path(tmp)/'test';p.write_text(code)
 subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
 subprocess.run([str(b)],check=True)
 p.write_text(code.replace('row[x-1]=row[x]=0;','row[x]=0;'))
 subprocess.run(['cc',str(p),'-o',str(b)],check=True)
 assert subprocess.run([str(b)],capture_output=True).returncode!=0
 print('One-sided wide-boundary negative control rejected')
