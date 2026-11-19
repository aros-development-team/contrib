#!/usr/bin/env python3
"""Test actual scroller-aware text geometry with relative and absolute gadgets."""
from pathlib import Path
import subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/'charmapconclass.c').read_text()
f=s[s.index('static LONG charmapcon_content_right('):s.index('/***********  CharMapCon::New()')]
start=s.index('static VOID charmapcon_docommand(')
a=s.index('    case C_FORMFEED:',start);b=s.index('    case C_DELETE_CHAR:',a)
c=s.index('    case C_ERASE_IN_DISPLAY:',start);d=s.index('    case C_INSERT_LINE:',c)
dispatch=s[a:b]+s[c:d]
code=r'''
#include <assert.h>
#define VOID void
typedef long IPTR;typedef void *Msg;typedef unsigned char UBYTE;typedef long LONG;typedef short WORD;typedef void Class;
struct Library {int unused;};struct RastPort {UBYTE FgPen,DrawMode;};
struct Window {int Width,Height,BorderLeft,BorderRight,BorderTop,BorderBottom,Flags;struct RastPort *RPort;};
struct Gadget {int LeftEdge,Flags;};
struct Scroll {struct Gadget scroller,up,down;};
struct charmapcondata {struct Scroll *prop;int boopsigad;struct Library *ccd_GfxBase;};
typedef struct {struct Window *cu_Window;int cu_XRSize,cu_XROrigin,cu_XMax,cu_XRExtant,cu_BgPen,hidden;} Object;
static struct charmapcondata fixture;static int rectangles;
static long expected_right,expected_bottom;
#define INST_DATA(cl,o) (&fixture)
#define CU(o) (o)
#define XRSIZE (o->cu_XRSize)
#define CHAR_XMAX(o) (o->cu_XMax)
#define GFX_X(o,n) (o->cu_XROrigin+(n)*XRSIZE)
#define GFX_XMIN(o) (o->cu_XROrigin)
#define GFX_XMAX(o) (o->cu_XRExtant)
#define GFX_YMIN(o) 20
#define GFX_YMAX(o) 107
#define GFLG_RELRIGHT 1
#define WFLG_GIMMEZEROZERO 2
#define CON_IS_OBSCURED(o) (o->hidden)
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))
#define JAM1 1
#define C_FORMFEED 1
#define C_ERASE_IN_DISPLAY 2
#define XCP 0
#define YCP 0
#define charmap_formfeed(cl,o) ((void)(cl),(void)(o))
#define charmap_erase_display(cl,o,x,y,m) ((void)(cl),(void)(o),(void)(m))
#define DoSuperMethodA(cl,o,msg) ((void)(cl),(void)(o),(void)(msg))
#define charmapcon_refresh_lines(cl,o,a,b) ((void)(cl),(void)(o))
#define SetAPen(r,p) ((void)GfxBase,(r)->FgPen=(p))
#define SetDrMd(r,m) ((r)->DrawMode=(m))
static void RectFill(struct RastPort *rp,long x1,long y1,long x2,long y2){
 assert(x1>=4 && y1>=20 && x2>=x1 && y2>=y1);
 assert(x2<=expected_right && y2<=expected_bottom);rectangles++;
}
'''+f+'static void erase(Object *o,int command,IPTR mode){Class *cl=0;Msg msg=0;IPTR *params=&mode;switch(command){'+dispatch+'}}\n'+r'''
int main(void) {
 struct RastPort rp={7,2};
 struct Window win={720,123,4,4,20,4,0,&rp};
 struct Scroll prop={{-16,1},{-21,1},{-21,1}};
 Object o={&win,8,4,88,715,0,0};fixture.prop=&prop;
 charmapcon_update_text_width(0,&o);assert(o.cu_XMax==85&&o.cu_XRExtant==691);
 expected_right=698;expected_bottom=118;
 for(int hidden=0;hidden<2;hidden++)for(int command=1;command<=2;command++)for(int mode=0;mode<4;mode++){
  o.hidden=hidden;rectangles=0;erase(&o,command,mode);
  assert(rectangles==(!hidden&&(command==C_FORMFEED||mode==2)?2:0));
  assert(rp.FgPen==7&&rp.DrawMode==2);
 }
 o.hidden=0;rectangles=0;
 charmapcon_clear_margins(0,&o);assert(rectangles==2);assert(rp.FgPen==7&&rp.DrawMode==2);
 o.hidden=1;charmapcon_clear_margins(0,&o);assert(rectangles==2);o.hidden=0;
 o.cu_XMax=80;o.cu_XRExtant=651;
 charmapcon_update_text_width(0,&o);assert(o.cu_XMax==80&&o.cu_XRExtant==651);
 prop.scroller.LeftEdge=704;prop.scroller.Flags=0;fixture.boopsigad=1;
 o.cu_XMax=88;charmapcon_update_text_width(0,&o);assert(o.cu_XMax==86);
 fixture.prop=0;o.cu_XMax=88;
 charmapcon_update_text_width(0,&o);assert(o.cu_XMax==88);
 fixture.prop=&prop;o.cu_XRSize=0;
 charmapcon_update_text_width(0,&o);assert(o.cu_XMax==88);
 fixture.boopsigad=0;prop.scroller.LeftEdge=-16;prop.scroller.Flags=1;o.cu_XRSize=8;
 for(int w=100;w<800;w++) {
  win.Width=w;o.cu_XMax=(w-8)/8-1;o.cu_XRExtant=4+8*(o.cu_XMax+1)-1;
  charmapcon_update_text_width(0,&o);
  assert(o.cu_XRExtant<w-21);
  expected_right=w-22;charmapcon_clear_margins(0,&o);
 }
 win.Width=720;win.Flags=WFLG_GIMMEZEROZERO;
 assert(charmapcon_content_right(0,&o)==695);
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='consoleng-geometry-') as d:
    p=Path(d)/'test.c';b=Path(d)/'test'
    for negative in (False,True):
        candidate=code
        if negative:
            a=candidate.index('static void erase(');e=candidate.index('int main',a)
            candidate=candidate[:a]+candidate[a:e].replace('charmapcon_clear_margins(cl, o);','(void)0;')+candidate[e:]
        p.write_text(candidate)
        subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
        result=subprocess.run([str(b)],capture_output=True)
        assert bool(result.returncode)==negative,result.stderr.decode()
print('Geometry PASS: track/arrow exclusion, relative/absolute/no gadget, hidden units, GZZ coordinates, margins across 700 widths; full-clear/ED3/hidden dispatch and raster-state restoration; missing-clear negative control')
