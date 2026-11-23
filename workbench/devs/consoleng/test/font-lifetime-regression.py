#!/usr/bin/env python3
"""Actual font release helper: cached success/failure, locking, and address reuse."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'consolengglyph.c').read_text()
a=s.index('void ConsolengCloseFont(');body=s[a:s.index('/* Bind an engine',a)]
prefix=r'''
#include <assert.h>
struct Library {int unused;};struct TextFont {int refs;};
struct ConsoleBase {int glyphLock;struct TextFont *glyphFont;int glyphFailed,glyphEngine;};
#define TAGGEDOPEN_GRAPHICS 1
static struct Library graphics;static int available=1,locked,unbound,closed;
static struct Library *TaggedOpenLibrary(int tag){return available?&graphics:0;}
static void ObtainSemaphore(int *s){assert(!locked);locked=1;}
static void ReleaseSemaphore(int *s){assert(locked);locked=0;}
static void ConsolengGlyphUnbind(struct ConsoleBase *d){assert(locked);d->glyphFont=0;d->glyphFailed=0;d->glyphEngine=0;unbound++;}
static void closefont(struct Library *g,struct TextFont *f){assert(locked);assert(f->refs>0);--f->refs;closed++;}
#define CloseFont(f) closefont(GfxBase,f)
'''
main=r'''
int main(void){
 struct TextFont f={1},other={2};struct ConsoleBase d={0,&f,0,1};
 ConsolengCloseFont(&d,&f);assert(!f.refs&&!d.glyphFont&&!d.glyphEngine&&unbound==1&&closed==1&&!locked);
 /* Reuse the same address: it must no longer match the former binding. */
 f.refs=1;assert(d.glyphFont!=&f);
 d.glyphFont=&f;d.glyphFailed=1;ConsolengCloseFont(&d,&f);
 assert(!d.glyphFont&&!d.glyphFailed&&unbound==2&&closed==2);
 d.glyphFont=&other;f.refs=1;ConsolengCloseFont(&d,&f);
 assert(d.glyphFont==&other&&other.refs==2&&unbound==2&&closed==3);
 ConsolengCloseFont(&d,0);assert(closed==3&&!locked);
 available=0;ConsolengCloseFont(&d,&other);assert(other.refs==2&&d.glyphFont==&other&&closed==3&&!locked);
 return 0;
}
'''
variants={'current':body,'stale_binding':body.replace('ConsolengGlyphUnbind(device);','(void)device;'),'unlocked_close':body.replace('    CloseFont(font);\n    ReleaseSemaphore(&device->glyphLock);','    ReleaseSemaphore(&device->glyphLock);\n    CloseFont(font);')}
with tempfile.TemporaryDirectory() as td:
 for name,code in variants.items():
  p=Path(td)/(name+'.c');b=Path(td)/name;p.write_text(prefix+code+main)
  subprocess.run(['cc','-Wall','-Werror','-Wno-unused-function',str(p),'-o',str(b)],check=True)
  r=subprocess.run([str(b)],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
  assert (r.returncode==0)==(name=='current'),(name,r.returncode)
  print(name, 'PASS' if name=='current' else 'negative control rejected')
# Installed OSC and owned-window fonts must use the checked release path.
for name in ('consoleclass.c','console.c'):
 text=(root/name).read_text()
 assert 'CloseFont(ICU(o)->setFont)' not in text
 assert 'CloseFont(ownfont)' not in text
print('Font lifetime PASS: success/failure cache invalidation before release, foreign bindings retained, NULL/library failure, locked close, address reuse')
