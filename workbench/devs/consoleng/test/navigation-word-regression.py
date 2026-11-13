#!/usr/bin/env python3
"""Word movement checked against independently tokenized scalar spans."""
from pathlib import Path
import random,subprocess,tempfile
r=Path(__file__).resolve().parents[1]
base=(r/'test/navigation-regression.py').read_text();prefix=base.split("prefix='''",1)[1].split("'''",1)[0]
random.seed(185)
texts=['a b','ab,cd ef','  abc  ','','中,a 中b','ab:cd│ef']
texts += [''.join(random.choice('abc ,:│中') for _ in range(random.randrange(1,24))) for _ in range(100)]
body=['int main(void){unsigned cases=0;']
for text in texts:
 scalars=list(text);cells=[];columns=[]
 for c in scalars:
  columns.append(len(cells));cells.append(ord(c))
  if c=='中':cells.append(0xfffffffe)
 if not scalars:scalars=[' '];columns=[0]
 body.append('{ULONG cells[]={'+','.join(map(str,cells or [0]))+'};struct charmap_line line={0};line.size='+str(len(cells))+';line.text=cells;struct con_nav n;')
 checks=[]
 for semantic in [False,True]:
  spans=[];start=None
  for i,c in enumerate(scalars):
   delim=semantic and c in ',│`|:"\' ()[]{}<>\t'
   if c in ' \t' or delim:
    if start is not None:spans.append((start,i-1));start=None
    if c not in ' \t':spans.append((i,i))
   elif start is None:start=i
  if start is not None:spans.append((start,len(scalars)-1))
  for i,col in enumerate(columns):
   for offset,desired in enumerate([
    next((a for a,b in spans if a>i),len(scalars)-1),
    next((b for a,b in spans if b>i),len(scalars)-1),
    next((a for a,b in reversed(spans) if a<i),0),
   ]):
    op=8+(3 if semantic else 0)+offset
    checks.append('{'+f'{col},{op},{columns[desired]}'+'}')
 body.append('unsigned checks[][3]={'+','.join(checks)+'};for(unsigned i=0;i<sizeof(checks)/sizeof(*checks);i++){con_nav_begin(&n,&line,0,checks[i][0],80);con_nav_move(&n,checks[i][1]);for(unsigned k=0;n.pending;k++){assert(k<100);con_nav_step(&n,1);}assert(n.cursor.column==checks[i][2]);cases++;}}')
body.append('''ULONG a[]={'a','b'},b[]={'c','d'};struct charmap_line lines[2]={{0}};
lines[0].text=a;lines[0].size=2;lines[0].next=&lines[1];lines[1].text=b;lines[1].size=2;lines[1].prev=&lines[0];
struct con_nav n;con_nav_begin(&n,lines,0,0,80);con_nav_move(&n,CON_NAV_WORD_NEXT);while(!con_nav_step(&n,1));assert(n.cursor.row==1&&n.cursor.column==0);
lines[0].wrapColumns=2;con_nav_begin(&n,lines,0,0,80);con_nav_move(&n,CON_NAV_WORD_NEXT);while(!con_nav_step(&n,1));assert(n.cursor.row==1&&n.cursor.column==1);
con_nav_begin(&n,&lines[1],1,1,80);con_nav_move(&n,CON_NAV_WORD_PREV);while(!con_nav_step(&n,1));assert(n.cursor.row==0&&n.cursor.column==0);
ULONG padded[]={'a','b',' '};lines[0].text=padded;lines[0].size=3;lines[0].wrapColumns=3;lines[0].wrapPadding=1;
con_nav_begin(&n,lines,0,0,80);con_nav_move(&n,CON_NAV_WORD_NEXT);while(!con_nav_step(&n,1));assert(n.cursor.row==1&&n.cursor.column==1);
ULONG *dense=malloc(10000*sizeof(*dense));assert(dense);for(unsigned i=0;i<10000;i++)dense[i]='a';
struct charmap_line longword={0};longword.text=dense;longword.size=10000;
con_nav_begin(&n,&longword,0,0,10000);con_nav_move(&n,CON_NAV_WORD_NEXT);
assert(!con_nav_step(&n,1024)&&n.cursor.column==1024);while(!con_nav_step(&n,1024));assert(n.cursor.column==9999);free(dense);
printf("Word navigation PASS: %u independently tokenized semantic/word/wide cases; hard and soft row boundaries\\n",cases);return 0;}''')
with tempfile.TemporaryDirectory() as d:
 p=Path(d);src=p/'t.c';exe=p/'t';src.write_text(prefix+(r/'navigation.h').read_text()+'\n'.join(body))
 subprocess.run(['cc','-Wall','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined',str(src),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
for label,old,new in [
 ('synthetic wrap spacer', 'if(size>boundary)size=boundary;', 'if(FALSE && size>boundary)size=boundary;'),
 ('semantic delimiters', 'if(n->motion>=CON_NAV_SEMANTIC_NEXT', 'if(FALSE && n->motion>=CON_NAV_SEMANTIC_NEXT'),
 ('hard row boundary', 'if(boundary==2 || kind==2', 'if(FALSE || kind==2'),
]:
 with tempfile.TemporaryDirectory() as d:
  p=Path(d);src=p/'t.c';exe=p/'t';header=(r/'navigation.h').read_text();assert old in header
  src.write_text(prefix+header.replace(old,new)+'\n'.join(body))
  subprocess.run(['cc','-Wall','-Werror','-Wno-misleading-indentation',str(src),'-o',str(exe)],check=True)
  result=subprocess.run([str(exe)],capture_output=True)
  assert result.returncode,label
  print('Negative control PASS:',label)
