#!/usr/bin/env python3
"""Pinned binary-property spans, membership and scalar boundaries."""
from pathlib import Path
import ast,contextlib,importlib.util,io,json,re,subprocess,tempfile
here=Path(__file__).resolve().parent;root=here.parent
spec=importlib.util.spec_from_file_location('audit',here/'search-reference-audit.py')
audit=importlib.util.module_from_spec(spec);spec.loader.exec_module(audit)
data=json.loads((here/'search-binary-property-cases.json').read_text());assert data['reference']==audit.PIN
subprocess.run(['python3',str(root/'tools/generate-search-unicode.py'),'--check'],check=True)
categories=[]
for family,filename in [('bp','property_bool.rs')]:
 raw=(root/'unicode/regex-syntax-0.8.6'/filename).read_text()
 blocks=re.findall(r'pub const ([A-Z_]+):[^=]+=\s*&\[(.*?)\];',raw,re.S)
 for name,body in blocks:
  if name in ('BY_NAME','INCB'):continue
  pairs=[]
  for pair in re.findall(r"\('(?:\\.|[^'])*', '(?:\\.|[^'])*'\)",body):
   pair=re.sub(r'\\u\{([0-9a-fA-F]+)\}',lambda m:'\\U%08x'%int(m[1],16),pair)
   pairs.append(tuple(map(ord,ast.literal_eval(pair))))
  assert pairs,name
  categories.append((family,name,pairs))
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
   for pattern in [r'(?-i:\p{'+name+'})',r'(?-i:[\p{'+name+'}])',r'(?-i:\P{'+name+'})']:
    wanted='!expected[cp]' if r'\P' in pattern else 'expected[cp]'
    main+='strcpy(pattern,'+json.dumps(pattern)+');verify(cp,pattern,'+wanted+');scans++;\n'
   main+='}}\n'
 main+=r'''
 verify(0x1f600,"\\p{Emoji}",TRUE);verify('0',"\\p{Emoji}",TRUE);
 verify('0',"\\p{Emoji_Presentation}",FALSE);verify(0x301,"\\p{Grapheme_Extend}",TRUE);
 verify(0x2160,"\\p{Uppercase}",TRUE);verify(0x2160,"\\p{Lu}",FALSE);
 verify(0x3c9,"(?i:\\p{Uppercase})",TRUE);verify(0x3c9,"(?i:\\P{Uppercase})",FALSE);
 const char *bad[]={"\\p{InCB}","\\p{Alphabetic=Yes}","\\p{gc=Alphabetic}","\\p{sc=Emoji}","(?-u:\\p{Emoji})","[\\p{Emoji}-z]"};
 for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++){ULONG q[128];unsigned n=strlen(bad[i]);for(unsigned j=0;j<n;j++)q[j]=bad[i][j];assert(!con_regex_select_begin(&selection,NULL,q,n,0,0,NULL));}
 fclose(f);free(expected);printf("Binary-property membership and %u scalar engine scans PASS\n",scans);
}
'''
 (work/'category.c').write_text(body+main)
 subprocess.run(['cc','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(work/'category.c'),'-o',str(work/'category')],check=True)
 subprocess.run([str(work/'category'),str(work/'oracle.bin')],check=True)
 # A real Binary-property namespace must fail if namespaces alias.
 broken=(body+main).replace('return rx_bp_member(id & 0xffff, cp);','return rx_sc_member(id & 0xffff, cp);')
 assert broken!=body+main
 (work/'broken.c').write_text(broken)
 subprocess.run(['cc','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(work/'broken.c'),'-o',str(work/'broken')],check=True)
 result=subprocess.run([str(work/'broken'),str(work/'oracle.bin')],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
 assert result.returncode!=0,'Binary-property alias mutation was not detected'
 print('Negative control PASS: binary property is distinct from Script')
