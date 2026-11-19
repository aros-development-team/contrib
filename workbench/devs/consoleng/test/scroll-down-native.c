/* Native CSI T full-grid comparison, history isolation, redraw and hidden tabs. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
static struct IOStdReq*io;static struct Window*w;static struct ConUnit*u;
static int put(const char*s){io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=strlen(s);return !DoIO((struct IORequest*)io);}
static ULONG pixels(void){ULONG h=2166136261UL;for(int y=u->cu_YROrigin;y<=u->cu_YRExtant;y++)for(int x=u->cu_XROrigin;x<=u->cu_XRExtant;x++)h=(h^(ULONG)ReadPixel(w->RPort,x,y))*16777619UL;return h;}
static int paint(int first,int count,int insert){char s[100];if(!put("\x1b[2J"))return 0;for(int y=0;y<=u->cu_YMax;y++){int src=y;
 if(y>=first&&count){src=insert?y-count:y+count;if((insert&&src<first)||(!insert&&src>u->cu_YMax))continue;}
 snprintf(s,sizeof(s),"\x1b[%d;1HRow %03d : ABCDEFGHIJKLMNOPQRSTUVWXYZ",y+1,src);if(!put(s))return 0;
 }return 1;}
#define CHECK(c) do{if(!(c)){printf("SCROLLDOWN_FAIL line=%d kind=%d hidden=%d dir=%d pos=%d amount=%d\n",__LINE__,kind,hidden,dir,pos,amount);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){int rc=20,opened=0,kind=0,hidden=0,dir=0,pos=0,amount=0,cases=0;struct MsgPort*port=CreateMsgPort();io=port?(void*)CreateIORequest(port,sizeof(*io)):NULL;CHECK(io);
 int counts[]={0,1,2,65535};
 for(kind=0;kind<2;kind++){
 w=OpenWindowTags(NULL,WA_Left,30,WA_Top,35,WA_Width,730,WA_Height,450,WA_Title,(IPTR)"ConsoleNG scroll down and history isolation",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,160,WA_MinHeight,80,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);
 io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",kind?CONU_CHARMAP:CONU_STANDARD,(struct IORequest*)io,0));opened=1;u=(void*)io->io_Unit;
 CHECK(put("\x1b[?25l\x1b]50;topaz,8\x07\x1b[38;2;230;230;230m\x1b[48;2;25;60;80m"));
 for(hidden=0;hidden<=(kind?1:0);hidden++)for(dir=0;dir<2;dir++)for(pos=0;pos<3;pos++)for(amount=0;amount<4;amount++){
 int first=pos==0?0:pos==1?2:u->cu_YMax;int n=counts[amount]?counts[amount]:1;if(n>u->cu_YMax-first+1)n=u->cu_YMax-first+1;
 CHECK(put("\x1b[3J"));
 if(dir)for(int old=0;old<80;old++)CHECK(put("OLD HISTORY MUST NOT BE INSERTED\r\n"));
 n=counts[amount]?counts[amount]:1;if(n>u->cu_YMax+1)n=u->cu_YMax+1;
 CHECK(paint(0,0,0));char seq[80];snprintf(seq,sizeof(seq),"\x1b[%d;4H",first+1);CHECK(put(seq));ULONG before=pixels();
 if(hidden)CHECK(put("\x1b[>9h"));snprintf(seq,sizeof(seq),"\x1b[%dT",counts[amount]);CHECK(put(seq));CHECK(u->cu_XCP==3&&u->cu_YCP==first);
 if(hidden){CHECK(pixels()==before);CHECK(put("\x1b[>9l"));}
 ULONG actual=pixels();
 if(kind){CHECK(put("\x1b[>9h\x1b[>9l"));CHECK(pixels()==actual);}
 CHECK(paint(0,n,1));ULONG expected=pixels();if(actual!=expected)printf("SCROLLDOWN_PIXELS got=%08lx expected=%08lx\n",(unsigned long)actual,(unsigned long)expected);CHECK(actual==expected);cases++;
 }
 printf("SCROLLDOWN_GROUP kind=%d PASS\n",kind);fflush(stdout);
 if(kind){CHECK(put("\x1b[2J\x1b[HPASS: 72 scroll-down reference comparisons.\r\nStandard/retained consoles; empty/populated history.\r\nHidden tabs and redraw; cursor stays fixed.\r\nCounts 0/1/2/65535; history never replaces blank rows."));printf("SCROLLDOWN_PASS cases=%d\n",cases);fflush(stdout);Delay(600);}
 CloseDevice((struct IORequest*)io);opened=0;CloseWindow(w);w=NULL;
 }rc=0;
 out:if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(io)DeleteIORequest((struct IORequest*)io);if(port)DeleteMsgPort(port);printf("SCROLLDOWN_SCRIPT rc=%d\n",rc);return rc;
}
