#!/usr/bin/env python3
"""Independent logical-line inline-find oracle and bounded missing-target scans."""
from pathlib import Path
import random,subprocess,tempfile
r=Path(__file__).resolve().parents[1]
prefix=(r/'test/navigation-regression.py').read_text().split("prefix='''",1)[1].split("'''",1)[0]
random.seed(185);rows=[];soft=[]
for y in range(30):
 cells=[]
 while len(cells)<8:
  if len(cells)<7 and random.randrange(6)==0:cells += [0x4e2d,0xfffffffe]
  else:cells.append(random.choice([0,32,97,98,99]))
 rows.append(cells);soft.append(y<29 and random.choice([False,True]))
leaders=[[(y,x) for x,c in enumerate(row) if c!=0xfffffffe] for y,row in enumerate(rows)]
checks=[]
for y in range(30):
 start=y
 while start and soft[start-1]:start-=1
 end=y
 while soft[end]:end+=1
 cells=sum(leaders[start:end+1],[])
 for origin in leaders[y]:
  at=cells.index(origin)
  for reverse in [False,True]:
   for short in [False,True]:
    for needle in [32,97,98,0x4e2d,120]:
     expected=origin
     for i in range(at-1,-1,-1) if reverse else range(at+1,len(cells)):
      yy,xx=cells[i]
      if (rows[yy][xx] or 32)==needle:
       expected=cells[i+(1 if reverse else -1)] if short else cells[i]
       if short and reverse and xx+1<len(rows[yy]) and rows[yy][xx+1]==0xfffffffe:expected=(yy,xx)
       break
     checks.append((*origin,int(reverse),int(short),needle,*expected))
body=['int main(void){struct charmap_line rows[30]={0};struct con_nav n;']
for y,row in enumerate(rows):
 body+=['ULONG t'+str(y)+'[]={'+','.join(map(str,row))+'};',f'rows[{y}].text=t{y};rows[{y}].size=8;rows[{y}].wrapColumns={8 if soft[y] else 0};']
body.append('for(unsigned i=0;i<30;i++){rows[i].prev=i?&rows[i-1]:NULL;rows[i].next=i<29?&rows[i+1]:NULL;}')
body.append('unsigned tests[][7]={'+','.join('{'+','.join(map(str,x))+'}' for x in checks)+'};')
body.append(r'''for(unsigned i=0;i<sizeof(tests)/sizeof(*tests);i++){
 unsigned *t=tests[i];con_nav_begin(&n,&rows[t[0]],t[0],t[1],8);n.find_char=t[4];n.find_short=t[3];con_nav_move(&n,CON_NAV_FIND);n.reverse=t[2];
 assert(!con_nav_step(&n,0));for(unsigned count=0;n.pending;count++){assert(count<250);con_nav_step(&n,1);}
 assert(n.cursor.row==t[5]&&n.cursor.column==t[6]);}
ULONG *dense=malloc(10000*sizeof(*dense));assert(dense);for(unsigned i=0;i<10000;i++)dense[i]='a';struct charmap_line line={0};line.size=10000;line.text=dense;
con_nav_begin(&n,&line,0,0,10000);n.find_char='z';con_nav_move(&n,CON_NAV_FIND);assert(!con_nav_step(&n,1024)&&n.cursor.column==1024);while(!con_nav_step(&n,1024));assert(n.cursor.column==0);
dense[9999]='z';con_nav_move(&n,CON_NAV_FIND);while(!con_nav_step(&n,1024));assert(n.cursor.column==9999);free(dense);
printf("Inline find PASS: %zu independent targets, wide cells, hard/soft boundaries and bounded 10000-cell scans\n",sizeof(tests)/sizeof(*tests));return 0;}''')
header=(r/'navigation.h').read_text()
for label,code in [('baseline',header),('backward short skips wide target',header.replace('if(matched->text &&','if(FALSE && matched->text &&')),('hard-break crossing',header.replace('if(con_nav_grid_advance(n,n->reverse)!=1)', 'if(!con_nav_grid_advance(n,n->reverse))')),('failed-search cursor drift',header.replace('n->cursor=n->origin;n->pending=FALSE;return;', 'n->pending=FALSE;return;'))]:
 with tempfile.TemporaryDirectory() as d:
  p=Path(d);src=p/'t.c';exe=p/'t';src.write_text(prefix+code+'\n'.join(body))
  subprocess.run(['cc','-Wall','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined',str(src),'-o',str(exe)],check=True)
  result=subprocess.run([str(exe)],capture_output=True)
  if label=='baseline':assert result.returncode==0,result.stderr;print(result.stdout.decode().strip())
  else:assert result.returncode!=0,label;print('Negative control PASS:',label)
