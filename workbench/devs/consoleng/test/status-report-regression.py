#!/usr/bin/env python3
"""Actual DSR parser and early unit-state reply dispatch."""
from pathlib import Path
import tempfile,subprocess
r=Path(__file__).resolve().parents[1];s=(r/'support.c').read_text();a=s.rindex('static BOOL getstatusrequest(');parser=s[a:s.index('static BOOL getscrollregion(',a)]
s=(r/'stdconclass.c').read_text();a=s.index('    if (msg->Command == C_DEVICE_STATUS_REPORT ||');dispatch=s[a:s.index('    /* Visibility belongs to the unit',a)]
code=r'''
#define CF_ORIGIN_MODE 2048
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef signed char BYTE;typedef unsigned char UBYTE;typedef intptr_t IPTR;typedef long LONG;typedef unsigned long ULONG;typedef int BOOL;
#define TRUE 1
#define FALSE 0
#define C_DEVICE_STATUS_REPORT 1
#define C_WINDOW_STATUS_REQUEST 2
#define C_PRIMARY_ATTRIBUTES 3
#define C_SECONDARY_ATTRIBUTES 4
#define C_REPORT_CELL_GEOMETRY 5
#define C_REPORT_PIXEL_GEOMETRY 6
#define RAWFMTFUNC_STRING 0
struct Library{unsigned short lib_Version,lib_Revision;};
struct ConsoleBase{struct {struct Library dd_Library;} device;};
struct Class{void*cl_UserData;};
typedef struct{int cu_XCP,cu_YCP,cu_XMax,cu_YMax,hidden,conFlags,cu_XRSize,cu_YRSize;} Object;
struct Msg{int Command,NumParams;IPTR*Params;};
#define CU(o) (o)
#define ICU(o) (o)
#define CON_SCROLL_TOP(o) 0
#define NewRawDoFmt(fmt,mode,out,...) snprintf((char*)out,32,fmt,__VA_ARGS__)
static char captured[32];static int injections;
#define con_inject(base,o,buf,n) (strcpy(captured,(char*)buf),injections++)
'''+parser+'static void run(struct Class*cl,Object*o,struct Msg*msg){IPTR*params=msg->Params;'+dispatch+'}\n'+r'''
int main(void){
const char *da[]={"c","0c","000c"};
for(int native=0;native<2;native++)for(int q=0;q<3;q++)for(unsigned n=0;n<=strlen(da[q]);n++){
 UBYTE*p=(void*)da[q],count=99;BYTE cmd=99;IPTR v[2]={99,99};int yes=n==strlen(da[q]);
 assert(getstatusrequest(&cmd,&p,&count,n,v,native)==yes);
 assert(p==(UBYTE*)da[q]+(yes?n:0)&&cmd==(yes?C_PRIMARY_ATTRIBUTES:99)&&count==(yes?0:99)&&v[0]==99&&v[1]==99);
}
const char *bad_da[]={"1c",">1c","?0c","=0c","0;c","0 c","999999999999c"};
for(int q=0;q<7;q++){UBYTE*p=(void*)bad_da[q],n=99;BYTE c=99;IPTR v[2]={99,99};assert(!getstatusrequest(&c,&p,&n,strlen(bad_da[q]),v,0)&&p==(void*)bad_da[q]&&c==99&&n==99);}
for(int hidden=0;hidden<2;hidden++){Object o={3,4,80,24,hidden},before=o;struct Msg m={C_PRIMARY_ATTRIBUTES,0,0};injections=0;run(0,&o,&m);assert(injections==1&&!strcmp(captured,"\x1b[?6c")&&!memcmp(&o,&before,sizeof(o)));}
const char *secondary[]={">c",">0c",">000c"};
for(int q=0;q<3;q++)for(unsigned n=0;n<=strlen(secondary[q]);n++){
 UBYTE*p=(void*)secondary[q],count=99;BYTE cmd=99;IPTR v[2]={99,99};int yes=n==strlen(secondary[q]);
 assert(getstatusrequest(&cmd,&p,&count,n,v,0)==yes);
 assert(p==(UBYTE*)secondary[q]+(yes?n:0)&&cmd==(yes?C_SECONDARY_ATTRIBUTES:99)&&count==(yes?0:99));
}
for(int hidden=0;hidden<2;hidden++)for(int ver=0;ver<=65535;ver+=21845){
 Object o={3,4,80,24,hidden},before=o;struct ConsoleBase base={{{ver,65535-ver}}};struct Class cl={&base};struct Msg m={C_SECONDARY_ATTRIBUTES,0,0};char expected[64];
 snprintf(expected,sizeof(expected),"\x1b[>0;%lu;1c",(unsigned long)ver*100+65535-ver);injections=0;run(&cl,&o,&m);assert(injections==1&&!strcmp(captured,expected)&&!memcmp(&o,&before,sizeof(o)));
}
for(int hidden=0;hidden<2;hidden++)for(int cols=1;cols<=32768;cols*=2)for(int rows=1;rows<=32768;rows*=2)for(int pixels=0;pixels<2;pixels++){
 Object o={3,4,cols-1,rows-1,hidden,0,32767,32767},before=o;struct Msg m={pixels?C_REPORT_PIXEL_GEOMETRY:C_REPORT_CELL_GEOMETRY,0,0};char expected[64];
 if(pixels)snprintf(expected,sizeof(expected),"\x1b[4;%lu;%lut",(unsigned long)rows*32767,(unsigned long)cols*32767);else snprintf(expected,sizeof(expected),"\x1b[8;%d;%dt",rows,cols);
 injections=0;run(0,&o,&m);assert(injections==1&&!strcmp(captured,expected)&&!memcmp(&o,&before,sizeof(o)));
}
const char*good[]={"5n","6n","0005n","0006n"};
for(int native=0;native<2;native++)for(int g=0;g<4;g++){
 for(unsigned len=0;len<strlen(good[g]);len++){UBYTE*p=(void*)good[g],count=99;BYTE c=99;IPTR v[2]={99,99};assert(!getstatusrequest(&c,&p,&count,len,v,native));assert(p==(void*)good[g]&&c==99&&count==99&&v[0]==99&&v[1]==99);}
 UBYTE*p=(void*)good[g],count=0;BYTE c=0;IPTR v[2]={0};assert(getstatusrequest(&c,&p,&count,strlen(good[g]),v,native));assert(!*p&&count==2&&c==C_DEVICE_STATUS_REPORT&&v[0]==(g%2?6:5)&&v[1]==native);
}
const char*windows[]={" q","0 q"};
for(int i=0;i<2;i++)for(int native=0;native<2;native++){UBYTE*p=(void*)windows[i],count=99;BYTE c=99;IPTR v[2]={99,99};assert(getstatusrequest(&c,&p,&count,strlen(windows[i]),v,native)==native);if(native){assert(!*p&&c==C_WINDOW_STATUS_REQUEST&&count==0);}else{assert(p==(void*)windows[i]&&c==99&&count==99);}assert(v[0]==99&&v[1]==99);}
const char*bad[]={"n","0n","7n","5;6n","?6n",">5n","999999999999999999n","5 m"};
for(int i=0;i<8;i++){UBYTE*p=(void*)bad[i],n=99;BYTE c=99;IPTR v[2]={99,99};assert(!getstatusrequest(&c,&p,&n,strlen(bad[i]),v,0));assert(c==99&&n==99&&v[0]==99&&p==(void*)bad[i]);}
for(int hidden=0;hidden<2;hidden++)for(int native=0;native<2;native++)for(int pos=0;pos<4;pos++)for(int variant=0;variant<4;variant++){
 Object o={pos*999,pos*999,32767,32767,hidden},before=o;IPTR v[2]={variant==0?5:6,native};struct Msg m={variant==2?C_WINDOW_STATUS_REQUEST:C_DEVICE_STATUS_REPORT,variant==3?0:2,v};char expected[64];
 if(variant==0)strcpy(expected,native?"\x9b" "0n":"\x1b[0n");else if(variant==2)strcpy(expected,"\x9b" "1;1;32768;32768 r");else snprintf(expected,sizeof(expected),(native||variant==3)?"\x9b" "%d;%dR":"\x1b[%d;%dR",o.cu_YCP+1,o.cu_XCP+1);
 injections=0;run(0,&o,&m);assert(injections==1&&!strcmp(captured,expected)&&!memcmp(&o,&before,sizeof(o)));
}return 0;}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)
 for negative in (False,True):
  candidate=code.replace('BOOL native = msg->NumParams < 2 || params[1];','BOOL native = TRUE;') if negative else code
  (p/'test.c').write_text(candidate);subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
  result=subprocess.run([str(p/'test')],capture_output=True);assert bool(result.returncode)==negative,result.stderr.decode()
print('PASS: 64 status framing/state cases plus DA1 fragmented/invalid queries and hidden replies; forced-native framing negative control')
