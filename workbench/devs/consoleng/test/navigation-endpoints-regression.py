#!/usr/bin/env python3
"""Independent pinned-reference endpoint oracle, including physical blank columns."""
from pathlib import Path
import random,subprocess,tempfile
r=Path(__file__).resolve().parents[1]
prefix=(r/'test/navigation-regression.py').read_text().split("prefix='''",1)[1].split("'''",1)[0]
random.seed(185);rows=[[],list(map(ord,'   abc  ')),[0x4e2d,0xfffffffe],list(map(ord,'        '))];soft=[False,True,True,False]
for i in range(76):
 row=[]
 while len(row)<random.randrange(9):
  if len(row)<7 and random.randrange(5)==0:row += [0x4e2d,0xfffffffe]
  else:row.append(random.choice([0,32,9,97,98,0x3a9]))
 rows.append(row);soft.append(random.choice([False,True]))
soft[-1]=False
# A real soft-wrapped row fills the grid; absent cells are ordinary blanks.
def occupied(y):return [x for x,c in enumerate(rows[y]) if c not in (0,32,9)]
def clamp(y,x):return x-1 if x<len(rows[y]) and rows[y][x]==0xfffffffe else x
def motion(y,x,last):
 if last:
  current=x+int(x+1<len(rows[y]) and rows[y][x+1]==0xfffffffe)
  col=occupied(y)[-1] if occupied(y) else 0
  if current<col:return y,clamp(y,col)
  if soft[y] and current==7:
   while soft[y]:y+=1
   col=occupied(y)[-1] if occupied(y) else current
  else:col=7
  return y,clamp(y,col)
 col=occupied(y)[0] if occupied(y) else 7
 if x!=col:return y,clamp(y,col)
 prior=y
 while prior and soft[prior-1]:prior-=1
 for yy in range(prior,y):
  if occupied(yy):return yy,clamp(yy,occupied(yy)[0])
 while not occupied(y) and soft[y]:y+=1
 return y,clamp(y,occupied(y)[0] if occupied(y) else 7)
body=['int main(void){struct charmap_line rows[80]={0};struct con_nav n;unsigned cases=0;']
for i,row in enumerate(rows):
 body.append('ULONG t'+str(i)+'[]={'+','.join(map(str,row or [0]))+'};')
 body.append(f'rows[{i}].text=t{i};rows[{i}].size={len(row)};rows[{i}].wrapColumns={8 if soft[i] else 0};')
body.append('for(unsigned i=0;i<80;i++){rows[i].prev=i?&rows[i-1]:NULL;rows[i].next=i<79?&rows[i+1]:NULL;}')
for y,row in enumerate(rows):
 for x in range(8):
  if clamp(y,x)!=x:continue
  for last in [False,True]:
   yy,xx=motion(y,x,last);zy,zx=motion(yy,xx,last);op='CON_NAV_LAST' if last else 'CON_NAV_OCCUPIED'
   body.append(f'con_nav_begin(&n,&rows[{y}],{y},{x},8);con_nav_move(&n,{op});assert(!con_nav_step(&n,0));for(unsigned k=0;n.pending;k++){{assert(k<2000);con_nav_step(&n,1);}}assert(n.cursor.row=={yy}&&n.cursor.column=={xx});con_nav_move(&n,{op});for(unsigned k=0;n.pending;k++){{assert(k<2000);con_nav_step(&n,1);}}assert(n.cursor.row=={zy}&&n.cursor.column=={zx});cases+=2;')
body.append(r'''ULONG *dense=calloc(10000,sizeof(*dense));assert(dense);dense[9999]='x';struct charmap_line line={0};line.size=10000;line.text=dense;
con_nav_begin(&n,&line,0,0,10000);con_nav_move(&n,CON_NAV_OCCUPIED);assert(!con_nav_step(&n,1024)&&n.scan_column==1024);while(!con_nav_step(&n,1024));assert(n.cursor.column==9999);
dense[9999]=0;dense[0]='x';con_nav_begin(&n,&line,0,0,10000);con_nav_move(&n,CON_NAV_LAST);assert(!con_nav_step(&n,1024)&&n.scan_column==8976);while(!con_nav_step(&n,1024));assert(n.cursor.column==9999);free(dense);
struct charmap_line *many=calloc(10000,sizeof(*many));assert(many);for(unsigned i=0;i<10000;i++){many[i].wrapColumns=i<9999?8:0;many[i].prev=i?&many[i-1]:NULL;many[i].next=i<9999?&many[i+1]:NULL;}
con_nav_begin(&n,many,0,7,8);con_nav_move(&n,CON_NAV_LAST);assert(!con_nav_step(&n,1024)&&n.cursor.row<1024);while(!con_nav_step(&n,1024));assert(n.cursor.row==9999&&n.cursor.column==7);
con_nav_begin(&n,&many[9999],9999,7,8);con_nav_move(&n,CON_NAV_OCCUPIED);assert(!con_nav_step(&n,1024)&&n.cursor.row>8975);while(!con_nav_step(&n,1024));assert(n.cursor.row==9999&&n.cursor.column==7);free(many);
printf("Endpoint PASS: %u independent repeated targets; 10000-cell and row bounds\n",cases);return 0;}''')
header=(r/'navigation.h').read_text()
for label,code in [('baseline',header),('lost right-edge toggle',header.replace('else n->cursor.column=n->width?n->width-1:0;','else n->cursor.column=occupied;')),('lost wrapped first occupied',header.replace('if(prev && n->cursor.row && prev->wrapColumns)','if(FALSE && prev && n->cursor.row && prev->wrapColumns)'))]:
 with tempfile.TemporaryDirectory() as d:
  p=Path(d);src=p/'t.c';exe=p/'t';src.write_text(prefix+code+'\n'.join(body))
  subprocess.run(['cc','-Wall','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined',str(src),'-o',str(exe)],check=True)
  result=subprocess.run([str(exe)],capture_output=True)
  if label=='baseline':assert result.returncode==0,result.stderr;print(result.stdout.decode().strip())
  else:assert result.returncode!=0,label;print('Negative control PASS:',label)
