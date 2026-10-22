#!/usr/bin/env python3
"""Pinned general-category spans, exhaustive membership and scalar boundaries."""
from pathlib import Path
import ast,contextlib,importlib.util,io,json,re,subprocess,tempfile
here=Path(__file__).resolve().parent;root=here.parent
spec=importlib.util.spec_from_file_location('audit',here/'search-reference-audit.py')
audit=importlib.util.module_from_spec(spec);spec.loader.exec_module(audit)
data=json.loads((here/'search-category-cases.json').read_text());assert data['reference']==audit.PIN
subprocess.run(['python3',str(root/'tools/generate-search-unicode.py'),'--check'],check=True)
raw=(root/'unicode/regex-syntax-0.8.6/general_category.rs').read_text()
blocks=re.findall(r'pub const ([A-Z_]+):[^=]+=\s*&\[(.*?)\];',raw,re.S)
categories=[]
for name,body in blocks:
 if name=='BY_NAME':continue
 # Parse original Rust character tuples independently of the generator.
 pairs=[]
 for pair in re.findall(r"\('(?:\\.|[^'])*', '(?:\\.|[^'])*'\)",body):
  pair=re.sub(r'\\u\{([0-9a-fA-F]+)\}',lambda m:'\\U%08x'%int(m[1],16),pair)
  pairs.append(tuple(map(ord,ast.literal_eval(pair))))
 assert pairs,name
 categories.append((name,pairs))
with tempfile.TemporaryDirectory() as d:
 work=Path(d);assert not audit.production(work,data['cases'],data['columns'])
 body=(work/'production.c').read_text().split('int main(void) {',1)[0]
 # Keep the reference harness selection object used under -Werror.
 main=r'''
static void verify(ULONG cp,const char *pattern,BOOL wanted){
 ULONG cells[]={cp,0xfffffffe},query[128];unsigned n=strlen(pattern);
 for(unsigned j=0;j<n;j++)query[j]=(UBYTE)pattern[j];
 struct charmap_line row={0};row.text=cells;row.size=2;
 assert(con_regex_select_begin(&selection,&row,query,n,0,0,NULL));
 struct con_search_match found;enum con_search_result result;unsigned ticks=0;
 do{result=con_regex_select_step(&selection,1,&found);assert(++ticks<10000);}while(result==CON_SEARCH_MORE);
 if((result==CON_SEARCH_FOUND)!=wanted){fprintf(stderr,"U+%x %s expected %d\n",cp,pattern,wanted);abort();}
}
int main(int argc,char **argv){
 assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);
 unsigned char *expected=malloc(0x110000);assert(expected);
 char pattern[128];unsigned scans=0;
'''
 with (work/'oracle.bin').open('wb') as oracle:
  for name,pairs in categories:
   expected=bytearray(0x110000);points=set()
   for lo,hi in pairs:
    expected[lo:hi+1]=b'\1'*(hi-lo+1)
    points.update(x for x in (lo-1,lo,hi,hi+1) if 0<x<=0x10ffff and not 0xd800<=x<=0xdfff)
   expected[0xd800:0xe000]=b'\0'*0x800
   oracle.write(expected)
   alias=name.lower().replace('_','')
   main+='assert(fread(expected,1,0x110000,f)==0x110000);\n'
   main+='ULONG id_'+name+'=999; for(unsigned i=0;i<sizeof(rx_gc_aliases)/sizeof(*rx_gc_aliases);i++)if(!strcmp(rx_gc_aliases[i].name,"'+alias+'"))id_'+name+'=rx_gc_aliases[i].id;assert(id_'+name+'!=999);\n'
   main+='for(ULONG cp=0;cp<0x110000;cp++)assert(rx_gc_member(id_'+name+',cp)==expected[cp]);\n'
   main+='{const ULONG points[]={'+','.join(map(str,sorted(points)))+'};\n'
   main+='for(unsigned i=0;i<sizeof(points)/sizeof(*points);i++){ULONG cp=points[i];\n'
   for pattern in [r'(?-i:\p{'+name+'})',r'(?-i:[\p{'+name+'}])',r'(?-i:\P{'+name+'})']:
    wanted='!expected[cp]' if r'\P' in pattern else 'expected[cp]'
    main+='strcpy(pattern,'+json.dumps(pattern)+');verify(cp,pattern,'+wanted+');scans++;\n'
   main+='}}\n'
 main+=r'''
 verify(0x212a,"(?i:\\p{Lu})",TRUE);verify('k',"(?i:\\p{Lu})",TRUE);
 verify('k',"(?i:\\P{Lu})",FALSE);verify(0x3c9,"(?i:\\p{Lu})",TRUE);
 verify(0x3c9,"(?i:\\P{Lu})",FALSE);
 const char *bad[]={"\\p{Cs}","\\p{Surrogate}","\\p{NoSuchCategory}","(?-u:\\p{Lu})","[a-\\p{Lu}]","[\\p{Lu}-z]"};
 for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++){ULONG q[128];unsigned n=strlen(bad[i]);for(unsigned j=0;j<n;j++)q[j]=bad[i][j];assert(!con_regex_select_begin(&selection,NULL,q,n,0,0,NULL));}
 fclose(f);free(expected);printf("Category membership and %u scalar engine scans PASS\n",scans);
}
'''
 (work/'category.c').write_text(body+main)
 subprocess.run(['cc','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(work/'category.c'),'-o',str(work/'category')],check=True)
 subprocess.run([str(work/'category'),str(work/'oracle.bin')],check=True)
for label,old,new in [('category complement','if(range.high & 0x40000000UL)member=!member;','if(range.high & 0x40000000UL)member=member;'),('category folding','if(rx_gc_member(id,equiv->other[k]))member=TRUE;','if(rx_gc_member(id,equiv->other[k]))member=member;')]:
 def change(source):assert old in source;return source.replace(old,new)
 with tempfile.TemporaryDirectory() as d:
  with contextlib.redirect_stdout(io.StringIO()):failed=audit.production(Path(d),data['cases'],data['columns'],change)
  assert failed,label
 print('Negative control PASS:',label)
