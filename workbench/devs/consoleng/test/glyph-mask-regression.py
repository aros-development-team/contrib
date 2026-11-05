#!/usr/bin/env python3
"""Exercise actual monochrome glyph packing, including non-byte-aligned origins."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'consolengglyph.c').read_text()
body='static UWORD ConsolengMakeMask('+s.split('static UWORD ConsolengMakeMask(',1)[1].split('/* Draw one codepoint',1)[0]
prefix=r'''
#include <assert.h>
#include <stdlib.h>
#include <string.h>
typedef unsigned short UWORD;
typedef unsigned int ULONG;
typedef unsigned char UBYTE;
#define MEMF_ANY 0
#define AllocVec(n,f) malloc(n)
#define FreeVec(p) free(p)
struct ConsoleBase {UBYTE *glyphMask; ULONG glyphMaskSize;};
struct GlyphMap {UWORD glm_BlackWidth,glm_BMRows,glm_BMModulo,glm_BlackLeft; UBYTE *glm_BitMap;};
'''
main=r'''
int main(void) {
 struct ConsoleBase b={0};
 for(int w=1;w<=33;w++)for(int left=0;left<16;left++) {
  UBYTE source[16]={0};
  struct GlyphMap gm={w,2,8,left,source};
  for(int y=0;y<2;y++)for(int x=0;x<w;x++)if((x+y)%3!=0)
   source[y*8+((left+x)>>3)] |= 0x80>>((left+x)&7);
  unsigned mod=ConsolengMakeMask(&b,&gm);
  assert(mod==((w+15)/16)*2);
  for(int y=0;y<2;y++)for(int x=0;x<mod*8;x++) {
   int bit=!!(b.glyphMask[y*mod+(x>>3)] & (0x80>>(x&7)));
   assert(bit==(x<w && (x+y)%3!=0));
  }
 }
 free(b.glyphMask);return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';b=Path(d)/'test';p.write_text(prefix+body+main)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
 subprocess.run([str(b)],check=True)
print('Glyph mask PASS: 528 width/origin cases, two rows, padding cleared')
