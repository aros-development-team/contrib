#!/usr/bin/env python3
"""Check actual refresh EOL code never paints an inverted/full-row rectangle."""
from pathlib import Path
import subprocess,tempfile
source=(Path(__file__).resolve().parents[1]/'charmapconclass.c').read_text()
refresh=source.split('static VOID charmapcon_refresh_lines(',1)[1]
body=refresh.split('        ULONG tailStart = 0;',1)[1].split('        /* Erased cells are retained backgrounds',1)[0]
body='        ULONG tailStart = 0;'+body
overlay=source.split('static VOID charmap_block_span(',1)[1].split('static VOID charmapcon_refresh_lines(',1)[0]
overlay='static VOID charmap_block_span('+overlay
# Tail background must precede text, or italic overhang is clipped.
assert refresh.index('ULONG tailStart = 0;') < refresh.index('Text(rp, bytebuf, n);')
code=r'''
#include <assert.h>
typedef unsigned long ULONG;typedef unsigned char UBYTE;typedef void VOID;
#define CON_TXTFLAGS_REVERSED 8
#define MIN(a,b) ((a)<(b)?(a):(b))
struct charmap_line {ULONG size;UBYTE *flags;ULONG *text;UBYTE *fgpen,*bgpen;};
struct charmap_search_ui {int nav_mode;struct {int selecting,blockwise;}nav;int found;ULONG first_row,last_row;struct {struct {ULONG column;}first,end;}match;};
static struct charmap_search_ui selection,*active_search;
''' +overlay+r'''
#define charmap_blank_tail_pen(l,f) 0
#define ConsoleNGFaintPen(o,g,fl,p,k) (p)
#define charmap_foreground_key(l,x) 0
static int count, left, right, mode;
static UBYTE tailUsed;
#define JAM1 0
#define SetDrMd(rp,m) (mode=(m))
#define CHAR_XMAX(o) 7
#define CU(o) (o)
#define SetAPen(rp,p) ((void)(tailUsed=(p)))
#define GFX_X(o,x) (10+(x)*8)
#define GFX_Y(o,y) (20+(y)*16)
#define GFX_XMAX(o) 73
#define RectFill(rp,a,b,c,d) (assert(mode==JAM1),count++,left=(a),right=(c))
static void clear(ULONG size, ULONG zero) {
    ULONG str[16]; for(ULONG i=0;i<16;i++)str[i]=i==zero?0:65;
    struct charmap_line storage={size,0,str,0,0}, *line=&storage;
    struct {UBYTE cu_FgPen;} object={5},*o=&object;
    struct {struct charmap_search_ui *search;ULONG scrollback_pos;} state={active_search,0},*data=&state;
    ULONG yc=0;
#define charmap_resolve_rgb(o,l,x) ((void)0)
'''+body+r'''
}
int main(void) {
    ULONG i;
    for(i=0;i<=10;i++) {
        mode=255;count=0;clear(i,15);
        assert(count==(i<8));
        if(count)assert(left==10+(int)i*8 && right==73 && left<=right);
    }
    mode=255;count=0;clear(10,3);
    assert(count==1 && left==34 && right==73);
    selection.found=1;selection.first_row=selection.last_row=0;
    selection.match.first.column=2;selection.match.end.column=6;active_search=&selection;
    mode=255;count=0;clear(3,15);assert(count==2&&left==34&&right==57&&tailUsed==5);
    mode=255;count=0;clear(0,15);assert(count==2&&left==26&&right==57);
    selection.match.first.column=9;selection.match.end.column=10;
    mode=255;count=0;clear(0,15);assert(count==1&&left==10&&right==73);
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='consoleng-eol-') as d:
    p=Path(d)/'test.c';binary=Path(d)/'test';p.write_text(code)
    subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
print('EOL regression PASS: partial/full/overfull row bounds')
