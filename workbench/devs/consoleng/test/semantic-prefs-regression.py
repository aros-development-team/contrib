#!/usr/bin/env python3
"""Custom semantic selection spans, bounded policy validation and snapshots."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
prefix=(r/'test/navigation-regression.py').read_text().split("prefix='''",1)[1].split("'''",1)[0]
h=(r/'include/consoleng.h').read_text();public=h[h.index('#define CONSOLENG_SEMANTIC_MAX'):h.index('#define CD_GETSELECTIONPROFILE')]
body=r'''
int main(void){
 struct ConsolengSemanticPrefs p=con_semantic_defaults(),bad;
 assert(con_semantic_prefs_valid(&p));
 for(unsigned i=0;i<64;i++){bad=p;bad.sp_Count=65+i;assert(!con_semantic_prefs_valid(&bad));}
 ULONG invalid[]={0,0xd800,0xdfff,0x110000,0xffffffff};
 for(unsigned i=0;i<5;i++){bad=p;bad.sp_Chars[0]=invalid[i];assert(!con_semantic_prefs_valid(&bad));}
 bad=p;bad.sp_Size--;assert(!con_semantic_prefs_valid(&bad));bad=p;bad.sp_Version++;assert(!con_semantic_prefs_valid(&bad));
 bad=p;bad.sp_Chars[1]=bad.sp_Chars[0];assert(!con_semantic_prefs_valid(&bad));bad=p;bad.sp_Chars[63]=1;assert(!con_semantic_prefs_valid(&bad));
 bad=(struct ConsolengSemanticPrefs){sizeof(bad),1,0,{0}};assert(con_semantic_prefs_valid(&bad));
 bad.sp_Count=64;for(unsigned i=0;i<64;i++)bad.sp_Chars[i]=0x10000+i;assert(con_semantic_prefs_valid(&bad));
 ULONG text[]={'a','b','/',0x03a9,'c',',','d','e'};
 struct charmap_line line={0};line.text=text;line.size=8;
 ULONG policies[][3]={{'/',0,0},{',','/',0},{0x03a9,0,0},{0,0,0},{'a','e','c'}};
 unsigned counts[]={1,2,1,0,3},cases=0;
 for(unsigned policy=0;policy<5;policy++)for(unsigned at=0;at<8;at++){
  unsigned lo=at,hi=at;
  while(lo){int stop=0;for(unsigned j=0;j<counts[policy];j++)stop|=text[lo-1]==policies[policy][j];if(stop)break;lo--;}
  while(hi+1<8){int stop=0;for(unsigned j=0;j<counts[policy];j++)stop|=text[hi+1]==policies[policy][j];if(stop)break;hi++;}
  struct con_nav n;con_nav_begin(&n,&line,0,at,8);assert(con_nav_set_delimiters(&n,policies[policy],counts[policy]));
  n.anchor=n.cursor;n.selecting=n.semanticwise=TRUE;unsigned steps=0;while(!con_nav_step(&n,1))assert(++steps<30);
  struct con_search_match m;ULONG first,last;con_nav_match(&n,&m,&first,&last);
  assert(m.first.column==lo&&m.end.column==hi+1);cases++;
 }
 struct con_nav n;con_nav_begin(&n,&line,0,0,8);ULONG custom[]={'/'};assert(con_nav_set_delimiters(&n,custom,1));custom[0]=',';
 assert(con_nav_delimiter(&n,'/')&&!con_nav_delimiter(&n,','));
 assert(!con_nav_set_delimiters(&n,NULL,1));assert(!con_nav_set_delimiters(&n,custom,65));assert(con_nav_delimiter(&n,'/'));
 con_nav_begin(&n,&line,0,0,8);for(ULONG cp=0;cp<0x2600;cp++)assert(con_nav_delimiter(&n,cp)==con_nav_semantic_delimiter(cp));
 printf("Semantic prefs PASS: %u independent custom spans, Unicode/empty/max sets, invalid policy rejection and snapshot lifetime\n",cases);
}
'''
source=prefix+public+(r/'semantic_prefs.h').read_text()+(r/'navigation.h').read_text()+body
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';b=Path(d)/'test';p.write_text(source)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
 subprocess.run([str(b)],check=True)
