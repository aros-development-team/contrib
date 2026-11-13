#!/usr/bin/env python3
"""Execute actual reveal command: install owned font before redraw, retain grid/history."""
from pathlib import Path
import subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/'consoleclass.c').read_text()
h=s.index('static VOID console_restore_font(');helper=s[h:s.index('static VOID console_detach_font_window(',h)]
a=s.index('    case C_RESET_OBSCURED:');body=s[a:s.index('    case C_SET_UTF8_MODE:',a)]
prefix=r'''
#include <assert.h>
typedef void VOID;typedef void Class;
struct Library {int unused;};struct TextFont {int size;};struct ConsolengFontWindow {struct TextFont *original;};struct RastPort {struct TextFont *font;};
typedef struct {unsigned conFlags;struct TextFont *setFont;struct RastPort rp;int rows,history;struct ConsolengFontWindow *fontWindow;} Object;
#define console_publish_title(cl,o) ((void)0)
#define ICU(o) (o)
#define RASTPORT(o) (&o->rp)
#define CF_OBSCURED 1
#define C_RESET_OBSCURED 1
#define D(x) ((void)0)
#define TAGGEDOPEN_GRAPHICS 1
static struct Library lib;static int available=1,sets;
static struct Library *TaggedOpenLibrary(int t){return available?&lib:0;}
static void install(struct Library *g,struct RastPort *r,struct TextFont *f){assert(g);r->font=f;sets++;}
#define SetFont(r,f) install(GfxBase,r,f)
''' + helper + r'''
static void reveal(Object *o) {Class *cl=0;switch(1){
'''
main=r'''
}}
int main(void){struct TextFont a={12},b={16};struct ConsolengFontWindow fw={&b};Object o={1,&a,{&b},23,71,&fw};
 reveal(&o);assert(o.rp.font==&a&&sets==1&&!o.conFlags&&o.rows==23&&o.history==71);
 o.setFont=0;o.rp.font=&b;o.conFlags=1;reveal(&o);assert(o.rp.font==&b&&sets==2&&!o.conFlags);
 o.setFont=&a;o.conFlags=1;available=0;reveal(&o);assert(o.rp.font==&b&&sets==2&&!o.conFlags);
 return 0;}
'''
with tempfile.TemporaryDirectory() as d:
 for name,code in [('current',body),('missing_restore',body.replace('console_restore_font(cl, o);','(void)cl;'))]:
  p=Path(d)/(name+'.c');b=Path(d)/name;p.write_text(prefix+code+main)
  subprocess.run(['cc','-Wall','-Werror','-Wno-unused-function',str(p),'-o',str(b)],check=True)
  r=subprocess.run([str(b)],capture_output=True)
  assert (r.returncode==0)==(name=='current')
  print(name,'PASS' if name=='current' else 'negative control rejected')
print('Font reveal PASS: owned font restored; no-font/library failure; geometry/history preserved')
