#!/usr/bin/env python3
"""Independent logical-line selection endpoints and bounded expansion."""
from pathlib import Path
import random,subprocess,tempfile
r=Path(__file__).resolve().parents[1]
prefix=(r/'test/navigation-regression.py').read_text().split("prefix='''",1)[1].split("'''",1)[0]
random.seed(185);soft=[random.randrange(2) for _ in range(49)]+[0]
expected=[]
for anchor in range(50):
 for cursor in range(50):
  a,b=sorted([anchor,cursor])
  while a and soft[a-1]:a-=1
  while soft[b]:b+=1
  expected.append((a,b))
body='int main(void){struct charmap_line rows[50]={0};struct con_nav n;struct con_search_match m;ULONG first,last;'
body+='unsigned soft[]={'+','.join(map(str,soft))+'};unsigned expected[][2]={'+','.join('{%d,%d}'%x for x in expected)+'};'
body+='''for(unsigned i=0;i<50;i++){rows[i].wrapColumns=soft[i]?80:0;rows[i].prev=i?&rows[i-1]:NULL;rows[i].next=i<49?&rows[i+1]:NULL;}
for(unsigned a=0;a<50;a++)for(unsigned b=0;b<50;b++){
 con_nav_begin(&n,&rows[b],b,0,80);n.anchor=(struct con_nav_cursor){&rows[a],a,0};n.selecting=n.linewise=TRUE;
 assert(!con_nav_step(&n,0)&&!n.match_ready);
 unsigned count=0;while(!con_nav_step(&n,1))assert(++count<110);
 con_nav_match(&n,&m,&first,&last);assert(first==expected[a*50+b][0]&&last==expected[a*50+b][1]);
 assert(m.first.line==&rows[first]&&m.end.line==&rows[last]&&m.first.column==0&&m.end.column==80);
}
struct charmap_line *many=calloc(10000,sizeof(*many));assert(many);
for(unsigned i=0;i<10000;i++){many[i].wrapColumns=80;many[i].prev=i?&many[i-1]:NULL;many[i].next=i<9999?&many[i+1]:NULL;}
con_nav_begin(&n,&many[5000],5000,0,80);n.anchor=n.cursor;n.selecting=n.linewise=TRUE;
assert(!con_nav_step(&n,1024)&&n.selection_first.row>=3976);
while(!con_nav_step(&n,1024));con_nav_match(&n,&m,&first,&last);assert(first==0&&last==9999);free(many);
puts("Line selection PASS: 2500 independent endpoint pairs and bounded 10000-row expansion");}
'''
header=(r/'navigation.h').read_text()
for label,code in [('baseline',header),('hard boundary',header.replace('line->prev->wrapColumns','TRUE').replace('line->wrapColumns && line->next','line->next')),('full final row',header.replace('n->selection_last.line,n->width','n->selection_last.line,0'))]:
 with tempfile.TemporaryDirectory() as d:
  p=Path(d);src=p/'t.c';exe=p/'t';src.write_text(prefix+code+body)
  subprocess.run(['cc','-Wall','-Werror','-Wno-misleading-indentation',str(src),'-o',str(exe)],check=True)
  result=subprocess.run([str(exe)],capture_output=True)
  if label=='baseline':assert result.returncode==0,result.stderr;print(result.stdout.decode().strip())
  else:assert result.returncode!=0,label;print('Negative control PASS:',label)
