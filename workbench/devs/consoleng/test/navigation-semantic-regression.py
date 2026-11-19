#!/usr/bin/env python3
"""Independent delimiter/bracket span oracle for semantic selection."""
from pathlib import Path
import random,subprocess,tempfile
r=Path(__file__).resolve().parents[1]
prefix=(r/'test/navigation-regression.py').read_text().split("prefix='''",1)[1].split("'''",1)[0]
delimiters=set(',│`|:"\' ()[]{}<>\t');pairs={'(':')','[':']','{':'}','<':'>'}
random.seed(185)
texts=['alpha,beta gamma','(a(b)c)','[x] {y}','unmatched (word','a\tb c','中ab,Ωd']
texts+=[''.join(random.choice('ab ,.()[]{}<>') for _ in range(random.randrange(1,16))) for _ in range(35)]
body=['int main(void){struct con_nav n;struct con_search_match m;ULONG first,last;unsigned cases=0;']
for text in texts:
 cells=[];positions=[]
 for c in text:
  positions.append(len(cells));cells.append(ord(c))
  if c=='中':cells.append(0xfffffffe)
 body.append('{ULONG cells[]={'+','.join(map(str,cells))+'};struct charmap_line line={0};line.text=cells;line.size='+str(len(cells))+';')
 expected=[]
 for a in range(len(text)):
  for b in range(len(text)):
   lo,hi=sorted([a,b]);matched=False
   if a==b:
    c=text[a]
    if c in pairs or c in pairs.values():
     forward=c in pairs;peer=pairs[c] if forward else next(k for k,v in pairs.items() if v==c);depth=0
     for j in range(a+1,len(text)) if forward else range(a-1,-1,-1):
      if text[j]==c:depth+=1
      elif text[j]==peer:
       if not depth:lo,hi=sorted([a,j]);matched=True;break
       depth-=1
   if not matched:
    while lo and text[lo-1] not in delimiters:lo-=1
    while hi+1<len(text) and text[hi+1] not in delimiters:hi+=1
   expected.append((positions[a],positions[b],positions[lo],positions[hi]+(2 if text[hi]=='中' else 1)))
 body.append('unsigned tests[][4]={'+','.join('{%d,%d,%d,%d}'%x for x in expected)+'};')
 body.append('''for(unsigned i=0;i<sizeof(tests)/sizeof(*tests);i++){
 con_nav_begin(&n,&line,0,tests[i][1],80);n.anchor=(struct con_nav_cursor){&line,0,tests[i][0]};n.selecting=n.semanticwise=TRUE;
 assert(!con_nav_step(&n,0));unsigned count=0;while(!con_nav_step(&n,1))assert(++count<100);
 con_nav_match(&n,&m,&first,&last);assert(first==0&&last==0&&m.first.column==tests[i][2]&&m.end.column==tests[i][3]);cases++;}}
''')
body.append(r"""{
 ULONG a[]={'a','b'},b[]={'c','d'},c[]={'e','f'};struct charmap_line rows[3]={0};
 rows[0].text=a;rows[1].text=b;rows[2].text=c;
 for(unsigned i=0;i<3;i++){rows[i].size=2;rows[i].prev=i?&rows[i-1]:NULL;rows[i].next=i<2?&rows[i+1]:NULL;}
 rows[0].wrapColumns=2;con_nav_begin(&n,&rows[1],1,0,2);n.anchor=n.cursor;n.selecting=n.semanticwise=TRUE;
 while(!con_nav_step(&n,1));con_nav_match(&n,&m,&first,&last);assert(first==0&&last==1&&m.first.column==0&&m.end.column==2);
 a[0]='(';c[1]=')';con_nav_begin(&n,rows,0,0,2);n.anchor=n.cursor;n.selecting=n.semanticwise=TRUE;
 while(!con_nav_step(&n,1));con_nav_match(&n,&m,&first,&last);assert(first==0&&last==2&&m.first.column==0&&m.end.column==2);
}
""")
body.append('''ULONG *dense=malloc(10000*sizeof(*dense));assert(dense);for(unsigned i=0;i<10000;i++)dense[i]='a';
struct charmap_line line={0};line.text=dense;line.size=10000;
con_nav_begin(&n,&line,0,5000,10000);n.anchor=n.cursor;n.selecting=n.semanticwise=TRUE;
assert(!con_nav_step(&n,1024)&&n.selection_first.column>=3976);while(!con_nav_step(&n,1024));
con_nav_match(&n,&m,&first,&last);assert(m.first.column==0&&m.end.column==10000);
dense[0]='(';con_nav_begin(&n,&line,0,0,10000);n.anchor=n.cursor;n.selecting=n.semanticwise=TRUE;
assert(!con_nav_step(&n,1024));while(!con_nav_step(&n,1024));con_nav_match(&n,&m,&first,&last);assert(m.first.column==0&&m.end.column==10000);
free(dense);printf("Semantic selection PASS: %u independent spans and bounded 10000-cell/unmatched scans\\n",cases);}
''')
header=(r/'navigation.h').read_text()
for label,code in [('baseline',header),('delimiter boundary',header.replace('moved==1 && !con_nav_delimiter(n,cp)','moved==1 && (con_nav_delimiter(n,cp),TRUE)')),('bracket preference',header.replace('n->match_phase=n->anchor.row==n->cursor.row && n->anchor.column==n->cursor.column?1:3','n->match_phase=3'))]:
 with tempfile.TemporaryDirectory() as d:
  p=Path(d);src=p/'t.c';exe=p/'t';src.write_text(prefix+code+'\n'.join(body))
  subprocess.run(['cc','-Wall','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined',str(src),'-o',str(exe)],check=True)
  result=subprocess.run([str(exe)],capture_output=True)
  if label=='baseline':assert result.returncode==0,result.stderr;print(result.stdout.decode().strip())
  else:assert result.returncode!=0,label;print('Negative control PASS:',label)
