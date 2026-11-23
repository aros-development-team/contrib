#!/usr/bin/env python3
"""Exercise the actual ECH command bodies with a small host-side device model.
Native drawing, parser reassembly and handler integration require native tests.
"""
from pathlib import Path
from hyperlink_fixture import hyperlink_source
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
def body(name):
    source = (root / name).read_text()
    return source.split('    case C_ERASE_CHAR:', 1)[1].split('    case C_ERASE_IN_LINE:', 1)[0]

prefix = r'''
/* This fixture has no plain wide grid; standard-width-regression and native edits cover it. */
#define stdcon_width_edit(d,o,x,y,n,op) ((void)(x),(void)(y),(void)(n),(void)(op))
#include <assert.h>
#define CHARMAP_ROLES(o) 0x0102
#include <stdint.h>
#include <string.h>
typedef intptr_t IPTR;
typedef long LONG;
typedef unsigned long ULONG;
typedef unsigned char UBYTE;
struct charmap_line { ULONG size; ULONG *text; signed char *fgpen,*bgpen,*flags; unsigned long long *roles; unsigned long wrapColumns; unsigned char wrapPadding;struct con_link **links;ULONG *underlineColors;};
struct unit { int x,y,max,fg,bg; };
struct rp { UBYTE FgPen; };
static struct charmap_line fixture_line;
static int rect[4], draws;
static void grow(struct charmap_line *line, ULONG size) {
    ULONG i;
    assert(size <= 8);
    for(i=line->size;i<size;i++) {
        line->text[i]=0; line->fgpen[i]=line->bgpen[i]=line->flags[i]=0;
    }
    line->size=size;
}
#define charmap_resize(base,line,size) grow(line,size)
#define XCP (o->x)
#define YCP (o->y)
#define CU(o) (o)
#define cu_FgPen fg
#define cu_BgPen bg
#define CHAR_XMAX(o) (o->max)
#define GFX_X(o,x) (10+(x)*8)
#define GFX_Y(o,y) (20+(y)*16)
#define charmapcon_find_line(cl,o,y) (&fixture_line)
#define DoSuperMethodA(cl,o,msg) ((void)0)
#define Console_UnRenderCursor(o) ((void)0)
#define Console_RenderCursor(o) ((void)0)
#define SetAPen(rp,p) ((rp)->FgPen=(p))
#define RectFill(rp,a,b,c,d) (rect[0]=(a),rect[1]=(b),rect[2]=(c),rect[3]=(d),draws++)
'''
header = (root / 'console_gcc.h').read_text()
commands = 'enum' + header.split('enum', 1)[1].split('};', 1)[0] + '};'
support = (root / 'support.c').read_text()
parser = 'static const struct Command' + support.split('static const struct Command', 1)[1].split('VOID printstring', 1)[0]
prefix += commands + r"""
typedef signed char BYTE;
typedef unsigned short UWORD;
typedef int BOOL;
typedef struct unit Object;
typedef void VOID;
typedef short WORD;
#define CON_IS_UTF8(o) 1
#define CON_TEXT_IS_UTF8(o) CON_IS_UTF8(o)
#define CON_IS_OBSCURED(o) 0
#define charmapcon_refresh_lines(cl,o,a,b) ((void)0)
#define CELL_WIDE_CONT 0xFFFFFFFEUL
struct ConsoleBase;
#define TRUE 1
#define FALSE 0
#define FIRST_CSI_CMD 0x40
#define MAX_COMMAND_PARAMS 16
#define D(x) ((void)0)
#define Console_GetDefaultParams(unit,command,params) ((params)[0]=1)
""" + parser
source=(root/'charmapconclass.c').read_text()
helper='static VOID charmap_repair_wide_cells('+source.split('static VOID charmap_repair_wide_cells(',1)[1].split('static BOOL charmap_delete_char(',1)[0]
width='WORD con_cp_width('+support.split('WORD con_cp_width(',1)[1].split('/* Cells (columns)',1)[0]
prefix += hyperlink_source() + width + helper
code = prefix + '\nstatic void cells(struct unit *o, IPTR *params) { switch(0) { case 0:' + body('charmapconclass.c') + '} }\n'
code += 'static void pixels(struct unit *o, IPTR *params, struct rp *rp) { switch(0) { case 0:' + body('stdconclass.c') + '} }\n'
code += r'''
int main(void) {
    const char *streams[] = {"X", "0X", "3X", "65535X", "b", "0b", "65535b", "999999999b"};
    const int expected[] = {1,0,3,65535,1,0,65535,65535};
    int k;
    for(k=0;k<8;k++) {
        UBYTE *input=(UBYTE *)streams[k], *cursor=input, n=0;
        BYTE command=-1; IPTR params[16]={0};
        assert(getparamcommand(&command,&cursor,&n,strlen(streams[k]),params,0,0));
        assert(command==(k<4?C_ERASE_CHAR:C_REPEAT_CHAR) && params[0]==expected[k]);
        assert(cursor==input+strlen(streams[k]));
    }
    const IPTR counts[] = {0,1,3,1000000};
    int test, x, initial;
    for(initial=0;initial<=8;initial++) for(test=0;test<4;test++) for(x=0;x<8;x++) {
        ULONG text[8]; signed char fg[8],bg[8],flags[8];
        struct unit unit={x,2,7,4,6}; struct rp rp={9};
        IPTR count=counts[test]; int n=count>0?count:1;
        int i;
        if(n>8-x)n=8-x;
        for(i=0;i<8;i++) {text[i]='A'+i;fg[i]=1;bg[i]=2;flags[i]=3;}
        fixture_line=(struct charmap_line){initial,text,fg,bg,flags};
        draws=0; cells(&unit,&count); pixels(&unit,&count,&rp);
        assert(unit.x==x && unit.y==2 && fixture_line.size==(ULONG)(initial>x+n?initial:x+n));
        assert(draws==1 && rect[0]==10+x*8 && rect[2]==10+(x+n)*8-1);
        assert(rect[1]==52 && rect[3]==67 && rp.FgPen==9);
        for(i=0;i<(int)fixture_line.size;i++) {
            int erased=i>=x && i<x+n;
            assert(text[i]==(ULONG)(erased || i>=initial?' ':'A'+i));
            assert(bg[i]==(erased?6:i>=initial?0:2) && flags[i]==(erased || i>=initial?0:3));
        }
    }
    for(int pair=0;pair<7;pair++)for(int x=0;x<8;x++)for(int k=0;k<4;k++) {
        ULONG text[8],expected[8];signed char fg[8],bg[8],flags[8];
        struct unit unit={x,2,7,4,6};IPTR count=counts[k];
        int n=count>0?count:1;if(n>8-x)n=8-x;
        for(int i=0;i<8;i++){text[i]='A'+i;fg[i]=1;bg[i]=2;flags[i]=3;}
        text[pair]=0x2615;text[pair+1]=CELL_WIDE_CONT;
        memcpy(expected,text,sizeof text);
        for(int i=x;i<x+n;i++)expected[i]=' ';
        if(x<pair+2 && x+n>pair)expected[pair]=expected[pair+1]=' ';
        fixture_line=(struct charmap_line){8,text,fg,bg,flags};cells(&unit,&count);
        for(int i=0;i<8;i++)assert(text[i]==expected[i]);
        assert(unit.x==x&&unit.y==2);
    }
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='consoleng-ech-') as directory:
    source = Path(directory) / 'test.c'
    binary = Path(directory) / 'test'
    source.write_text(code)
    subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(source),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
print('ECH regression PASS: actual parser accepts default/zero/3/65535; 288 short-line/count/position + 224 wide-boundary cases; cells, bounds, cursor and pen preservation')
