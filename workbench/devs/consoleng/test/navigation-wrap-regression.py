#!/usr/bin/env python3
"""Independent cell-leader oracle for soft-wrap versus hard-row navigation."""
from pathlib import Path
import random,subprocess,tempfile
r=Path(__file__).resolve().parents[1]
prefix=(r/'test/navigation-regression.py').read_text().split("prefix='''",1)[1].split("'''",1)[0]
random.seed(185)
rows=[];soft=[];padding=[]
for i in range(50):
 cells=[]
 while len(cells)<8:
  if len(cells)<7 and random.randrange(4)==0:cells += [0x4e2d,0xfffffffe]
  else:cells.append(ord('a')+random.randrange(26))
 wrap=i<49 and random.choice([False,True]);pad=int(wrap and cells[-1]!=0xfffffffe and random.randrange(3)==0)
 rows.append(cells);soft.append(wrap);padding.append(pad)
leaders=[[i for i,c in enumerate(row[:8-padding[y]]) if c!=0xfffffffe] for y,row in enumerate(rows)]
body=['int main(void){struct charmap_line rows[50]={0};struct con_nav n;unsigned cases=0;']
for y,cells in enumerate(rows):
 body.append('ULONG cells'+str(y)+'[]={'+','.join(map(str,cells))+'};')
 body.append(f'rows[{y}].text=cells{y};rows[{y}].size=8;rows[{y}].wrapColumns={8 if soft[y] else 0};rows[{y}].wrapPadding={padding[y]};')
body.append('for(unsigned i=0;i<50;i++){rows[i].prev=i?&rows[i-1]:NULL;rows[i].next=i<49?&rows[i+1]:NULL;}')
for y,cols in enumerate(leaders):
 for ci,x in enumerate(cols):
  left=(y,cols[ci-1]) if ci else (y-1,leaders[y-1][-1]) if y and soft[y-1] else (y,x)
  right=(y,cols[ci+1]) if ci+1<len(cols) else (y+1,0) if soft[y] else (y,x)
  first=y
  if not x:
   while first and soft[first-1]:first-=1
  for motion,(ey,ex) in zip(['LEFT','RIGHT','FIRST'],[left,right,(first,0)]):
   body.append(f'con_nav_begin(&n,&rows[{y}],{y},{x},8);con_nav_move(&n,CON_NAV_{motion});for(unsigned k=0;n.pending;k++){{assert(k<100);con_nav_step(&n,1);}}assert(n.cursor.row=={ey}&&n.cursor.column=={ex});cases++;')
body.append('''struct charmap_line *longrows=calloc(10000,sizeof(*longrows));assert(longrows);
for(unsigned i=0;i<10000;i++){longrows[i].wrapColumns=8;longrows[i].prev=i?&longrows[i-1]:NULL;longrows[i].next=i<9999?&longrows[i+1]:NULL;}
con_nav_begin(&n,&longrows[9999],9999,0,8);con_nav_move(&n,CON_NAV_FIRST);
assert(!con_nav_step(&n,0)&&n.cursor.row==9999);assert(!con_nav_step(&n,1024)&&n.cursor.row==8975);
while(!con_nav_step(&n,1024));assert(!n.cursor.row);free(longrows);
printf("Wrap navigation PASS: %u independent cell targets and bounded 10000-row first-column traversal\\n",cases);}
''')
header=(r/'navigation.h').read_text()
for label,code in [('baseline',header),('lost soft-wrap crossing',header.replace('==1)n->cursor=probe.cursor;','==1 && probe.cursor.row==n->cursor.row)n->cursor=probe.cursor;')),('crossed hard rows',header.replace('==1)n->cursor=probe.cursor;','!=0)n->cursor=probe.cursor;'))]:
 with tempfile.TemporaryDirectory() as d:
  p=Path(d);src=p/'t.c';exe=p/'t';src.write_text(prefix+code+'\n'.join(body))
  subprocess.run(['cc','-Wall','-Werror','-Wno-misleading-indentation',str(src),'-o',str(exe)],check=True)
  result=subprocess.run([str(exe)],capture_output=True)
  if label=='baseline':assert result.returncode==0,result.stderr;print(result.stdout.decode().strip())
  else:assert result.returncode!=0,label;print('Negative control PASS:',label)
