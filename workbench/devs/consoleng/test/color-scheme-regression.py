#!/usr/bin/env python3
"""Execute production scheme parser, reporter and atomic preference publication."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'support.c').read_text(); a=s.rindex('static BOOL getstatusrequest('); parser=s[a:s.index('static BOOL getscrollregion(',a)]
s=(r/'stdconclass.c').read_text();a=s.index('static VOID stdcon_schemereply(');report=s[a:s.index('/* Replace an override',a)];a=s.index('static BOOL stdcon_colorprefs(');prefs=s[a:s.index('/* DECSCUSR',a)]
code=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
typedef unsigned char UBYTE;typedef signed char BYTE;typedef unsigned long ULONG;typedef long LONG;typedef intptr_t IPTR;typedef int BOOL;typedef void VOID;
#define TRUE 1
#define FALSE 0
#define C_DEVICE_STATUS_REPORT 1
#define C_WINDOW_STATUS_REQUEST 2
#define C_PRIMARY_ATTRIBUTES 3
#define C_SECONDARY_ATTRIBUTES 4
#define CONSOLENG_COLOR_COUNT 18
#define CONUNIT_PEN_DEFFG 0
#define CONUNIT_PEN_DEFBG 1
#define CF_COLOR_SCHEME_NOTIFY 8
struct Library{int unused;};
struct stdcondata{struct Library*scd_GfxBase;void*cm;ULONG schemeRGB,defrgb[2],ansirgb[256];LONG defpen[2],ansipen[256];unsigned short*penmap[2];};
typedef struct{struct stdcondata data;ULONG conFlags;}Object;
typedef struct{void*cl_UserData;}Class;
struct ConsolengColorPrefs{ULONG cp_Size,cp_Version,cp_Mask,cp_RGB[18];};
#define INST_DATA(cl,o) (&(o)->data)
#define CU(o) (o)
#define ICU(o) (o)
static char wire[32];static int emitted,failpen=-1,calls,releases;
static ULONG stdcon_penrgb(struct stdcondata*d,UBYTE p){(void)d;return p?0xffffff:0;}
static ULONG stdcon_slotrgb(struct stdcondata*d,ULONG i){(void)d;(void)i;return 0;}
static LONG stdcon_bestpen(struct stdcondata*d,UBYTE r,UBYTE g,UBYTE b){(void)d;(void)r;(void)g;(void)b;return calls++==failpen?-1:7;}
#define ReleasePen(cm,p) ((void)GfxBase,releases++)
#define Console_GetColorPen(o,p,out) (*(out)=0)
static void stdcon_followdefault(Object*o,UBYTE a,UBYTE b,UBYTE c,UBYTE d){(void)o;(void)a;(void)b;(void)c;(void)d;}
#define con_inject(base,o,p,n) ((void)cl,strcpy(wire,(const char*)(p)),emitted++)
'''+parser+report+prefs+r'''
int main(void){
 const char *valid[]={"?996n","?000996n"};
 for(int c1=0;c1<2;c1++)for(int v=0;v<2;v++)for(unsigned n=0;n<=strlen(valid[v]);n++){
 UBYTE*p=(void*)valid[v],count=99;BYTE cmd=99;IPTR args[2]={99,99};BOOL ok=n==strlen(valid[v]);
 assert(getstatusrequest(&cmd,&p,&count,n,args,c1)==ok);
 if(ok)assert(cmd==C_DEVICE_STATUS_REPORT&&count==2&&args[0]==996&&args[1]==c1&&!*p);
 else assert(cmd==99&&count==99&&args[0]==99&&args[1]==99&&p==(void*)valid[v]);
 }
 const char *bad[]={"?997n","?996;n","?996;1n","?996 n","?996c","996n","?999999999999999999999999999n","?n"};
 for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++){UBYTE*p=(void*)bad[i],n=99;BYTE c=99;IPTR a[2]={99,99};assert(!getstatusrequest(&c,&p,&n,strlen(bad[i]),a,0)&&c==99&&n==99&&a[0]==99&&p==(void*)bad[i]);}
 Class cl={0};Object o={0};unsigned short pen=0;o.data.cm=&o;o.data.penmap[0]=o.data.penmap[1]=&pen;
 for(int g=0;g<256;g++){o.data.schemeRGB=0x01000000UL|g*0x010101UL;emitted=0;stdcon_schemereply(&cl,&o);assert(emitted==1&&!strcmp(wire,g<128?"\x1b[?997;1n":"\x1b[?997;2n"));}
 o.data.schemeRGB=0;pen=1;stdcon_schemereply(&cl,&o);assert(!strcmp(wire,"\x1b[?997;2n"));pen=0;stdcon_schemereply(&cl,&o);assert(!strcmp(wire,"\x1b[?997;1n"));
 struct ConsolengColorPrefs p={sizeof(p),1,3,{0xffffff,0x102030}};o.conFlags=CF_COLOR_SCHEME_NOTIFY;
 emitted=0;assert(stdcon_colorprefs(&cl,&o,&p,1)&&emitted==1&&!strcmp(wire,"\x1b[?997;1n"));
 emitted=0;assert(stdcon_colorprefs(&cl,&o,&p,1)&&emitted==0);
 p.cp_RGB[1]=0xffffff;assert(stdcon_colorprefs(&cl,&o,&p,1)&&emitted==1&&!strcmp(wire,"\x1b[?997;2n"));
 p.cp_RGB[0]=0;emitted=0;assert(stdcon_colorprefs(&cl,&o,&p,1)&&emitted==1);
 o.conFlags=0;p.cp_RGB[1]=0;emitted=0;assert(stdcon_colorprefs(&cl,&o,&p,1)&&emitted==0);
 o.conFlags=CF_COLOR_SCHEME_NOTIFY;
 for(int fail=0;fail<2;fail++){Object before=o;p.cp_RGB[1]=0xffffff;failpen=fail;calls=0;releases=0;assert(!stdcon_colorprefs(&cl,&o,&p,1));assert(!memcmp(&o,&before,sizeof(o))&&emitted==0&&releases==fail);}
 failpen=-1;struct ConsolengColorPrefs got;assert(stdcon_colorprefs(&cl,&o,&got,0)&&emitted==0);
 puts("PASS: fragmented private query, 256 gray levels, fallback, atomic prefs, disable, no-op and failed allocation");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)
 for negative in (False,True):
  text=code.replace('changed && (ICU(o)->conFlags & CF_COLOR_SCHEME_NOTIFY)','changed') if negative else code
  (p/'test.c').write_text(text)
  subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
  result=subprocess.run([str(p/'test')],capture_output=True)
  assert bool(result.returncode)==negative,result.stderr.decode()
  if not negative:print(result.stdout.decode(),end='')
print('PASS: unnegotiated-notification negative control rejected')
