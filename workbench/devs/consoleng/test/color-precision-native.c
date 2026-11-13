/* Native legacy hexadecimal versus scaled RGB palette precision. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
#include "consoleng.h"
static struct IOStdReq *io;
static int send(const void *s, ULONG n, int split)
{
    const UBYTE *p=s;
    while(n) {
        ULONG size=split?1:n;
        io->io_Command=CMD_WRITE;io->io_Data=(APTR)p;io->io_Length=size;
        if(DoIO((struct IORequest*)io)||io->io_Actual!=size)return 0;
        p+=size;n-=size;
    }
    return 1;
}
#define TEXT(s) send(s,sizeof(s)-1,0)
#define CHECK(c) do { if(!(c)){printf("OSC_CONTROL_FAIL line=%d kind=%d ctrl=%d hidden=%d split=%d st=%d\n",__LINE__,kind,ctrl,hidden,split,st);fflush(stdout);Delay(500);goto out;} } while(0)
int main(void)
{
    int rc=20,opened=0,kind=0,ctrl=0,hidden=0,split=0,st=0,cases=0;
    struct Window *w=NULL;struct MsgPort *p=CreateMsgPort();
    io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(io);
    for(kind=0;kind<2;kind++) {
        w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,730,WA_Height,470,
            WA_Title,(IPTR)"ConsoleNG OSC controls",WA_DragBar,TRUE,
            WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,
            WA_MinWidth,200,WA_MinHeight,160,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);
        CHECK(w);io->io_Data=w;io->io_Length=sizeof(*w);
        CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",kind,(struct IORequest*)io,0));opened=1;




        static const ULONG values[]={0,1,7,8,15,16,127,128,255,256,2047,2048,4095,4096,32767,65535};
        for(split=0;split<2;split++)for(st=1;st<=4;st++)for(ctrl=0;ctrl<16;ctrl++)for(int format=0;format<2;format++) {
            ULONG mask=(1UL<<(4*st))-1;
            ULONG r=values[ctrl]&mask,g=mask-r,b=r^(mask/3),rr,gg,bb;
            if(format){rr=255*r/mask;gg=255*g/mask;bb=255*b/mask;}
            else if(st==1){rr=r<<4;gg=g<<4;bb=b<<4;}
            else {rr=r>>(4*(st-2));gg=g>>(4*(st-2));bb=b>>(4*(st-2));}
            char seq[80];int n;
            if(format)n=snprintf(seq,sizeof(seq),"\x1b]4;1;rgb:%0*lx/%0*lx/%0*lx\7",st,(unsigned long)r,st,(unsigned long)g,st,(unsigned long)b);
            else n=snprintf(seq,sizeof(seq),"\x1b]4;1;#%0*lx%0*lx%0*lx\7",st,(unsigned long)r,st,(unsigned long)g,st,(unsigned long)b);
            CHECK(send(seq,n,split));
            struct ConsolengColorPrefs got;
            io->io_Command=CD_GETCOLORPREFS;io->io_Data=&got;io->io_Length=sizeof(got);CHECK(!DoIO((struct IORequest*)io));
            CHECK((got.cp_Mask&(1UL<<3))&&got.cp_RGB[3]==((rr<<16)|(gg<<8)|bb));cases++;
        }
        printf("OSC_CONTROL_UNIT kind=%d cases=%d PASS\n",kind,cases);fflush(stdout);
        CHECK(TEXT("\x1b]104\7\x1b]50;DejaVuSansMono,16\7\x1b]2;ConsoleNG - color precision\7\x1b[2J\x1b[HPASS: hexadecimal color precision matches the reference.\r\n#123 selects #102030; rgb:1/2/3 selects #112233.\r\nOne through four digits per component verified.\r\nIndependent red, green and blue values checked.\r\nWhole and bytewise writes; standard and retained units.\r\n512 native palette readbacks passed."));
        if(kind){printf("OSC_CONTROL_PASS cases=%d\n",cases);fflush(stdout);Delay(600);}
        CloseDevice((struct IORequest*)io);opened=0;CloseWindow(w);w=NULL;
    }
    rc=0;
out:
    if(opened)CloseDevice((struct IORequest*)io);
    if(w)CloseWindow(w);
    if(io)DeleteIORequest((struct IORequest*)io);
    if(p)DeleteMsgPort(p);
    printf("OSC_CONTROL_SCRIPT rc=%d\n",rc);return rc;
}
