#!/usr/bin/env python3
"""Actual rectangular serialization versus independent per-row Unicode slices."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'charmapconclass.c').read_text()
t=(r/'test/softwrap-copy-regression.py').read_text();prefix=t.split("prefix=r'''",1)[1].split("'''",1)[0]
prefix='#include <stdio.h>\n'+'\n'.join(x for x in prefix.splitlines() if not x.startswith('static void check('))
u=(r/'support.c').read_text();u=u[u.index('ULONG con_utf8_encode('):u.index('ULONG con_utf8_count(')]
body=s[s.index('static BOOL charmap_search_space('):s.index('static VOID charmap_search_delete_word(')]+s[s.index('static VOID charmap_block_span('):s.index('/* Overlay only:')]+s[s.index('static ULONG enc_cells_len('):s.index('struct ConClipData')]
rows=[list(map(ord,'abcd  ')),[ord('A'),0x4e2d,0xfffffffe,ord('B')],[],list(map(ord,' x\t ')),[0x3a9,0x2003,ord('z')],[0x4e2d,0xfffffffe,ord('q'),0x4e2d,0xfffffffe],[ord('a'),0xa0],list(map(ord,'tail'))]
main=['int main(void){struct charmap_line rows[8]={0};unsigned cases=0;']
for i,row in enumerate(rows):
 main.append('ULONG t%d[]={%s};'%(i,','.join(map(str,row or [0]))))
 main.append(f'rows[{i}].text=t{i};rows[{i}].size={len(row)};rows[{i}].wrapColumns={len(row) if i%2 else 0};')
main.append('for(unsigned i=0;i<7;i++)rows[i].next=&rows[i+1];')
cases=[]
for a in range(8):
 for b in range(a,8):
  for left in range(8):
   for right in range(left+1,9):
    chunks=[]
    for row in rows[a:b+1]:
     l=min(left,len(row));h=min(right,len(row))
     if l and l<len(row) and row[l]==0xfffffffe:l-=1
     if h<len(row) and row[h]==0xfffffffe:h+=1
     chunks.append(''.join(chr(cp) for cp in row[l:h] if cp!=0xfffffffe).rstrip())
    expected='\r'.join(chunks).encode()
    for reverse in (False,True):cases.append((a,b,right if reverse else left,left if reverse else right,expected))
main.append('struct {unsigned a,b,l,r;const char *text;}tests[]={')
for a,b,l,h,expected in cases:main.append('{%d,%d,%d,%d,"%s"},'%(a,b,l,h,''.join('\\x%02x'%x for x in expected)))
main.append('''};for(unsigned i=0;i<sizeof(tests)/sizeof(*tests);i++){
 ULONG size=charmapcon_calc_selection_size(&rows[tests[i].a],&rows[tests[i].b],tests[i].l,tests[i].r,1,1,1);
 assert(size==strlen(tests[i].text));char *copy=charmapcon_get_selection(size,&rows[tests[i].a],&rows[tests[i].b],tests[i].l,tests[i].r,1,1,1);
 assert(!size||(copy&&!memcmp(copy,tests[i].text,size)));free(copy);cases++;}
printf("Block copy PASS: %u independent rectangles, reversed columns, soft/hard rows, wide edges, Unicode whitespace and empty rows\\n",cases);}
''')
for label,code in [('baseline',body),('lost wide edge',body.replace('if(*left && *left<line->size','if(0 && *left && *left<line->size')),('lost row separators',body.replace('block ? cur != last :','block ? 0 :'))]:
 with tempfile.TemporaryDirectory() as d:
  p=Path(d);src=p/'t.c';exe=p/'t';src.write_text(prefix+u+code+'\n'.join(main))
  subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(src),'-o',str(exe)],check=True)
  result=subprocess.run([str(exe)],capture_output=True)
  if label=='baseline':assert result.returncode==0,result.stderr;print(result.stdout.decode().strip())
  else:assert result.returncode!=0,label;print('Negative control PASS:',label)
