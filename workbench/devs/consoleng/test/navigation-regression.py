#!/usr/bin/env python3
"""Retained navigation: independent scalar-position oracle and bounded history."""
from pathlib import Path
import random, subprocess, tempfile
r=Path(__file__).resolve().parents[1]
prefix='''#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
typedef uint32_t ULONG;typedef int BOOL;typedef void VOID;
#define TRUE 1
#define FALSE 0
struct charmap_line {struct charmap_line *next,*prev;ULONG size;ULONG *text;ULONG wrapColumns,wrapPadding;};
struct con_search_position {const struct charmap_line *line;ULONG column;};
struct con_search_match {struct con_search_position first,end;};
'''
rows=[[ord('a'),0x4e00,0xfffffffe,ord('b')],[],[ord('x')],list(map(ord,'long line')), [0x4e00,0xfffffffe]]
random.seed(185)
ops=[random.randrange(8) for _ in range(5000)]
expected=[];row=col=0
for op in ops:
 positions=[i for i in range(80) if i>=len(rows[row]) or rows[row][i]!=0xfffffffe]
 if op==0:col=max((p for p in positions if p<col),default=0)
 elif op==1:col=min((p for p in positions if p>col),default=col)
 elif op==2:row=max(0,row-1)
 elif op==3:row=min(len(rows)-1,row+1)
 elif op==4:col=0
 elif op==5:
  occupied=next((i for i in range(len(rows[row])-1,-1,-1) if rows[row][i] not in (0,32,9)),0)
  current=col+int(col+1<len(rows[row]) and rows[row][col+1]==0xfffffffe)
  col=occupied if current<occupied else 79
 elif op==6:
  while row:
   row-=1;pos=[i for i in range(80) if i>=len(rows[row]) or rows[row][i]!=0xfffffffe];col=max((p for p in pos if p<=col),default=0)
 elif op==7:
  while row<len(rows)-1:
   row+=1;pos=[i for i in range(80) if i>=len(rows[row]) or rows[row][i]!=0xfffffffe];col=max((p for p in pos if p<=col),default=0)
 positions=[i for i in range(80) if i>=len(rows[row]) or rows[row][i]!=0xfffffffe]
 col=max((p for p in positions if p<=col),default=0)
 expected.append((row,col))
body='int main(void){\n'
for i,rowdata in enumerate(rows):body+=f'ULONG t{i}[]={{'+','.join(map(str,rowdata or [0]))+'};\n'
body+='struct charmap_line lines[5]={0};\n'
for i,rowdata in enumerate(rows):body+=f'lines[{i}].size={len(rowdata)};lines[{i}].text=t{i};\n'
body+='for(unsigned i=0;i<5;i++){if(i)lines[i].prev=&lines[i-1];if(i<4)lines[i].next=&lines[i+1];}\n'
body+='struct con_nav nav;con_nav_begin(&nav,lines,0,0,80);\n'
body+='unsigned ops[]={'+','.join(map(str,ops))+'};\n'
body+='''for(unsigned i=0;i<sizeof(ops)/sizeof(*ops);i++){
 con_nav_move(&nav,ops[i]);struct con_nav_cursor before=nav.cursor;
 assert(!con_nav_step(&nav,0)&&!memcmp(&before,&nav.cursor,sizeof(before)));
 unsigned steps=0;while(!con_nav_step(&nav,1)){assert(++steps<=85);}
 printf("%u %u\\n",nav.cursor.row,nav.cursor.column);
}
struct charmap_line *many=calloc(10000,sizeof(*many));assert(many);
for(unsigned i=0;i<10000;i++){if(i)many[i].prev=&many[i-1];if(i<9999)many[i].next=&many[i+1];}
con_nav_begin(&nav,many,0,0,80);con_nav_move(&nav,CON_NAV_BOTTOM);
assert(!con_nav_step(&nav,1024)&&nav.cursor.row==1024);
while(!con_nav_step(&nav,1024));assert(nav.cursor.row==9999);
con_nav_move(&nav,CON_NAV_TOP);assert(!con_nav_step(&nav,1024)&&nav.cursor.row==8975);
while(!con_nav_step(&nav,1024));assert(nav.cursor.row==0);free(many);
con_nav_begin(&nav,lines,0,1,80);nav.selecting=TRUE;nav.anchor=nav.cursor;
con_nav_move(&nav,CON_NAV_RIGHT);assert(con_nav_step(&nav,1));
struct con_search_match m;ULONG first,last;con_nav_match(&nav,&m,&first,&last);
assert(m.first.column==1&&m.end.column==4&&first==0&&last==0);
nav.cursor.column=0;con_nav_match(&nav,&m,&first,&last);assert(m.first.column==0&&m.end.column==3);
return 0;}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);src=p/'t.c';exe=p/'t';src.write_text(prefix+(r/'navigation.h').read_text()+body)
 subprocess.run(['cc','-Wall','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined',str(src),'-o',str(exe)],check=True)
 output=subprocess.check_output([str(exe)],text=True)
 actual=[tuple(map(int,x.split())) for x in output.splitlines()]
 assert actual==expected,next((i,a,b) for i,(a,b) in enumerate(zip(actual,expected)) if a!=b)
 print('Navigation PASS: 5000 independent motion cases, wide cells, empty rows, selection endpoints, zero budget and 10000-row traversal')
for label,old,new in [
 ('wide-cell boundary','0xfffffffeUL','0xffffffffUL'),
 ('zero-budget traversal','while(budget && n->cursor.line &&','while(n->cursor.line &&'),
]:
 with tempfile.TemporaryDirectory() as d:
  p=Path(d);src=p/'t.c';exe=p/'t';header=(r/'navigation.h').read_text();assert old in header
  src.write_text(prefix+header.replace(old,new)+body)
  subprocess.run(['cc','-Wall','-Werror','-Wno-misleading-indentation',str(src),'-o',str(exe)],check=True)
  result=subprocess.run([str(exe)],capture_output=True,text=True)
  actual=[tuple(map(int,x.split())) for x in result.stdout.splitlines() if len(x.split())==2]
  assert result.returncode or actual!=expected,label
  print('Negative control PASS:',label)
