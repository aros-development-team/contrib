#!/usr/bin/env python3
"""Actual retained cursor span: both wide halves, orphans and viewport bounds."""
from pathlib import Path
import subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/'charmapconclass.c').read_text();a=s.index('static IPTR charmap_cursor_span(');b=s.index('\nAROS_UFH3S',a);f=s[a:b]
code=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
typedef uint32_t ULONG;typedef int32_t LONG;typedef long IPTR;typedef int Class;
#define CELL_WIDE_CONT 0xfffffffeUL
struct charmap_line{struct charmap_line*next;ULONG size;ULONG*text;};
struct charmapcondata{struct charmap_line*top_of_window;};
typedef struct{struct charmapcondata data;int x,y,xmax,ymax;}Object;
#define INST_DATA(cl,o) (&(o)->data)
#define XCP (o->x)
#define YCP (o->y)
#define CHAR_XMAX(o) ((o)->xmax)
#define CHAR_YMAX(o) ((o)->ymax)
static int con_cp_width(ULONG cp){return cp==0x1f600?2:1;}
'''+f+r'''
int main(void){int cases=0;
 for(int size=0;size<=8;size++)for(int pair=0;pair<7;pair++)for(int orphan=0;orphan<3;orphan++)for(int xmax=0;xmax<8;xmax++)for(int x=-1;x<10;x++)for(int y=-1;y<4;y++){
  ULONG text[8];for(int i=0;i<8;i++)text[i]='A';text[pair]=orphan==1?'A':0x1f600;text[pair+1]=orphan==2?'B':CELL_WIDE_CONT;
  struct charmap_line row={0,size,text},empty={&row,0,0};Object o={{&empty},x,y,xmax,1};
  int want=1;
  if(y==1&&x>=0&&x<size&&x<=xmax){
   if(!orphan&&x==pair+1)want=3;
   if(!orphan&&x==pair&&x+1<size&&x<xmax)want=2;
  }
  assert(charmap_cursor_span(0,&o)==want);cases++;
 }
 Object o={{0},0,0,7,1};assert(charmap_cursor_span(0,&o)==1);
 printf("Cursor span PASS %d viewport/pair/orphan cases\n",cases);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';b=Path(d)/'t';p.write_text(code)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
 subprocess.run([str(b)],check=True)
 p.write_text(code.replace('return 3;', 'return 1;'))
 subprocess.run(['cc',str(p),'-o',str(b)],check=True)
 assert subprocess.run([str(b)],capture_output=True).returncode!=0
 print('Lost continuation-half normalization negative control rejected')
