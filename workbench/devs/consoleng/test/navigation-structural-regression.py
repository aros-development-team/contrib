#!/usr/bin/env python3
"""Independent paired-bracket and paragraph targets for retained navigation."""
from pathlib import Path
import random,subprocess,tempfile
r=Path(__file__).resolve().parents[1]
prefix=(r/'test/navigation-regression.py').read_text().split("prefix='''",1)[1].split("'''",1)[0]
random.seed(185)
texts=['(a(b)c)','[a] {b} <c>','unmatched ((((',')x','([)]','']
texts+=[''.join(random.choice('ab ()[]{}<>') for _ in range(random.randrange(1,35))) for _ in range(80)]
body=['int main(void){unsigned cases=0;']
pairs={'(':')','[':']','{':'}','<':'>'}
for text in texts:
 expected=[]
 for i,c in enumerate(text):
  target=i
  if c in pairs or c in pairs.values():
   forward=c in pairs;peer=pairs[c] if forward else next(a for a,b in pairs.items() if b==c)
   depth=0
   for j in range(i+1,len(text)) if forward else range(i-1,-1,-1):
    if text[j]==c:depth+=1
    elif text[j]==peer:
     if not depth:target=j;break
     depth-=1
  expected.append(target)
 body.append('{ULONG cells[]={'+','.join(map(str,map(ord,text)))+('0' if not text else '')+'};struct charmap_line line={0};line.text=cells;line.size='+str(len(text))+';struct con_nav n;')
 body.append('unsigned expected[]={'+','.join(map(str,expected or [0]))+'};for(unsigned i=0;i<sizeof(expected)/sizeof(*expected);i++){con_nav_begin(&n,&line,0,i,80);con_nav_move(&n,CON_NAV_BRACKET);for(unsigned k=0;n.pending;k++){assert(k<100);con_nav_step(&n,1);}assert(n.cursor.column==expected[i]);cases++;}}')
rows=['',' ','a','b','','c','  ','','d','']
body.append('{struct charmap_line rows[10]={{0}};struct con_nav n;')
for i,text in enumerate(rows):body.append('ULONG row'+str(i)+'[]={'+','.join(map(str,map(ord,text)))+('0' if not text else '')+'};rows['+str(i)+'].text=row'+str(i)+';rows['+str(i)+'].size='+str(len(text))+';')
body.append('for(unsigned i=0;i<10;i++){if(i)rows[i].prev=&rows[i-1];if(i<9)rows[i].next=&rows[i+1];}')
for start in range(10):
 for direction in [-1,1]:
  target=start;seen=False
  while True:
   empty=not rows[target].strip(' \t')
   if empty and seen:break
   if not empty:seen=True
   if target+direction not in range(10):break
   target+=direction
  op='CON_NAV_PARAGRAPH_UP' if direction<0 else 'CON_NAV_PARAGRAPH_DOWN'
  body.append(f'con_nav_begin(&n,&rows[{start}],{start},0,80);con_nav_move(&n,{op});for(unsigned k=0;n.pending;k++){{assert(k<100);con_nav_step(&n,1);}}assert(n.cursor.row=={target}&&n.cursor.column==0);cases++;')
body.append('}')
body.append('''ULONG *dense=malloc(10000*sizeof(*dense));assert(dense);for(unsigned i=0;i<10000;i++)dense[i]='x';dense[0]='(';dense[9999]=')';
struct charmap_line line={0};line.text=dense;line.size=10000;struct con_nav n;
con_nav_begin(&n,&line,0,0,10000);con_nav_move(&n,CON_NAV_BRACKET);assert(!con_nav_step(&n,1024)&&n.cursor.column<=1024);while(!con_nav_step(&n,1024));assert(n.cursor.column==9999);
dense[9999]='x';con_nav_begin(&n,&line,0,0,10000);con_nav_move(&n,CON_NAV_BRACKET);while(!con_nav_step(&n,1024));assert(n.cursor.column==0);free(dense);
printf("Structural navigation PASS: %u independent bracket/paragraph targets and bounded/unmatched scans\\n",cases);return 0;}''')
with tempfile.TemporaryDirectory() as d:
 p=Path(d);src=p/'t.c';exe=p/'t';src.write_text(prefix+(r/'navigation.h').read_text()+'\n'.join(body))
 subprocess.run(['cc','-Wall','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined',str(src),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
for label,old,new in [
 ('nested brackets', 'n->depth++;', 'n->depth+=0;'),
 ('unmatched restoration', 'if(!con_nav_advance(n,n->reverse)){n->cursor=n->origin;', 'if(!con_nav_advance(n,n->reverse)){'),
]:
 with tempfile.TemporaryDirectory() as d:
  p=Path(d);src=p/'t.c';exe=p/'t';header=(r/'navigation.h').read_text();assert old in header
  src.write_text(prefix+header.replace(old,new)+'\n'.join(body))
  subprocess.run(['cc','-Wall','-Werror','-Wno-misleading-indentation',str(src),'-o',str(exe)],check=True)
  assert subprocess.run([str(exe)],capture_output=True).returncode,label
  print('Negative control PASS:',label)
