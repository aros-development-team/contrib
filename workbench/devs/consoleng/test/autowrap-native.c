/* Native DOS write/read: DECAWM margins, delayed wrap, retained repaint. */
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>
static int put(BPTR f,const char *s){return Write(f,(APTR)s,strlen(s))==(LONG)strlen(s);}
static int pos(BPTR f,int *row,int *col){
 char b[64];int n=0;if(!put(f,"\x1b[6n"))return 0;
 while(n<63){if(!WaitForChar(f,5000000)||Read(f,b+n,1)!=1)return 0;if(b[n++]=='R')break;}b[n]=0;
 const char *p=b;if((unsigned char)*p==0x9b)p++;else if(!strncmp(p,"\x1b[",2))p+=2;else return 0;
 return sscanf(p,"%d;%dR",row,col)==2;
}
static int expect(BPTR f,int r,int c){int row=0,col=0;int ok=pos(f,&row,&col)&&row==r&&col==c;printf("WRAP_POSITION actual=%d,%d expected=%d,%d pass=%d\n",row,col,r,c,ok);fflush(stdout);return ok;}
int main(void){
 BPTR f=Open((CONST_STRPTR)"PARITY:",MODE_NEWFILE);int rc=20,row=0,width=0;char cmd[256];
 if(!f)return 20;
 if(!SetMode(f,1)||!put(f,"\x1b%G\x1b]50;DejaVuSansMono,14\x07\x1b[2J\x1b[1;999H")||!pos(f,&row,&width)||width<20)goto done;
 printf("WRAP_WIDTH=%d\n",width);
 snprintf(cmd,sizeof(cmd),"\x1b[HConsoleNG native autowrap acceptance\r\nDisabled: right edge must read ABF\x1b[?7l\x1b[4;%dHABCDEF",width-2);
 if(!put(f,cmd)||!expect(f,4,width))goto done;
 if(!put(f,"\x1b[?7hZ")||!expect(f,5,2))goto done;
 snprintf(cmd,sizeof(cmd),"\x1b[6;1HEnabled: ABC at edge, DEF on next row\x1b[7;%dHABCDEF",width-2);
 if(!put(f,cmd)||!expect(f,8,4))goto done;
 snprintf(cmd,sizeof(cmd),"\x1b[10;1HUTF-8 disabled: Cyrillic AB equivalent at edge\x1b[?7l\x1b[11;%dH\xd0\x90\xd0\x91\xd0\x92\xd0\x93\xd0\x94\xd0\x95",width-2);
 if(!put(f,cmd)||!expect(f,11,width))goto done;
 snprintf(cmd,sizeof(cmd),"\x1b[17;1HWide disabled: preserve Q at edge\x1b[?7l\x1b[18;%dHQ\xe2\x98\x95",width);
 if(!put(f,cmd)||!expect(f,18,width))goto done;
 snprintf(cmd,sizeof(cmd),"\x1b[20;1HWide enabled: blank edge, coffee on next row\x1b[?7h\x1b[21;%dH\xe2\x98\x95",width);
 if(!put(f,cmd)||!expect(f,22,3))goto done;
 if(!put(f,"\x1b[24;1HWide overwrite: Actual / Expected pairs below"
 "\x1b[25;1HActual: A\xe2\x98\x95" "BC\x1b[25;10HX"
 "\x1b[26;1HExpect: AX BC"
 "\x1b[28;1HActual: A\xe2\x98\x95" "BC\x1b[28;11HX"
 "\x1b[29;1HExpect: A XBC"))goto done;
 if(!put(f,"\x1b[?7h\x1b[14;1HBefore retained-screen repaint"))goto done;
 puts("WRAP_DRAW_READY");fflush(stdout);Delay(500);
 if(!put(f,"\x1b[?1049h\x1b[2J\x1b[HRepainting primary screen...") )goto done;
 Delay(50);
 if(!put(f,"\x1b[?1049l\x1b[16;1HPASS: positions verified; compare retained rows above"))goto done;
 puts("WRAP_REPAINT_READY");fflush(stdout);Delay(750);rc=0;
done: SetMode(f,0);Close(f);printf("WRAP_DONE rc=%d\n",rc);fflush(stdout);return rc;
}
