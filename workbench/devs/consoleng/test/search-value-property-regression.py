#!/usr/bin/env python3
"""Pinned Age and break-property membership and scalar boundaries."""
from pathlib import Path
import ast,contextlib,importlib.util,io,json,re,subprocess,tempfile
here=Path(__file__).resolve().parent;root=here.parent
spec=importlib.util.spec_from_file_location('audit',here/'search-reference-audit.py')
audit=importlib.util.module_from_spec(spec);spec.loader.exec_module(audit)
data=json.loads((here/'search-value-property-cases.json').read_text());assert data['reference']==audit.PIN
subprocess.run(['python3',str(root/'tools/generate-search-unicode.py'),'--check'],check=True)
categories=[]
for family,filename in [('age','age.rs'),('gcb','grapheme_cluster_break.rs'),('sb','sentence_break.rs'),('wb','word_break.rs')]:
 raw=(root/'unicode/regex-syntax-0.8.6'/filename).read_text()
 blocks=re.findall(r'pub const ([A-Z0-9_]+):[^=]+=\s*&\[(.*?)\];',raw,re.S)
 for name,body in blocks:
  if name=='BY_NAME':continue
  pairs=[]
  for pair in re.findall(r"\('(?:\\.|[^'])*', '(?:\\.|[^'])*'\)",body):
   pair=re.sub(r'\\u\{([0-9a-fA-F]+)\}',lambda m:'\\U%08x'%int(m[1],16),pair)
   pairs.append(tuple(map(ord,ast.literal_eval(pair))))
  assert pairs,name
  categories.append((family,name,pairs))
# Age means assigned at or before this version, independently unioned here.
original=list(categories)
version=lambda name:tuple(map(int,name[1:].split('_')))
categories=[(family,name,[pair for f,n,ps in original if f=='age' and version(n)<=version(name) for pair in ps] if family=='age' else pairs) for family,name,pairs in original]
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
  for family,name,pairs in categories:
   expected=bytearray(0x110000);points=set()
   for lo,hi in pairs:
    expected[lo:hi+1]=b'\1'*(hi-lo+1)
    points.update(x for x in (lo-1,lo,hi,hi+1) if 0<x<=0x10ffff and not 0xd800<=x<=0xdfff)
   expected[0xd800:0xe000]=b'\0'*0x800
   oracle.write(expected)
   alias=name.lower().replace('_','');ident=family+'_'+name
   main+='assert(fread(expected,1,0x110000,f)==0x110000);\n'
   main+='ULONG id_'+ident+'=999; for(unsigned i=0;i<sizeof(rx_'+family+'_aliases)/sizeof(*rx_'+family+'_aliases);i++)if(!strcmp(rx_'+family+'_aliases[i].name,"'+alias+'"))id_'+ident+'=rx_'+family+'_aliases[i].id;assert(id_'+ident+'!=999);\n'
   main+='for(ULONG cp=0;cp<0x110000;cp++)assert(rx_'+family+'_member(id_'+ident+',cp)==expected[cp]);\n'
   main+='{const ULONG points[]={'+','.join(map(str,sorted(points)))+'};\n'
   main+='for(unsigned i=0;i<sizeof(points)/sizeof(*points);i++){ULONG cp=points[i];\n'
   for pattern in [r'(?-i:\p{'+family+'='+name+'})',r'(?-i:[\p{'+family+'='+name+'}])',r'(?-i:\P{'+family+'='+name+'})']:
    wanted='!expected[cp]' if r'\P' in pattern else 'expected[cp]'
    main+='strcpy(pattern,'+json.dumps(pattern)+');verify(cp,pattern,'+wanted+');scans++;\n'
   main+='}}\n'
 main+=r'''
 verify('A',"\\p{Age=1.1}",TRUE);verify(0x1f600,"\\p{Age=1.1}",FALSE);
 verify(0x1f600,"\\p{Age=6.0}",FALSE);verify(0x1f600,"\\p{Age=6.1}",TRUE);
 verify(0x301,"\\p{GCB=Extend}",TRUE);verify('A',"\\p{WB=ALetter}",TRUE);verify('.',"\\p{SB=ATerm}",TRUE);
 const char *bad[]={"\\p{Age=99.0}","\\p{gcb=NoSuchValue}","\\p{wb=Latin}","\\p{sb=Emoji}","\\p{gc=V1_1}","(?-u:\\p{Age=1.1})"};
 for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++){ULONG q[128];unsigned n=strlen(bad[i]);for(unsigned j=0;j<n;j++)q[j]=bad[i][j];assert(!con_regex_select_begin(&selection,NULL,q,n,0,0,NULL));}
 fclose(f);free(expected);printf("Value-property membership and %u scalar engine scans PASS\n",scans);
}
'''
 (work/'category.c').write_text(body+main)
 subprocess.run(['cc','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(work/'category.c'),'-o',str(work/'category')],check=True)
 subprocess.run([str(work/'category'),str(work/'oracle.bin')],check=True)
 # A real Age/Script namespace distinction must fail if namespaces alias.
 broken=(body+main).replace('return rx_age_member(id & 0xffff, cp);','return rx_sc_member(id & 0xffff, cp);')
 assert broken!=body+main
 (work/'broken.c').write_text(broken)
 subprocess.run(['cc','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(work/'broken.c'),'-o',str(work/'broken')],check=True)
 result=subprocess.run([str(work/'broken'),str(work/'oracle.bin')],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
 assert result.returncode!=0,'Age alias mutation was not detected'
 print('Negative control PASS: Age is distinct from Script')
