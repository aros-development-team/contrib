#!/usr/bin/env python3
"""Run actual window-font registry helpers with allocation/lifetime sentinels."""
from pathlib import Path
import subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/'consoleclass.c').read_text()
a=s.index('static struct ConsolengFontWindow *console_attach_font_window(')
body=s[a:s.index('/* Titles are already converted',a)]
prefix=r'''
#include <assert.h>
#include <stdlib.h>
typedef void VOID;typedef void Class;typedef unsigned long ULONG;
struct MinNode {struct MinNode *next,*prev;};struct MinList {struct MinNode head;};
#define Node MinNode
#define List MinList
struct Library {int unused;};struct TextFont {int size;};
struct RastPort {struct TextFont *Font;};struct Window {struct RastPort *RPort;};
struct ConsolengFontWindow {struct MinNode node;struct Window *window;struct TextFont *original;ULONG users;};
struct ConsoleBase {struct MinList fontWindows;int fontWindowLock;};
typedef struct {struct ConsolengFontWindow *fontWindow;struct TextFont *setFont;struct Window *window;} Object;
#define ICU(o) (o)
#define RASTPORT(o) (o->window->RPort)
#define MEMF_PUBLIC 1
#define MEMF_CLEAR 2
#define TAGGEDOPEN_GRAPHICS 1
#define ForeachNode(l,e) for(e=(void*)(l)->head.next;&e->node!=&(l)->head;e=(void*)e->node.next)
static int locked,allocations,fail;
static void ObtainSemaphore(int *s){assert(!locked);locked=1;}
static void ReleaseSemaphore(int *s){assert(locked);locked=0;}
static void *AllocVec(size_t n,int flags){assert(locked);if(fail)return NULL;allocations++;return calloc(1,n);}
static void FreeVec(void *p){assert(locked);allocations--;free(p);}
static void AddTail(struct MinList *l,struct MinNode *n){assert(locked);n->prev=l->head.prev;n->next=&l->head;n->prev->next=n;l->head.prev=n;}
static void Remove(struct MinNode *n){assert(locked);n->prev->next=n->next;n->next->prev=n->prev;}
static struct Library lib;
static struct Library *TaggedOpenLibrary(int t){return &lib;}
static void setfont(struct Library *g,struct RastPort *r,struct TextFont *f){r->Font=f;}
#define SetFont(r,f) setfont(GfxBase,r,f)
'''
main=r'''
int main(void){
 struct ConsoleBase d={0};d.fontWindows.head.next=d.fontWindows.head.prev=&d.fontWindows.head;
 struct TextFont base={8},owned={12},other={16};struct RastPort rp={&base},rp2={&other};struct Window w={&rp},w2={&rp2};
 Object a={0,&owned,&w},b={0,0,&w},c={0,0,&w2};
 a.fontWindow=console_attach_font_window(&d,&w);assert(a.fontWindow&&a.fontWindow->users==1&&allocations==1);
 rp.Font=&owned;
 b.fontWindow=console_attach_font_window(&d,&w);assert(b.fontWindow==a.fontWindow&&b.fontWindow->original==&base&&b.fontWindow->users==2);
 console_restore_font(0,&b);assert(rp.Font==&base);
 c.fontWindow=console_attach_font_window(&d,&w2);assert(c.fontWindow!=a.fontWindow&&allocations==2);
 console_detach_font_window(&d,&a);assert(!a.fontWindow&&b.fontWindow->users==1&&rp.Font==&base&&allocations==2);
 b.setFont=&owned;rp.Font=&owned;console_detach_font_window(&d,&b);assert(!b.fontWindow&&rp.Font==&base&&allocations==1);
 console_detach_font_window(&d,&c);assert(!c.fontWindow&&allocations==0&&rp2.Font==&other);
 console_detach_font_window(&d,&c);assert(!locked);
 fail=1;assert(!console_attach_font_window(&d,&w)&&!allocations&&!locked);
 fail=0;a.fontWindow=console_attach_font_window(&d,&w);assert(a.fontWindow->original==&base);
 console_detach_font_window(&d,&a);assert(!allocations&&!locked);
 return 0;}
'''
with tempfile.TemporaryDirectory() as td:
 for name,code in [('current',body),('no_restore',body.replace('SetFont(RASTPORT(o), entry->original);','(void)entry;'))]:
  p=Path(td)/(name+'.c');b=Path(td)/name;p.write_text(prefix+code+main)
  subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
  r=subprocess.run([str(b)],capture_output=True)
  assert (r.returncode==0)==(name=='current'),(name,r.stderr.decode())
  print(name,'PASS' if name=='current' else 'negative control rejected')
print('Font windows PASS: shared baseline, independent windows, hidden/visible detach, final free, reopen, allocation failure and locking')
