/* Native cursor shape changes at one fixed position, including hidden change. */
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>
static int put(BPTR f,const char *s){return Write(f,(APTR)s,strlen(s))==(LONG)strlen(s);}
static int position(BPTR f){
 char b[64];int n=0,row=0,col=0;if(!put(f,"\x1b[6n"))return 0;
 while(n<63){if(!WaitForChar(f,5000000)||Read(f,b+n,1)!=1)return 0;if(b[n++]=='R')break;}b[n]=0;
 const char *p=b;if((unsigned char)*p==0x9b)p++;else if(!strncmp(p,"\x1b[",2))p+=2;else return 0;
 int ok=sscanf(p,"%d;%dR",&row,&col)==2&&row==6&&col==11;
 printf("CURSOR_POSITION=%d,%d pass=%d\n",row,col,ok);fflush(stdout);return ok;
}
int main(int argc,char **argv){
 BPTR f=Open((CONST_STRPTR)"PARITY:",MODE_NEWFILE);int rc=20;char s[32];
 if(!f)return 20;
 if(!SetMode(f,1)||!put(f,"\x1b%G\x1b]50;DejaVuSansMono,24\x07\x1b[2J\x1b[HNative ConsoleNG cursor shapes\r\nBlock / underline / bar\r\nSame position, no text repaint\x1b[6;1HCursor ->    \x1b[6;11H"))goto done;
 int first=argc>1&&!strcmp(argv[1],"BLINK")?1:2;
 for(int style=first;style<=6;style+=2){
  snprintf(s,sizeof(s),"\x1b[%d q",style);
  if(!put(f,s)||!position(f))goto done;
  printf("CURSOR_%d_READY\n",style);fflush(stdout);Delay(400);
 }
 if(!put(f,"Z\b\x1b[?25l\x1b[2 q\x1b[?25h")||!position(f))goto done;
 puts("CURSOR_RESTORE_READY");fflush(stdout);Delay(400);
 if(!put(f,"\x1b[?25l")||!position(f))goto done;
 puts("CURSOR_HIDDEN_READY");fflush(stdout);Delay(400);rc=0;
done:SetMode(f,0);Close(f);printf("CURSOR_DONE rc=%d\n",rc);fflush(stdout);return rc;
}
