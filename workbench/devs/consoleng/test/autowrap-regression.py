#!/usr/bin/env python3
"""Execute actual row storage/dispatch and wrap advancement, including repaint data."""
from pathlib import Path
from hyperlink_fixture import hyperlink_source
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'charmapconclass.c').read_text();r=(root/'stdconclass.c').read_text()
def function(s,name):
 a=s.index(name);a=s.rfind('\n',0,a)+1;b=s.index('{',a);depth=1;c=b+1
 while depth:
  depth+=(s[c]=='{')-(s[c]=='}');c+=1
 return s[a:c]
helpers='\n'.join(function(r,n) for n in ['stdcon_prepare_print(', 'stdcon_advance_print('])
helpers+='\n'+function(s,'charmap_repair_wide_cells(')+'\n'+function(s,'charmap_insert_char(')+'\n'+function(s,'charmap_ascii(')+'\n'+function(s,'charmap_print_span(')
a=s.index('    case C_ASCII_STRING:',s.index('static VOID charmapcon_docommand('));b=s.index('    case C_SET_OBSCURED:',a)
body=s[a:b]
a=r.index('        case C_ASCII_STRING:',r.index('if (CON_IS_OBSCURED(o))'));b=r.index('        case C_RESET_FONT:',a)
hidden='static void hidden_write(Object *o,const char *text,int len){IPTR params[2]={(IPTR)text,len};switch(C_ASCII_STRING){'+r[a:b]+'}}\n'
prefix=r'''
#include <assert.h>
#define CHARMAP_ROLES(o) 0x0102
#include <string.h>
#include <stdint.h>
typedef unsigned long ULONG;typedef long LONG;typedef unsigned char UBYTE;typedef short WORD;typedef intptr_t IPTR;typedef int BOOL;typedef void VOID;typedef void Class;typedef void *Msg;
#define CON_TXTFLAGS_BOLD 2
#define CON_TXTFLAGS_ITALIC 4
#define TRUE 1
#define FALSE 0
#define MIN(a,b) ((a)<(b)?(a):(b))
#define CHARMAP_ERASED 0x21
#define CELL_WIDE_CONT 0xfffffffeUL
#define CF_WRAP_PENDING 128
#define CF_INSERT_MODE 1024
#define PMB_AWM 1
#define C_ASCII_STRING 1
#define C_LINEFEED 2
#define C_NEXT_LINE 3
#define C_INDEX 4
struct charmapcondata {struct con_link *current_link;};
static struct charmapcondata link_data;
#define INST_DATA(cl,o) (&link_data)
#define charmap_underline_colors(l,x,n,c) ((void)(l),(void)(x),(void)(n),!(c))
#define charmap_links(l,x,n,link) ((void)(l),(void)(x),(void)(n),!(link))
struct charmap_line {ULONG size,capacity,text[64];UBYTE fgpen[64],bgpen[64],flags[64];unsigned long long *roles; unsigned long wrapColumns; unsigned char wrapPadding;struct con_link **links;ULONG *underlineColors;};
typedef struct {ULONG x,xc,y,max,underlineColor;int mode,conFlags,utf8,hidden,refreshes;UBYTE cu_FgPen,cu_BgPen,cu_TxFlags;struct charmap_line rows[64];} Object;
struct P_Console_DoCommand {int MethodID,Command,NumParams;IPTR *Params;};
#define ICU(o) (o)
#define CU(o) (o)
#define XCP (o->x)
#define XCCP (o->xc)
#define YCP (o->y)
#define CHAR_XMAX(o) ((o)->max)
#define CHAR_XMIN(o) 0
#define CHECK_MODE(o,m) ((o)->mode)
#define CON_IS_UTF8(o) ((o)->utf8)
#define CON_TEXT_IS_UTF8(o) CON_IS_UTF8(o)
#define CON_IS_OBSCURED(o) ((o)->hidden)
#define charmapcon_refresh_lines(cl,o,a,b) ((o)->refreshes++)
#define Console_Down(o,n) ((o)->y+=(n))
#define charmapcon_find_line(cl,o,y) (&(o)->rows[y])
#define charmap_resize(base,line,n) ((line)->size=(n))
#define SetMem(p,c,n) memset(p,c,n)
#define charmapcon_clear_selection(cl,o) ((void)0)
/* These row-storage fixtures contain no URLs; real URL/wrap detection is
   exercised in url-action-regression.py. */
#define con_url_in_row(line) FALSE
/* Fixture UTF-8 decoder and coffee width; production decoding has separate coverage. */
static ULONG con_utf8_decode(const UBYTE **p,LONG *n){ULONG c=*(*p)++;--*n;int extra=c>=0xe0?2:c>=0xc0?1:0;if(extra)c&=extra==2?15:31;while(extra--&&*n){c=(c<<6)|(*(*p)++&63);--*n;}return c;}
static WORD con_cp_width(ULONG c){return c==0x2615?2:1;}
static ULONG con_utf8_cellcount(const UBYTE *p,LONG n){ULONG c=0;while(n){c+=con_cp_width(con_utf8_decode(&p,&n));}return c;}
'''
superfn=r'''
static void DoSuperMethodA(Class *cl,Object *o,Msg m){
 struct P_Console_DoCommand *part=m;
 stdcon_prepare_print(o);
 stdcon_advance_print(o,o->utf8?con_utf8_cellcount((const UBYTE*)part->Params[0],part->Params[1]):part->Params[1],FALSE);
}
static void write_text(Object *o,const char *text,int len){
 Class *cl=0;IPTR args[2]={(IPTR)text,len};struct P_Console_DoCommand command={0,C_ASCII_STRING,2,args},*msg=&command;IPTR *params=args;
 switch(msg->Command){
'''+body+r'''
 }
}
int main(void){
 /* Cursor-skipped cells are blanks, not printed characters with the new SGR. */
 for(int utf=0;utf<2;utf++)for(int flags=0;flags<256;flags++){
  Object o={0};o.max=31;o.x=o.xc=7;o.utf8=utf;
  o.cu_FgPen=4;o.cu_BgPen=5;o.cu_TxFlags=flags;
  unsigned long long roles[64]={0};o.rows[0].roles=roles;
  write_text(&o,"X",1);
  for(int i=0;i<7;i++){assert(o.rows[0].text[i]==' ');assert(o.rows[0].flags[i]==0);assert(o.rows[0].bgpen[i]==5);assert(roles[i]==0x0102);}
  assert(o.rows[0].flags[7]==flags);
 }

 /* Styled output must request recomposition, including an overwritten old
    styled cell; ordinary and hidden output must keep their existing paths. */
 for(int old=0;old<8;old++)for(int fresh=0;fresh<8;fresh++)for(int hidden=0;hidden<2;hidden++){
  Object o={0};o.max=31;o.hidden=hidden;o.cu_TxFlags=fresh;
  o.rows[0].size=1;o.rows[0].text[0]='W';o.rows[0].flags[0]=old;
  write_text(&o,"y",1);
  assert(o.refreshes==(!hidden&&((old|fresh)&6)!=0));
 }

 /* Actual dispatch: inserted text shifts the full retained row once. */
 for(int width=1;width<=32;width++)for(int start=0;start<width;start++)for(int hidden=0;hidden<2;hidden++){
  Object o={0};o.max=width-1;o.x=o.xc=start;o.mode=1;o.hidden=hidden;o.conFlags=CF_INSERT_MODE;
  for(int row=0;row<64;row++){o.rows[row].size=width;for(int col=0;col<width;col++)o.rows[row].text[col]='A'+col%26;}
  write_text(&o,"xy",2);
  for(int col=0;col<width;col++){
   ULONG want=col<start?(ULONG)('A'+col%26):col==start?'x':col==start+1?'y':(ULONG)('A'+(col-2)%26);
   assert(o.rows[0].text[col]==want);
  }
  if(start==width-1){assert(o.rows[1].text[0]=='y');if(width>1)assert(o.rows[1].text[1]=='A');}
 }
 int cases=0;
 for(int width=1;width<=32;width++)for(int start=0;start<width;start++)for(int mode=0;mode<=1;mode++)for(int utf=0;utf<=1;utf++){
  Object o={0};o.max=width-1;o.x=o.xc=start;o.mode=mode;o.utf8=utf;
  char text[96];int bytes=0;ULONG expected[64][64]={{0}};int x=start,y=0,pending=0;
  for(int i=0;i<48;i++){
   int c=utf?0x410+i:'A'+i%26;
   if(utf){text[bytes++]=0xc0|(c>>6);text[bytes++]=0x80|(c&63);}else text[bytes++]=c;
   if(pending&&mode){x=0;y++;pending=0;}
   expected[y][x]=c;
   if(x==width-1)pending=1;else x++;
  }
  write_text(&o,text,bytes);
  assert(o.x==x&&o.xc==x&&o.y==y);
  for(int row=0;row<=y;row++)for(int col=0;col<width;col++)
   if(expected[row][col])assert(o.rows[row].text[col]==expected[row][col]);
  for(int row=0;row<=y;row++){assert(o.rows[row].size<=width);assert(o.rows[row].wrapColumns==(row<y?(unsigned)width:0));}
  /* Separate writes must preserve delayed-wrap and mode-toggle state. */
  if(!mode){o.mode=1;write_text(&o,"Z",1);assert(o.y==1&&o.rows[1].text[0]=='Z');}
  cases++;
 }
 assert(cases==2112);
 /* Overwrite either boundary of an existing wide pair, across all spans. */
 for(int start=0;start<9;start++)for(int len=1;len<=9-start;len++)for(int hidden=0;hidden<=1;hidden++){
  Object o={0};o.max=8;o.x=o.xc=start;o.utf8=1;o.hidden=hidden;
  o.cu_FgPen=4;o.cu_BgPen=5;o.cu_TxFlags=8;
  ULONG expected[9]={'A',0x2615,CELL_WIDE_CONT,'B',0x2615,CELL_WIDE_CONT,'C','D','E'};
  struct charmap_line *line=&o.rows[0];line->size=9;
  memcpy(line->text,expected,sizeof(expected));
  memset(line->fgpen,1,9);memset(line->bgpen,2,9);memset(line->flags,1,9);
  int repaired=(start==2||start==5||start+len==2||start+len==5);
  for(int i=start;i<start+len;i++){
   if(expected[i]==CELL_WIDE_CONT&&i)expected[i-1]=' ';
   if(expected[i]==0x2615&&i<8)expected[i+1]=' ';
   expected[i]='X';
  }
  char text[9];memset(text,'X',len);write_text(&o,text,len);
  assert(o.refreshes==(repaired&&!hidden));
  for(int i=0;i<9;i++){
   assert(line->text[i]==expected[i]);
   int inside=i>=start&&i<start+len;
   assert(line->fgpen[i]==(inside?4:1));assert(line->bgpen[i]==(inside?5:2));assert(line->flags[i]==(inside?8:1));
  }
 }

 for(int width=1;width<=32;width++)for(int start=0;start<width;start++)for(int mode=0;mode<=1;mode++){
  Object o={0};o.max=width-1;o.x=o.xc=start;o.mode=mode;o.utf8=1;
  ULONG expected[64][64];memset(expected,0,sizeof(expected));
  for(int r=0;r<64;r++)for(int c=0;c<width;c++){o.rows[r].text[c]=expected[r][c]='#';o.rows[r].size=width;}
  int x=start,y=0,pending=0;const int input[]={0x2615,'X',0x2615,'Y'};
  for(int i=0;i<4;i++){
   if(pending&&mode){x=0;y++;pending=0;}
   int w=input[i]==0x2615?2:1;
   if(w>width-x){
    if(!mode||width<2){pending=1;continue;}
    expected[y][x]=' ';y++;x=0;
   }
   if(expected[y][x]==CELL_WIDE_CONT&&x)expected[y][x-1]=' ';
   if(x+w<width&&expected[y][x+w]==CELL_WIDE_CONT)expected[y][x+w]=' ';
   expected[y][x]=input[i];
   if(w==2)expected[y][++x]=CELL_WIDE_CONT;
   if(x==width-1)pending=1;else x++;
  }
  Object obscured=o;
  hidden_write(&obscured,"\xe2\x98\x95X\xe2\x98\x95Y",8);
  write_text(&o,"\xe2\x98\x95X\xe2\x98\x95Y",8);
  assert(obscured.x==o.x&&obscured.y==o.y&&obscured.conFlags==o.conFlags);
  assert(o.x==x&&o.y==y);
  for(int row=0;row<y;row++){assert(o.rows[row].wrapColumns==(unsigned)width);assert(o.rows[row].wrapPadding==(expected[row][width-1]==' '));}
  for(int r=0;r<=y;r++)for(int c=0;c<width;c++)assert(o.rows[r].text[c]==expected[r][c]);
 }

 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';b=Path(d)/'test'
 for negative in [0,1,2,3,4]:
  h=helpers.replace('&& CHECK_MODE(o, PMB_AWM)', '') if negative==1 else helpers
  if negative==3:h=h.replace("line->text[xcp - 1] = ' ';", "line->text[xcp - 1] = 0x2615;")
  main=superfn.replace('if (width > CHAR_XMAX(o) + 1 - XCP)', 'if (width < 0)') if negative==2 else superfn
  if negative==4:main=main.replace("ICU(o)->conFlags & CF_INSERT_MODE", "ICU(o)->conFlags & 0")
  p.write_text(prefix+hyperlink_source()+h+hidden+main)
  subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
  result=subprocess.run([str(b)],capture_output=True)
  assert (result.returncode!=0)==bool(negative),result.stderr.decode()
print('Autowrap PASS: 2112 row-storage/width/mode/UTF8 cases, delayed wrap, mode enable; four negative controls fail; 1056 insert-mode row/hidden cases; 1056 wide-margin/hidden-state and 90 overwrite/rendition/repaint cases')
