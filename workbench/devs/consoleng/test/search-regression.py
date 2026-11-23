#!/usr/bin/env python3
"""Actual incremental search versus independent flattened substring oracle."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
h=(r/'search.h').read_text().replace('#include <exec/types.h>','')
c='\n'.join(x for x in (r/'search.c').read_text().splitlines() if not x.startswith('#include'))
line=(r/'charmap.h').read_text();line=line[line.index('struct charmap_line\n'):line.index('VOID charmap_dispose_lines')]
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
typedef uint32_t ULONG;typedef uint16_t UWORD;typedef uint8_t UBYTE;typedef int8_t BYTE;typedef uint64_t UQUAD;typedef int BOOL;typedef void VOID;
#define CHARMAP_ERASED 0x21
#define TRUE 1
#define FALSE 0
'''
main=r'''
static uint32_t rng=42;
static unsigned navigation_checks;
static unsigned random_number(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
struct token {ULONG cp;struct con_search_position first,end;};
static ULONG folded(ULONG c,int fold){if(fold&&c>=65&&c<=90)c+=32;return c;}
static void verify(struct charmap_line *rows,int count,const ULONG *query,unsigned length,int fold,unsigned budget){
 struct token text[500];unsigned n=0;
 for(int row=0;row<count;row++){
  unsigned width=rows[row].wrapColumns?rows[row].wrapColumns:rows[row].size;
  if(rows[row].wrapColumns)width-=rows[row].wrapPadding;
  if(width>rows[row].size)width=rows[row].size;
  if(!rows[row].wrapColumns)while(width&&rows[row].flags&&rows[row].text[width-1]==' '&&(rows[row].flags[width-1]==CHARMAP_ERASED)&&(!rows[row].links||!rows[row].links[width-1]))width--;

  for(unsigned col=0;col<width;col++){
   ULONG cp=rows[row].text[col];if(cp==0xfffffffe)continue;
   unsigned end=col+1;if(end<width&&rows[row].text[end]==0xfffffffe)end++;
   text[n++]=(struct token){cp?cp:' ',{rows+row,col},{rows+row,end}};
  }
  if(row+1<count&&!rows[row].wrapColumns)text[n++]=(struct token){10,{rows+row,width},{rows+row+1,0}};
 }
 struct con_search_stream stream;con_search_stream_begin(&stream,rows,0);unsigned emitted=0,work=0;
 struct con_search_token token={0};
 for(;;){struct con_search_token before=token;enum con_search_stream_result status=con_search_stream_step(&stream,&token);assert(++work<2000);
  if(status!=CON_STREAM_TOKEN){assert(!memcmp(&token,&before,sizeof(token)));if(status==CON_STREAM_DONE)break;continue;}
  assert(emitted<n&&token.cp==text[emitted].cp);
  assert(token.first.line==text[emitted].first.line&&token.first.column==text[emitted].first.column);
  assert(token.end.line==text[emitted].end.line&&token.end.column==text[emitted].end.column);emitted++;
 }
 assert(emitted==n);
 struct con_search_match expected[500];unsigned wanted=0;
 for(unsigned i=0;i+length<=n;i++){
  unsigned j=0;while(j<length&&folded(text[i+j].cp,fold)==folded(query[j],fold))j++;
  if(j==length)expected[wanted++]=(struct con_search_match){text[i].first,text[i+length-1].end};
 }
 struct con_search state;assert(con_search_begin(&state,rows,0,query,length,fold));
 struct con_search snapshot=state;struct con_search_match result={{rows,999},{rows,999}};
 assert(con_search_step(&state,0,&result)==CON_SEARCH_MORE&&!memcmp(&state,&snapshot,sizeof(state)));
 unsigned found=0,steps=0;
 for(;;){struct con_search_match before=result;enum con_search_result status=con_search_step(&state,budget,&result);assert(++steps<1000);
  if(status==CON_SEARCH_FOUND){assert(found<wanted);assert(result.first.line==expected[found].first.line&&result.first.column==expected[found].first.column&&result.end.line==expected[found].end.line&&result.end.column==expected[found].end.column);found++;}
  else {assert(!memcmp(&before,&result,sizeof(result)));if(status==CON_SEARCH_DONE)break;}
 }
 assert(found==wanted);con_search_cancel(&state);assert(!state.stream.cursor.line&&!state.length);
 /* Navigation oracle: independently index the flattened result list. */
 for(int reverse=0;reverse<2;reverse++)for(int anchor=-2;anchor<(int)wanted;anchor++){
  navigation_checks++;
  struct con_search_selection selection;
  struct con_search_match absent={{rows,9999},{rows,10000}};
  const struct con_search_match *at=anchor==-2?NULL:anchor==-1?&absent:expected+anchor;
  assert(con_search_select_begin(&selection,rows,query,length,fold,reverse,at));
  struct con_search_selection saved=selection;
  struct con_search_match result={{rows,999},{rows,999}},before=result;
  assert(con_search_select_step(&selection,0,&result)==CON_SEARCH_MORE);
  assert(!memcmp(&selection,&saved,sizeof(saved))&&!memcmp(&result,&before,sizeof(result)));
  unsigned ticks=0;enum con_search_result status;
  do {before=result;status=con_search_select_step(&selection,budget,&result);assert(++ticks<1000);
   if(status!=CON_SEARCH_FOUND)assert(!memcmp(&before,&result,sizeof(result)));
  }while(status==CON_SEARCH_MORE);
  if(!wanted)assert(status==CON_SEARCH_DONE);
  else {
   int index=anchor<0?(reverse?(int)wanted-1:0):reverse?(anchor+(int)wanted-1)%(int)wanted:(anchor+1)%(int)wanted;
   assert(status==CON_SEARCH_FOUND);
   assert(result.first.line==expected[index].first.line&&result.first.column==expected[index].first.column&&result.end.line==expected[index].end.line&&result.end.column==expected[index].end.column);
  }
  before=result;assert(con_search_select_step(&selection,999,&result)==CON_SEARCH_DONE&&!memcmp(&result,&before,sizeof(result)));
  con_search_select_cancel(&selection);assert(!selection.scan.length&&!selection.scan.stream.cursor.line&&!selection.anchor.first.line);
 }

}
int main(void){
 for(unsigned iteration=0;iteration<3000;iteration++){
  struct charmap_line rows[8]={0};ULONG cells[8][20]={0};BYTE flags[8][20]={0};unsigned count=1+random_number()%8;
  for(unsigned row=0;row<count;row++){
   rows[row].text=cells[row];rows[row].flags=flags[row];rows[row].size=random_number()%20;rows[row].next=row+1<count?rows+row+1:NULL;
   for(unsigned col=0;col<rows[row].size;col++){
    const ULONG alphabet[]={'a','b','A',0x3a9,0x4e2d,0};ULONG cp=alphabet[random_number()%6];cells[row][col]=cp;
    if(cp==0x4e2d&&col+1<rows[row].size)cells[row][++col]=0xfffffffe;
   }
   if(rows[row].size&&random_number()%2){unsigned last=rows[row].size-1;if(cells[row][last]==0xfffffffe&&last)cells[row][last-1]='b';cells[row][last]=' ';flags[row][last]=CHARMAP_ERASED;}
   if(random_number()%2){rows[row].wrapColumns=rows[row].size?rows[row].size:1;
    if(rows[row].size&&random_number()%2){unsigned last=rows[row].size-1;if(cells[row][last]==0xfffffffe&&last)cells[row][last-1]='b';cells[row][last]=' ';rows[row].wrapPadding=1;}}
  }
  ULONG query[17];unsigned length=1+random_number()%17;
  for(unsigned j=0;j<length;j++){const ULONG alphabet[]={'a','b','A',0x3a9,0x4e2d,' ',10};query[j]=alphabet[random_number()%7];}
  if(iteration%2==0){length=1;query[0]='a';}
  const unsigned budgets[]={1,2,7,4096};
  for(unsigned b=0;b<4;b++)for(int fold=0;fold<2;fold++)verify(rows,count,query,length,fold,budgets[b]);
 }
 /* Overlapping maximum-length matches across soft row boundaries. */
 struct charmap_line rows[10]={0};ULONG cells[10][32],query[256];
 for(unsigned i=0;i<256;i++)query[i]='a';
 for(unsigned row=0;row<10;row++){for(unsigned col=0;col<32;col++)cells[row][col]='a';rows[row].text=cells[row];rows[row].size=rows[row].wrapColumns=32;rows[row].next=row<9?rows+row+1:NULL;}
 verify(rows,10,query,256,0,1);verify(rows,10,query,3,0,4096);
 /* Exact wide-cell endpoint, hard boundary and folding are explicit contracts. */
 ULONG wide[]={0x4e2d,0xfffffffe,'A'},tail[]={'b'};struct charmap_line one={0},two={0};one.text=wide;one.size=3;one.next=&two;two.text=tail;two.size=1;
 struct con_search state;struct con_search_match match;ULONG needle[]={0x4e2d};
 assert(con_search_begin(&state,&one,0,needle,1,0)&&con_search_step(&state,10,&match)==CON_SEARCH_FOUND&&match.first.column==0&&match.end.column==2);
 ULONG across[]={'a',10,'b'};assert(con_search_begin(&state,&one,0,across,3,1)&&con_search_step(&state,20,&match)==CON_SEARCH_FOUND&&match.first.column==2&&match.end.line==&two&&match.end.column==1);
 assert(con_search_begin(&state,&one,1,needle,1,0)&&con_search_step(&state,20,&match)==CON_SEARCH_DONE);
 ULONG invalid[]={0,0xd800,0x110000,0xfffffffe};for(unsigned i=0;i<4;i++)assert(!con_search_begin(&state,&one,0,invalid+i,1,0)&&!state.length&&!state.stream.cursor.line);
 assert(!con_search_begin(&state,&one,0,query,257,0));assert(!con_search_begin(&state,&one,0,NULL,1,0));assert(!con_search_begin(&state,&one,0,query,0,0));
 /* Background padding trims in bounded steps; printed trailing blanks remain. */
 ULONG padded[4096];BYTE erased[4096];for(unsigned i=0;i<4096;i++){padded[i]=i?' ':'a';erased[i]=i?CHARMAP_ERASED:0;}
 struct charmap_line pad={0};pad.text=padded;pad.flags=erased;pad.size=4096;ULONG aletter='a';
 assert(con_search_begin(&state,&pad,0,&aletter,1,0));assert(con_search_step(&state,7,&match)==CON_SEARCH_MORE&&state.stream.limit==4089&&state.stream.cursor.column==0);
 assert(con_search_step(&state,4096,&match)==CON_SEARCH_FOUND&&match.first.column==0&&match.end.column==1);
 ULONG rowtext[]={'a',' ',' '};BYTE rowflags[]={0,0,CHARMAP_ERASED};struct charmap_line trimmed={0};trimmed.text=rowtext;trimmed.flags=rowflags;trimmed.size=3;trimmed.next=&two;
 ULONG boundary[]={'a',' ',10,'b'};
 assert(con_search_begin(&state,&trimmed,0,boundary,4,0));assert(con_search_step(&state,30,&match)==CON_SEARCH_FOUND&&match.first.column==0&&match.end.line==&two);
 rowflags[1]=32; /* Double-underlined printed blank is not erased padding. */
 assert(con_search_begin(&state,&trimmed,0,boundary,4,0));assert(con_search_step(&state,30,&match)==CON_SEARCH_FOUND);
 /* Empty rows still consume budget; independent states do not interfere. */
 struct charmap_line empty[64]={0};for(unsigned i=0;i<64;i++){empty[i].wrapColumns=1;empty[i].next=i<63?empty+i+1:NULL;}
 ULONG letter='a';assert(con_search_begin(&state,empty,0,&letter,1,0));
 for(unsigned i=0;i<64;i++){enum con_search_result status=con_search_step(&state,1,&match);assert(state.stream.cursor.line==(i<63?empty+i+1:NULL));assert(status==(i<63?CON_SEARCH_MORE:CON_SEARCH_DONE));}
 struct con_search other;ULONG capital='A';assert(con_search_begin(&state,&one,0,&capital,1,0));assert(con_search_begin(&other,&one,0,needle,1,0));capital='Z';
 assert(con_search_step(&other,1,&match)==CON_SEARCH_FOUND&&match.first.column==0);
 assert(con_search_step(&state,8,&match)==CON_SEARCH_FOUND&&match.first.column==2);
 /* Every cell matches: finding matches must not renew the work budget. */
 struct con_search_selection selection;
 struct con_search_match absent={{&one,9999},{&one,10000}};
 ULONG dense[128];for(unsigned i=0;i<128;i++)dense[i]='a';
 struct charmap_line dense_row={0};dense_row.text=dense;dense_row.size=128;
 for(int reverse=0;reverse<2;reverse++){
  assert(con_search_select_begin(&selection,&dense_row,&letter,1,0,reverse,&absent));
  for(unsigned step=1;step<=16;step++){
   assert(con_search_select_step(&selection,7,&match)==CON_SEARCH_MORE);
   assert(selection.scan.stream.cursor.line==&dense_row&&selection.scan.stream.cursor.column==step*7);
  }
  con_search_select_cancel(&selection);assert(con_search_select_step(&selection,100,&match)==CON_SEARCH_DONE);
 }
 assert(!con_search_select_begin(&selection,&dense_row,NULL,1,0,0,NULL));
 assert(con_search_select_step(&selection,1,&match)==CON_SEARCH_DONE);
 printf("Navigation PASS:%u independent indexed-oracle selections plus dense-match budget checks\n",navigation_checks);
 ULONG loweromega=0x3c9;assert(con_search_begin(&state,&one,0,&loweromega,1,1));assert(con_search_step(&state,20,&match)==CON_SEARCH_DONE);
 puts("Search PASS:24000 randomized oracle scans, bounded steps, overlap, Unicode/wide cells, soft/hard boundaries, maximum query, cancellation and bidirectional wrapped selection");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';b=Path(d)/'t';p.write_text(prefix+line+h+c+main)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True);subprocess.run([str(b)],check=True)
 broken=c.replace('if (cp == SEARCH_WIDE_CONT) return CON_STREAM_MORE;','/* incorrectly include continuation */')
 assert broken != c, 'Negative control did not modify production source'
 p.write_text(prefix+line+h+broken+main)
 subprocess.run(['cc','-Wall','-Werror',str(p),'-o',str(b)],check=True)
 assert subprocess.run([str(b)],capture_output=True).returncode!=0
 print('Negative control PASS: wide-continuation inclusion rejected')

 with tempfile.TemporaryDirectory() as d2:
  p=Path(d2)/'t.c';b=Path(d2)/'t'
  broken=c.replace('if (!line->next || line->wrapColumns) return stream->cursor.line ? CON_STREAM_MORE : CON_STREAM_DONE;', 'if (!line->next) return CON_STREAM_DONE;')
  assert broken != c, 'Negative control did not modify production source'
  p.write_text(prefix+line+h+broken+main)
  subprocess.run(['cc','-Wall','-Werror',str(p),'-o',str(b)],check=True)
  assert subprocess.run([str(b)],capture_output=True).returncode!=0
  print('Negative control PASS: treating soft wraps as hard breaks rejected')

with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';b=Path(d)/'t'
 broken=c.replace('selection->reverse ? selection->last : selection->first', 'selection->reverse ? selection->first : selection->last')
 assert broken != c, 'Negative control did not modify production source'
 p.write_text(prefix+line+h+broken+main)
 subprocess.run(['cc','-Wall','-Werror',str(p),'-o',str(b)],check=True)
 assert subprocess.run([str(b)],capture_output=True).returncode!=0
 print('Negative control PASS: wrong wraparound direction rejected')

with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';b=Path(d)/'t'
 broken=c.replace('stream->trimming = !line->wrapColumns && line->flags;', 'stream->trimming = FALSE;')
 assert broken != c, 'Negative control did not modify production source'
 p.write_text(prefix+line+h+broken+main)
 subprocess.run(['cc','-Wall','-Werror',str(p),'-o',str(b)],check=True)
 assert subprocess.run([str(b)],capture_output=True).returncode!=0
 print('Negative control PASS: retained erased padding rejected')
