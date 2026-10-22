#!/usr/bin/env python3
"""Compile actual private ADT FreeType BGRA conversion; validate alpha and pitch."""
from pathlib import Path
import subprocess,tempfile,argparse
parser=argparse.ArgumentParser();parser.add_argument('source',type=Path,help='ADT source root');args=parser.parse_args()
s=(args.source/'workbench/libs/freetype2/glyph.c').read_text(encoding='latin1')
a=s.index('static BOOL bitmap_argb_at(');b=s.index('static ULONG scale_bitmap_dim(',a);f=s[a:b]
src=r'''
#include <stdint.h>
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
typedef uint8_t UBYTE;typedef uint32_t ULONG;typedef int32_t LONG;typedef int BOOL;
#define TRUE 1
#define FALSE 0
#define FT_PIXEL_MODE_BGRA 7
typedef struct {unsigned width,rows;int pitch;unsigned char*buffer;int pixel_mode;} FT_Bitmap;
'''+f+r'''
int main(void){UBYTE bytes[16]={0};FT_Bitmap b={1,2,8,bytes,7};ULONG out;UBYTE *p=(UBYTE*)&out;
for(unsigned alpha=0;alpha<256;alpha++)for(unsigned channel=0;channel<256;channel++){
 bytes[0]=channel;bytes[1]=channel;bytes[2]=channel;bytes[3]=alpha;
 assert(bitmap_argb_at(&b,0,0,&out));unsigned expected=alpha?(channel*255+alpha/2)/alpha:0;if(expected>255)expected=255;
 assert(p[0]==alpha&&p[1]==expected&&p[2]==expected&&p[3]==expected);
}
bytes[8]=10;bytes[9]=20;bytes[10]=30;bytes[11]=255;b.pitch=-8;assert(bitmap_argb_at(&b,0,0,&out));assert(p[0]==255&&p[1]==30&&p[2]==20&&p[3]==10);
assert(!bitmap_argb_at(&b,1,0,&out)&&!out);assert(!bitmap_argb_at(&b,0,2,&out));assert(!bitmap_argb_at(NULL,0,0,&out));assert(!bitmap_argb_at(&b,0,0,NULL));b.pixel_mode=0;assert(!bitmap_argb_at(&b,0,0,&out));
puts("Color conversion PASS: 65536 alpha/channel pairs, negative pitch, bounds and nulls");}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';e=Path(d)/'t';p.write_text(src)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(e)],check=True);subprocess.run([str(e)],check=True)
