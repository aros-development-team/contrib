#!/usr/bin/env python3
"""Execute the actual retained-cell DCH implementation under host sanitizers."""
from pathlib import Path
from hyperlink_fixture import hyperlink_source
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'charmapconclass.c').read_text()
function='static BOOL charmap_delete_char('+s.split('static BOOL charmap_delete_char(',1)[1].split('static BOOL charmap_insert_char(',1)[0]
helper='static VOID charmap_repair_wide_cells('+s.split('static VOID charmap_repair_wide_cells(',1)[1].split('static BOOL charmap_delete_char(',1)[0]
raster=(root/'stdconclass.c').read_text().split('    case C_DELETE_CHAR:',1)[1].split('    case C_LINEFEED:',1)[0]
prefix=r'''
/* This fixture has no plain wide grid; standard-width-regression and native edits cover it. */
#define stdcon_width_edit(d,o,x,y,n,op) ((void)(x),(void)(y),(void)(n),(void)(op))
#include <assert.h>
#define CHARMAP_ROLES(o) 0x0102
#include <stdint.h>
#include <string.h>
typedef intptr_t IPTR;
typedef void VOID;
#define CELL_WIDE_CONT 0xFFFFFFFEUL
#define CON_IS_UTF8(o) 1
#define CON_TEXT_IS_UTF8(o) CON_IS_UTF8(o)
typedef unsigned long ULONG;
static int con_cp_width(ULONG cp) { return cp == 0x1F600 ? 2 : 1; }
typedef int BOOL;
typedef void Class;
#define TRUE 1
#define FALSE 0
struct charmap_line { ULONG size; ULONG *text; signed char *fgpen,*bgpen,*flags; unsigned long long *roles; unsigned long wrapColumns; unsigned char wrapPadding;struct con_link **links;ULONG *underlineColors;};
typedef struct {int max,fg,bg,x,y;} Object;
typedef long LONG;
typedef unsigned char UBYTE;
struct rp {UBYTE FgPen,BgPen,DrawMode;};
static int scroll[6],cleared;
#define JAM1 0
#define SetBPen(rp,p) ((rp)->BgPen=(p))
#define SetDrMd(rp,m) ((rp)->DrawMode=(m))
#define RectFill(rp,a,b,c,d) (cleared=1,assert((rp)->FgPen==6&&(rp)->BgPen==6&&(rp)->DrawMode==JAM1),scroll[2]=(a),scroll[3]=(b),scroll[4]=(c),scroll[5]=(d))
#define XCP (o->x)
#define YCP (o->y)
#define XRSIZE 8
#define GFX_X(o,x) (10+(x)*8)
#define GFX_Y(o,y) (20+(y)*16)
#define GFX_XMAX(o) (10+((o)->max+1)*8-1)
#define Console_UnRenderCursor(o) ((void)0)
#define Console_RenderCursor(o) ((void)0)
#define SetAPen(rp,p) ((rp)->FgPen=(p))
#define ScrollRaster(rp,dx,dy,a,b,c,d) (scroll[0]=(dx),scroll[1]=(dy),scroll[2]=(a),scroll[3]=(b),scroll[4]=(c),scroll[5]=(d))
static struct charmap_line fixture;
static int fail_alloc;
#define CHAR_XMAX(o) ((o)->max)
#define CU(o) (o)
#define cu_FgPen fg
#define cu_BgPen bg
#define charmapcon_find_line(cl,o,y) (&fixture)
static void grow(struct charmap_line *line, ULONG size) {
    ULONG i;
    if(fail_alloc)return;
    assert(size<=8);
    for(i=line->size;i<size;i++) {
        line->text[i]=0;line->fgpen[i]=line->bgpen[i]=line->flags[i]=0;
        if(line->roles)line->roles[i]=0;
        if(line->underlineColors)line->underlineColors[i]=0;
    }
    line->size=size;
}
#define charmap_resize(base,line,size) grow(line,size)
'''
main=r'''
int main(void) {
    const IPTR counts[]={0,1,3,65535};
    int initial,x,k,i;
    Object o={7,4,6,0,2};
    for(initial=0;initial<=8;initial++)for(x=0;x<8;x++)for(k=0;k<4;k++) {
        ULONG text[8],colors[8]; signed char fg[8],bg[8],flags[8]; unsigned long long roles[8];
        int count=counts[k]>0?counts[k]:1;
        if(count>8-x)count=8-x;
        for(i=0;i<8;i++){text[i]='A'+i;fg[i]=1;bg[i]=2;flags[i]=3;roles[i]=0x0300+i;colors[i]=101+i;}
        fixture=(struct charmap_line){initial,text,fg,bg,flags,roles};fixture.underlineColors=colors;
        fail_alloc=0;
        assert(charmap_delete_char(0,&o,x,0,counts[k]));
        assert(fixture.size==8);
        {
            IPTR parameter=counts[k]; struct rp rp={9,12,2};cleared=0;scroll[0]=0;
            o.x=x; pixels(&o,&parameter,&rp);
            assert(cleared==(count==8-x));
            assert(scroll[0]==(cleared?0:count*8) && scroll[1]==0);
            assert(scroll[2]==10+x*8 && scroll[4]==73);
            assert(scroll[3]==52 && scroll[5]==67);
            assert(o.x==x && o.y==2 && rp.FgPen==9 && rp.BgPen==12 && rp.DrawMode==2);
        }
        for(i=0;i<8;i++) {
            int from=i<x?i:i+count;
            int blank=i>=8-count;
            /* The prefix before cursor is not affected by deletion. */
            if(i<x){from=i;blank=0;}
            assert(colors[i]==(ULONG)(blank||from>=initial?0:101+from));
            assert(text[i]==(ULONG)(blank||from>=initial?' ':'A'+from));
            assert(bg[i]==(blank?6:from>=initial?0:2));
            assert(flags[i]==(blank||from>=initial?0:3));
            assert(roles[i]==(blank?0x0102:from>=initial?0:0x0300+from));
        }
    }
    /* Edit through every possible half of a wide glyph and screen edge. */
    for(int pair=0;pair<7;pair++)for(x=0;x<8;x++)for(k=0;k<4;k++) {
        ULONG text[8], expected[8]; signed char fg[8],bg[8],flags[8];
        int count=counts[k]>0?counts[k]:1;
        if(count>8-x)count=8-x;
        for(i=0;i<8;i++){text[i]='A'+i;fg[i]=1;bg[i]=2;flags[i]=3;}
        text[pair]=0x1F600;text[pair+1]=CELL_WIDE_CONT;
        for(i=0;i<8;i++) {
            int from=i<x?i:i<8-count?i+count:-1;
            expected[i]=from<0?' ':text[from];
        }
        /* Derive expected fragment blanking from the unmodified result. */
        for(i=0;i<8;i++) {
            if(expected[i]==0x1F600 && (i==7||expected[i+1]!=CELL_WIDE_CONT))expected[i]=' ';
            if(expected[i]==CELL_WIDE_CONT && (i==0||expected[i-1]!=0x1F600))expected[i]=' ';
        }
        fixture=(struct charmap_line){8,text,fg,bg,flags};fail_alloc=0;
        assert(charmap_delete_char(0,&o,x,0,counts[k]));
        for(i=0;i<8;i++)assert(text[i]==expected[i]);
    }
    {
        ULONG text[8]={'A'}; signed char fg[8]={1},bg[8]={2},flags[8]={3};
        fixture=(struct charmap_line){1,text,fg,bg,flags};fail_alloc=1;
        assert(!charmap_delete_char(0,&o,0,0,3));
        assert(fixture.size==1&&text[0]=='A'&&bg[0]==2);
    }
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='consoleng-dch-') as directory:
    source=Path(directory)/'test.c';binary=Path(directory)/'test'
    source.write_text(prefix+hyperlink_source()+helper+function+'static void pixels(Object *o, IPTR *params, struct rp *rp) { switch(0) { case 0:'+raster+'}}\n'+main)
    subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(source),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
print('DCH regression PASS: 288 short-line/count/position cases plus 224 wide-boundary cases and allocation failure')
