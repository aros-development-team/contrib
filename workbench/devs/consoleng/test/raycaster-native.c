/* ConsoleNG terminal raycaster. No game assets; DOS stream rendering/input.
 * Default: bounded automatic stress run. INTERACTIVE: WASD, Q to exit.
 */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <devices/conunit.h>
#include <dos/dosextens.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
static const char *maze[]={
"1111111111111111","1000000000000001","1022200003330001","1020000000030001",
"1020000000030001","1000000000000001","1000044440000001","1000040040000001",
"1000000040000001","1000000000000001","1003300002220001","1000300000020001",
"1000000000000001","1000000000000001","1000000000000001","1111111111111111"};
struct Library *CyberGfxBase;
static int antialias = 1;
static BPTR stream;
static char *frame;
static size_t used,capacity;
static int add(const char *fmt,...){va_list ap;va_start(ap,fmt);int n=vsnprintf(frame+used,capacity-used,fmt,ap);va_end(ap);if(n<0||(size_t)n>=capacity-used)return 0;used+=n;return 1;}
static int put(const char*s){LONG n=strlen(s);return Write(stream,s,n)==n;}
static int wall(double x,double y){int a=(int)floor(x),b=(int)floor(y);return a<0||b<0||a>=16||b>=16||maze[b][a]!='0';}
static void move(double *x,double *y,double angle,double step){double dx=cos(angle)*step,dy=sin(angle)*step;if(!wall(*x+dx*2,*y))*x+=dx;if(!wall(*x,*y+dy*2))*y+=dy;}
static ULONG pixels(struct Window*w,struct ConUnit*u){ULONG h=2166136261UL;for(int y=u->cu_YROrigin;y<=u->cu_YRExtant;y++)for(int x=u->cu_XROrigin;x<=u->cu_XRExtant;x++)h=(h^(ULONG)ReadRGBPixel(w->RPort,x,y))*16777619UL;return h;}
static int status(void){char b[4];if(!put("\x1b[5n"))return 0;for(int i=0;i<4;i++)if(!WaitForChar(stream,2000000)||Read(stream,b+i,1)!=1)return 0;return !memcmp(b,"\x1b[0n",4);}
struct Ray{double distance,texture;int type,side;};
/* Box-filter four subcell samples. No interpolation of text glyphs: this
 * changes only the application's RGB image, leaving console text crisp. */
static void shade(const struct Ray *r, double y, int rows, int *rgb)
{
    static const int colors[5][3] = {
        {90,90,90}, {180,160,125}, {190,65,45}, {45,115,190}, {170,140,40}
    };
    double height = rows / r->distance;
    double top = (rows - height) * .5, bottom = (rows + height) * .5;
    if (y >= top && y < bottom && r->type) {
        double v = (y - top) / height;
        int mortar = ((int)(v * 12) % 3 == 0) ||
            ((int)(r->texture * 8 + ((int)(v * 4) % 2) * .5) % 4 == 0);
        double light = (r->side ? .67 : 1.0) /
            (1 + r->distance * .11) * (mortar ? .56 : 1.0);
        for (int c = 0; c < 3; c++) rgb[c] = colors[r->type][c] * light;
    } else if (y < rows / 2.) {
        rgb[0] = 18 + y / 3; rgb[1] = 30 + y / 3; rgb[2] = 53 + y / 2;
    } else {
        rgb[0] = 30 + (y - rows / 2.);
        rgb[1] = 29 + (y - rows / 2.);
        rgb[2] = 27 + (y - rows / 2.);
    }
}
static int draw(struct ConUnit*u,double px,double py,double angle,int number){
 int cols=u->cu_XMax+1,rows=u->cu_YMax-2;if(cols<20||rows<10||cols>512||rows>256)return 0;
 size_t need=(size_t)cols*rows*32+2048;if(need>capacity){char *p=realloc(frame,need);if(!p)return 0;frame=p;capacity=need;}
 int samples=antialias?2:1;
 struct Ray *r=malloc(cols*samples*sizeof(*r));if(!r)return 0;
 double dx=cos(angle),dy=sin(angle);
 for(int x=0;x<cols*samples;x++){
  double camera=2.0*(x+.5)/(cols*samples)-1,rx=dx-dy*.66*camera,ry=dy+dx*.66*camera;
  int mx=(int)px,my=(int)py,sx=rx<0?-1:1,sy=ry<0?-1:1,side=0,hit=0;
  double ax=fabs(rx)<1e-12?1e12:fabs(1/rx),ay=fabs(ry)<1e-12?1e12:fabs(1/ry);
  double tx=(rx<0?px-mx:mx+1-px)*ax,ty=(ry<0?py-my:my+1-py)*ay;
  for(int n=0;n<64;n++){if(tx<ty){tx+=ax;mx+=sx;side=0;}else{ty+=ay;my+=sy;side=1;}if(mx<0||my<0||mx>=16||my>=16)break;if(maze[my][mx]!='0'){hit=maze[my][mx]-'0';break;}}
  r[x].distance=fmax(.05,side?ty-ay:tx-ax);r[x].texture=side?px+r[x].distance*rx:py+r[x].distance*ry;r[x].texture-=floor(r[x].texture);r[x].type=hit;r[x].side=side;
 }
 used=0;int ok=add("\x1b[H\x1b[0;38;2;240;230;200;48;2;18;24;34m CONSOLE-NG / RAYCASTER   frame %03d | AA %s\x1b[K",number,antialias?"2x2":"OFF");
 for(int y=0;y<rows&&ok;y++){
  ok=add("\x1b[%d;1H",y+2);int last=-1;
  for(int x=0;x<cols&&ok;x++){
   int sum[3]={0,0,0};
   for(int sx=0;sx<samples;sx++) for(int sy=0;sy<samples;sy++) {
       int color[3]; shade(&r[x*samples+sx],y+(sy+.5)/samples,rows,color);
       for(int c=0;c<3;c++) sum[c]+=color[c];
   }
   int red=sum[0]/(samples*samples),green=sum[1]/(samples*samples),blue=sum[2]/(samples*samples);
   int rgb=(red<<16)|(green<<8)|blue;if(rgb!=last){ok=add("\x1b[48;2;%d;%d;%dm",red,green,blue);last=rgb;}if(ok)ok=add(" ");
  }
 }
 if(ok)ok=add("\x1b[%d;1H\x1b[0;38;2;220;220;210;48;2;18;24;34m WASD move/turn | T toggle AA | Q exit | DOS stream\x1b[K",rows+2);
 free(r);return ok&&Write(stream,frame,used)==(LONG)used;
}
#define CHECK(c) do{if(!(c)){printf("RAYCAST_FAIL line=%d frame=%d\n",__LINE__,n);fflush(stdout);Delay(500);goto out;}}while(0)
int main(int argc,char**argv){int rc=20,n=0,alternate=0,interactive=argc>1&&!strcmp(argv[1],"INTERACTIVE");double x=5.5,y=3.5,angle=.7;struct InfoData info;struct Window*w=NULL;struct ConUnit*u=NULL;ULONG initial=0;
 CyberGfxBase=OpenLibrary((CONST_STRPTR)"cybergraphics.library",0);CHECK(CyberGfxBase);
 stream=Open((CONST_STRPTR)"RGCON:",MODE_OLDFILE);CHECK(stream);struct FileHandle*fh=BADDR(stream);CHECK(DoPkt(fh->fh_Type,ACTION_DISK_INFO,(SIPTR)MKBADDR(&info),0,0,0,0));struct IOStdReq*io=(void*)(IPTR)info.id_InUse;CHECK(io&&io->io_Unit);u=(void*)io->io_Unit;w=u->cu_Window;CHECK(w);
 MoveWindow(w,20-w->LeftEdge,30-w->TopEdge);SizeWindow(w,760-w->Width,520-w->Height);Delay(30);CHECK(SetMode(stream,TRUE));
 CHECK(put("\x1b[?25l\x1b]50;topaz,8\x07\x1b[0;38;2;230;230;230;48;2;18;24;34m\x1b[2J\x1b[HPrimary console: raycaster must restore this screen.\x1b[22t"));initial=pixels(w,u);CHECK(put("\x1b[?1049h\x1b[?7l\x1b]2;ConsoleNG raycaster - native DOS stream\x07"));alternate=1;
 printf("RAYCAST_READY interactive=%d aa=%d\n",interactive,antialias);fflush(stdout);
 for(n=0;interactive||n<80;n++){
  if(!interactive){angle+=.045;move(&x,&y,angle,.045);if(n==24){SizeWindow(w,-80,-48);Delay(20);}if(n==48){SizeWindow(w,80,48);Delay(20);}}
  while(WaitForChar(stream,1)){char key;CHECK(Read(stream,&key,1)==1);if(key=='q'||key=='Q')goto done;if(key=='w'||key=='W')move(&x,&y,angle,.15);if(key=='s'||key=='S')move(&x,&y,angle,-.15);if(key=='a'||key=='A')angle-=.12;if(key=='d'||key=='D')angle+=.12;if(key=='t'||key=='T')antialias=!antialias;printf("RAYCAST_INPUT key=%02x x=%.3f y=%.3f angle=%.3f aa=%d\n",(unsigned char)key,x,y,angle,antialias);fflush(stdout);}
  CHECK(draw(u,x,y,angle,n));if(!interactive&&n%16==0)CHECK(status());Delay(1);
 }
done:
 if(!interactive){antialias=0;CHECK(draw(u,x,y,angle,n));puts("RAYCAST_COMPARE_OFF");fflush(stdout);Delay(400);antialias=1;}
 CHECK(draw(u,x,y,angle,n));ULONG last=pixels(w,u);CHECK(draw(u,x,y,angle,n));CHECK(pixels(w,u)==last);CHECK(status());
 printf("RAYCAST_SCENE frames=%d duplicate_pixels=PASS raw_status=PASS\n",n);fflush(stdout);Delay(600);
 CHECK(put("\x1b[?7h\x1b[?1049l\x1b[23t"));alternate=0;CHECK(pixels(w,u)==initial);CHECK(put("\r\nPASS: raycaster returned to the original console.\r\n"));printf("RAYCAST_PASS frames=%d primary_restore=PASS\n",n);fflush(stdout);Delay(150);CHECK(SetMode(stream,FALSE));rc=0;
out:if(stream){if(alternate)put("\x1b[?7h\x1b[?1049l\x1b[23t");put("\x1b[?25h\x1b[0m");SetMode(stream,FALSE);Close(stream);}free(frame);if(CyberGfxBase)CloseLibrary(CyberGfxBase);printf("RAYCAST_SCRIPT rc=%d\n",rc);return rc;}
