/* IL/DL moved paragraphs must survive native shrink/grow with default blanks. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
static struct IOStdReq*io;
static int put(const char*s){io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=strlen(s);return !DoIO((struct IORequest*)io)&&io->io_Actual==strlen(s);}
static int blankrow(struct Window*w,struct ConUnit*u){
 for(int y=0;y<u->cu_YRSize;y++)for(int x=0;x<(u->cu_XMax+1)*u->cu_XRSize;x++)
 if(ReadPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+y)!=ReadPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+u->cu_YRSize+y))return 0;
 return 1;
}
static int equal(struct Window*w,struct ConUnit*u,int row,int reference,int rows){int ink=0;LONG bg=ReadPixel(w->RPort,u->cu_XROrigin,u->cu_YROrigin);
 for(int y=0;y<rows*u->cu_YRSize;y++)for(int x=0;x<(u->cu_XMax+1)*u->cu_XRSize;x++){
 LONG a=ReadPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+row*u->cu_YRSize+y),b=ReadPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+reference*u->cu_YRSize+y);
 if(a!=b){printf("LINEREFLOW_PIXEL x=%d y=%d got=%08lx want=%08lx\n",x,y,(unsigned long)a,(unsigned long)b);return 0;}if(a!=bg)ink=1;
 }return ink;}
#define CHECK(c) do{if(!(c)){printf("LINEREFLOW_FAIL line=%d hidden=%d dir=%d delta=%d amount=%d fill=%d\n",__LINE__,hidden,dir,delta,amount,fill);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){int rc=20,opened=0,hidden=0,dir=0,delta=0,amount=0,fill=0,cases=0;struct Window*w=NULL;struct MsgPort*p=CreateMsgPort();io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(io);
 for(fill=0;fill<2;fill++)for(hidden=0;hidden<2;hidden++)for(dir=0;dir<2;dir++)for(delta=-1;delta<=1;delta+=2)for(amount=1;amount<=2;amount++){
 w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,600,WA_Height,450,WA_Title,(IPTR)"ConsoleNG line edits and paragraph reflow",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,200,WA_MinHeight,160,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);
 io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_CHARMAP,(struct IORequest*)io,0));opened=1;struct ConUnit*u=(void*)io->io_Unit;
 CHECK(put("\x1b[?25l\x1b]50;topaz,8\x07\x1b[0m\x1b]10;#e6e6e6\x07\x1b]11;#193c50\x07\x1b[2J\x1b[5;1H"));
 int oldwidth=u->cu_XMax+1,n=oldwidth*2+13;char text[500],seq[80];CHECK(n<499);for(int j=0;j<n;j++)text[j]='A'+j%26;text[n]=0;if(fill){char spaces[200];memset(spaces,' ',oldwidth);spaces[oldwidth]=0;CHECK(put("\x1b[H"));CHECK(put(spaces));CHECK(put("\x1b[5;1H"));}CHECK(put(text));
 CHECK(put(hidden?"\x1b[>9h":"\x1b[>9l"));snprintf(seq,sizeof(seq),"\x1b[2;1H\x1b[%d%c\x1b[1;7H",amount,dir?'L':'M');CHECK(put(seq));CHECK(put("\x1b[>9l"));
 SizeWindow(w,delta*120,0);Delay(50);CHECK(u->cu_XMax+1==oldwidth+delta*15);CHECK(u->cu_XCP==6&&u->cu_YCP==0);CHECK(blankrow(w,u));
 int target=4+(dir?amount:-amount);CHECK(put("\x1b[15;1H"));CHECK(put(text));CHECK(equal(w,u,target,14,5));
 CHECK(put("\x1b[>9h\x1b[>9l"));CHECK(equal(w,u,target,14,5));
 LONG blank=ReadPixel(w->RPort,u->cu_XROrigin,u->cu_YROrigin);
 CHECK(put("\x1b[41m\x1b[>9h\x1b[>9l"));
 for(int y=0;y<u->cu_YRSize;y++)for(int x=0;x<(fill?u->cu_XMax+1:6)*u->cu_XRSize;x++)CHECK(ReadPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+y)==blank);
 CHECK(put("\x1b[0m"));
 printf("LINEREFLOW_CASE hidden=%d dir=%d delta=%d amount=%d PASS\n",hidden,dir,delta,amount);fflush(stdout);cases++;
 if(cases==32){CHECK(put("\x1b[2J\x1b[HPASS: 32 native line-edit / resize combinations.\r\nMoved paragraphs reflow after shrink and grow.\r\nDefault blank rows do not multiply on shrink.\r\nHidden edits and reveal match fresh reference text."));printf("LINEREFLOW_PASS cases=%d\n",cases);fflush(stdout);Delay(600);}
 CloseDevice((struct IORequest*)io);opened=0;CloseWindow(w);w=NULL;
 }rc=0;
 out:if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(io)DeleteIORequest((struct IORequest*)io);if(p)DeleteMsgPort(p);printf("LINEREFLOW_SCRIPT rc=%d\n",rc);return rc;
}
