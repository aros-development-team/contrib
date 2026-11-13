#!/usr/bin/env python3
"""Real retained URL detection and ReadArgs quoting, with UTF-8/soft wraps."""
from pathlib import Path
from hyperlink_fixture import hyperlink_source
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'url_action.c').read_text();s=s[s.index('#define CON_URL_MAX'):s.index('/* Console input runs')]
h=(r/'charmap.h').read_text();h=h[h.index('struct charmap_line\n'):h.index('VOID charmap_dispose_lines')]
u=(r/'support.c').read_text();u=u[u.index('ULONG con_utf8_encode('):u.index('\n}',u.index('ULONG con_utf8_encode('))+2]
w=(r/'support.c').read_text();w=w[w.index('WORD con_cp_width('):w.index('\n}',w.index('WORD con_cp_width('))+2]
prefix=r'''
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
typedef int16_t WORD;typedef uint64_t UQUAD;typedef uint32_t ULONG;typedef int32_t LONG;typedef uint8_t UBYTE;typedef int8_t BYTE;typedef int BOOL;typedef void VOID;
#define TRUE 1
#define FALSE 0
#define CELL_WIDE_CONT 0xfffffffeUL
'''
main=r'''
static ULONG chars[2048];
static struct charmap_line row(const char*s){ULONG n=strlen(s);for(ULONG i=0;i<n;i++)chars[i]=(UBYTE)s[i];return (struct charmap_line){.size=n,.text=chars};}
int main(void){UBYTE out[2048],args[2050];
 struct charmap_line ordinary=row("History row 046 - scroll verification");ULONG plainEnd=0;
 assert(!con_url_run(&ordinary,0,&plainEnd)&&plainEnd==ordinary.size);
 struct charmap_line mixed=row("History row 046 https://example.org tail words");ULONG mixedEnd=0;
 assert(!con_url_run(&mixed,0,&mixedEnd)&&mixedEnd==16);
 assert(con_url_run(&mixed,mixedEnd,&mixedEnd)&&mixedEnd==35);
 assert(!con_url_run(&mixed,mixedEnd,&mixedEnd)&&mixedEnd==mixed.size);
 const char*texts[]={"https://example.org/a?x=1&y=2#z","(https://example.org/a(b)).","[http://[::1]/path],","MAILTO:name@example.org","ftp://example.org/hello","https://example.org/a*\"b"};
 const char*expected[]={"https://example.org/a?x=1&y=2#z","https://example.org/a(b)","http://[::1]/path","MAILTO:name@example.org","ftp://example.org/hello","https://example.org/a*"};
 for(int i=0;i<6;i++){struct charmap_line l=row(texts[i]);assert(con_url_at(&l,10,out,sizeof(out)));assert(!strcmp((char*)out,expected[i]));}

 /* Every decorated cell must also be clickable, including punctuation and
    every possible automatic wrap boundary. Application flags stay untouched. */
 const char *decorated="(https://example.org/a(b)). plain";
 for(ULONG split=1;split<strlen(decorated);split++){
  ULONG aa[64],bb[64];BYTE fa[64],fb[64];ULONG n=strlen(decorated);
  for(ULONG j=0;j<n;j++){if(j<split){aa[j]=decorated[j];fa[j]=j%8;}else{bb[j-split]=decorated[j];fb[j-split]=j%8;}}
  struct charmap_line a={.size=split,.text=aa,.flags=fa,.wrapColumns=split},b={.size=n-split,.text=bb,.flags=fb,.prev=&a};a.next=&b;
  for(int row=0;row<2;row++){
   struct charmap_line *r=row?&b:&a;ULONG base=row?split:0;
   for(ULONG col=0,end=0;col<r->size;col=end){
    BOOL link=con_url_run(r,col,&end);assert(end>col && end<=r->size);
    for(ULONG j=col;j<end;j++){
     BOOL expected=base+j>=1 && base+j<25;
     assert(link==expected);assert(con_url_at(r,j,out,sizeof(out))==expected);
     assert(r->flags[j]==(base+j)%8);
    }
   }
  }
 }
 struct charmap_line edit=row("https://example.org/");assert(con_url_in_row(&edit));edit.text[0]='x';assert(!con_url_in_row(&edit));

 const char*bad[]={"javascript:alert(1)","file:///SYS/System", "https://", "ordinary_text", "http:// withspace"};
 for(int i=0;i<5;i++){struct charmap_line l=row(bad[i]);assert(!con_url_at(&l,0,out,sizeof(out)));}
 struct charmap_line l=row("https://example.org/");ULONG col=l.size;l.text[l.size++]=0x1f600;assert(con_url_at(&l,col,out,sizeof(out)));assert(!strcmp((char*)out,"https://example.org/\xf0\x9f\x98\x80"));

 /* Actual cell layout for a wide URI glyph: leader+continuation. */
 l=row("https://example.org/");col=l.size;l.text[l.size++]=0x1f600;l.text[l.size++]=CELL_WIDE_CONT;l.text[l.size++]='/';l.text[l.size++]='x';
 for(ULONG k=0;k<l.size;k++){assert(con_url_at(&l,k,out,sizeof(out)));assert(!strcmp((char*)out,"https://example.org/\xf0\x9f\x98\x80/x"));}
 ULONG edge=0;assert(con_url_run(&l,0,&edge)&&edge==l.size);
 l.text[col]='x';assert(!con_url_at(&l,col+1,out,sizeof(out))); /* orphan */
 /* Wide glyph wraps before the final unused physical cell. */
 ULONG atext[32],btext[]={0x4e2d,CELL_WIDE_CONT,'/','x',')','.'};
 const char*head="(https://example.org/";ULONG length=strlen(head);
 for(ULONG k=0;k<length;k++)atext[k]=head[k];
 atext[length]=' ';
 struct charmap_line wa={.size=length+1,.text=atext,.wrapColumns=length+1,.wrapPadding=1};
 struct charmap_line wb={.size=6,.text=btext,.prev=&wa};wa.next=&wb;
 assert(!con_url_at(&wa,length,out,sizeof(out)));assert(!con_url_run(&wa,length,&edge)&&edge==wa.size);
 assert(con_url_run(&wa,1,&edge)&&edge==length);
 for(ULONG k=0;k<4;k++){assert(con_url_at(&wb,k,out,sizeof(out)));assert(!strcmp((char*)out,"https://example.org/\xe4\xb8\xad/x"));}
 assert(con_url_run(&wb,0,&edge)&&edge==4);assert(!con_url_at(&wb,4,out,sizeof(out)));

 for(int split=1;split<24;split++){
  ULONG first[64],last[64];const char*text="https://example.org/longpath";int n=strlen(text);
  for(int j=0;j<split;j++)first[j]=text[j];
  for(int j=split;j<n;j++)last[j-split]=text[j];
  struct charmap_line a={.size=split,.text=first,.wrapColumns=split},b={.size=n-split,.text=last,.prev=&a};a.next=&b;
  assert(con_url_at(&b,0,out,sizeof(out))&&!strcmp((char*)out,text));
  a.wrapColumns=0;assert(!con_url_at(&b,0,out,sizeof(out)));
 }
 l=row("https://example.org/");for(int j=l.size;j<1100;j++)chars[j]='a';l.size=1100;assert(!con_url_at(&l,20,out,sizeof(out)));
 assert(url_arguments((UBYTE*)"https://e.org/a*\"b&x=`hi`",args,sizeof(args)));
 assert(!strcmp((char*)args,"\"https://e.org/a***\"b&x=`hi`\"\n"));
 assert(!url_arguments((UBYTE*)"https://e.org/\nrun",args,sizeof(args)));
 assert(!url_arguments((UBYTE*)"https://e.org/",args,5));
 assert(!url_arguments((UBYTE*)"javascript:bad",args,sizeof(args)));
 /* Explicit labels override visible text, including spaces and punctuation. */
 struct con_link_pool pool={0};
 struct con_link *target=con_link_create(&pool,(UBYTE*)"https://target.org/",strlen("https://target.org/"),NULL,0);
 struct con_link *blocked=con_link_create(&pool,(UBYTE*)"file:///SYS/private",19,NULL,0);
 assert(target&&blocked);
 l=row("before Open report after");struct con_link *metadata[64]={0};l.links=metadata;
 assert(con_link_fill(metadata+7,11,target));
 for(ULONG k=0,e=0;k<l.size;k=e){BOOL visible=con_url_run(&l,k,&e);assert(e>k&&e<=l.size);
  for(ULONG j=k;j<e;j++){assert(visible==(j>=7&&j<18));assert(con_url_at(&l,j,out,sizeof(out))==visible);if(visible)assert(!strcmp((char*)out,"https://target.org/"));}
 }
 assert(!con_url_at(&l,8,out,strlen("https://target.org/")));assert(con_url_at(&l,8,out,strlen("https://target.org/")+1));
 assert(con_link_fill(metadata,64,NULL));
 /* Never infer the displayed https URL when an explicit non-launchable URI owns it. */
 l=row("https://visible.org/");l.links=metadata;assert(con_link_fill(metadata,l.size,blocked));
 assert(!con_url_at(&l,0,out,sizeof(out)));assert(!con_url_run(&l,0,&edge)&&edge==l.size);
 assert(con_link_fill(metadata,64,NULL));
 /* Adjacent plain URL and label must neither merge nor hide the explicit run. */
 l=row("https://a/Open report");l.links=metadata;assert(con_link_fill(metadata+10,l.size-10,target));
 assert(con_url_run(&l,0,&edge)&&edge==10);assert(con_url_at(&l,0,out,sizeof(out))&&!strcmp((char*)out,"https://a/"));
 assert(con_url_run(&l,10,&edge)&&edge==l.size);assert(con_url_at(&l,10,out,sizeof(out))&&!strcmp((char*)out,"https://target.org/"));
 assert(con_link_fill(metadata,64,NULL));
 /* Wide continuations are clickable; synthetic wrap padding never is. */
 l=row("A");l.text[0]=0x1f600;l.text[1]=CELL_WIDE_CONT;l.text[2]=' ';l.size=3;l.links=metadata;
 assert(con_link_fill(metadata,3,target));l.wrapColumns=3;l.wrapPadding=1;
 assert(con_url_at(&l,0,out,sizeof(out))&&con_url_at(&l,1,out,sizeof(out))&&!con_url_at(&l,2,out,sizeof(out)));
 assert(con_url_run(&l,0,&edge)&&edge==2);assert(!con_url_run(&l,2,&edge)&&edge==3);
 con_link_fill(metadata,64,NULL);
 /* A label crossing either a soft wrap or a hard line keeps its explicit
    destination; it does not depend on reconstructing displayed URI text. */
 for(ULONG cut=1;cut<11;cut++)for(int soft=0;soft<2;soft++){
  ULONG left[16],right[16];struct con_link *lm[16]={0},*rm[16]={0};const char *label="Open report";
  for(ULONG i=0;i<cut;i++)left[i]=label[i];
  for(ULONG i=cut;i<11;i++)right[i-cut]=label[i];
  struct charmap_line a={.text=left,.size=cut,.links=lm,.wrapColumns=soft?cut:0};
  struct charmap_line b={.text=right,.size=11-cut,.links=rm,.prev=&a};a.next=&b;
  assert(con_link_fill(lm,cut,target)&&con_link_fill(rm,11-cut,target));
  for(int row=0;row<2;row++){struct charmap_line *q=row?&b:&a;
   assert(con_url_run(q,0,&edge)&&edge==q->size);
   for(ULONG k=0;k<q->size;k++)assert(con_url_at(q,k,out,sizeof(out))&&!strcmp((char*)out,"https://target.org/"));
  }
  con_link_fill(lm,16,NULL);con_link_fill(rm,16,NULL);
 }
 con_link_release(target);con_link_release(blocked);assert(!pool.records&&!pool.bytes);
 return 0;}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';b=Path(d)/'test';p.write_text(prefix+hyperlink_source()+h+u+'\n'+w+'\n'+s+main)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
 subprocess.run([str(b)],check=True)
 p.write_text((prefix+hyperlink_source()+h+u+'\n'+w+'\n'+s+main).replace('while (*end < line->size)','while (0)'))
 subprocess.run(['cc','-Wall','-Werror',str(p),'-o',str(b)],check=True)
 assert subprocess.run([str(b)],capture_output=True).returncode
 print('Non-link segmentation negative control rejected')
print('URL action PASS: schemes, punctuation/IPv6, UTF8 wide/continuation cells, wrap-padding exclusion,23wrap boundaries, hard-line isolation, lengths and ReadArgs quoting')
