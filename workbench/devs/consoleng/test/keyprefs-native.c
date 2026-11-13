/* Atomic interaction profiles, compatibility and saved shortcut verification. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
#include "consoleng.h"
static struct IOStdReq *io;
static int request(UWORD cmd,void*p,ULONG n){io->io_Command=cmd;io->io_Data=p;io->io_Length=n;return !DoIO((struct IORequest*)io)&&io->io_Actual==n;}
#define CHECK(c) do{if(!(c)){printf("KEYPREF_FAIL line=%d\n",__LINE__);goto out;}}while(0)
int main(int argc,char**argv){int rc=20,opened=0,saved=0;struct MsgPort*port=CreateMsgPort();
 struct ConsolengInteractionProfile old,p,got;
 io=port?(void*)CreateIORequest(port,sizeof(*io)):NULL;CHECK(io);
 CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_LIBRARY,(struct IORequest*)io,0));opened=1;
 CHECK(request(CD_GETINTERACTIONPROFILE,&old,sizeof(old)));
 if(argc>1){CHECK(old.ip_Bindings.bp_Count==4);ULONG expected=argv[1][0]=='F'?CONSOLENG_BIND_RAW+0x50+argv[1][1]-'1':(ULONG)argv[1][0];CHECK(old.ip_Bindings.bp_Entries[0].kb_Key==expected);CHECK(old.ip_Bindings.bp_Entries[0].kb_Modifiers==(argv[1][0]=='F'?0:5));printf("KEYPREF_EXPECT_%s_PASS\n",argv[1]);rc=0;goto out;}
 saved=1;p=old;p.ip_Bindings.bp_Entries[0]=(struct ConsolengKeyBinding){'x',5,1};p.ip_Appearance.rp_CursorColor.ccp_Enabled=1;
 CHECK(request(CD_SETINTERACTIONPROFILE,&p,sizeof(p)));CHECK(request(CD_GETINTERACTIONPROFILE,&got,sizeof(got)));CHECK(!memcmp(&got,&p,sizeof(p)));
 for(unsigned field=0;field<7;field++){
  struct ConsolengInteractionProfile bad=p;bad.ip_Appearance.rp_Theme.tp_Cursor.cp_Style=6;
  switch(field){case 0:bad.ip_Size--;break;case 1:bad.ip_Version++;break;case 2:bad.ip_Bindings.bp_Count=17;break;
   case 3:bad.ip_Bindings.bp_Entries[1]=bad.ip_Bindings.bp_Entries[0];break;case 4:bad.ip_Appearance.rp_Size--;break;
   case 5:bad.ip_Appearance.rp_CursorColor.ccp_Enabled=2;break;case 6:bad.ip_Appearance.rp_Theme.tp_Font.fp_Pixels=65536;break;}
  CHECK(!request(CD_SETINTERACTIONPROFILE,&bad,sizeof(bad))&&io->io_Actual==0);
  CHECK(request(CD_GETINTERACTIONPROFILE,&got,sizeof(got))&&!memcmp(&p,&got,sizeof(p)));
 }
 CHECK(!request(CD_SETINTERACTIONPROFILE,&p,sizeof(p)-1)&&io->io_Actual==0);
 CHECK(request(CD_SETAPPEARANCEPROFILE,&old.ip_Appearance,sizeof(old.ip_Appearance)));
 CHECK(request(CD_GETINTERACTIONPROFILE,&got,sizeof(got))&&!memcmp(&got.ip_Bindings,&p.ip_Bindings,sizeof(p.ip_Bindings)));
 struct {struct ConsolengInteractionProfile p;ULONG guard;} bounded={{0},0x12345678};
 CHECK(request(CD_GETINTERACTIONPROFILE,&bounded,sizeof(bounded.p))&&bounded.guard==0x12345678);
 puts("KEYPREF_ATOMIC_PASS invalid snapshots unchanged; old ABI preserves bindings; bounded copies");rc=0;
out:
 if(saved)request(CD_SETINTERACTIONPROFILE,&old,sizeof(old));
 if(opened)CloseDevice((struct IORequest*)io);
 if(io)DeleteIORequest((struct IORequest*)io);
 if(port)DeleteMsgPort(port);
 printf("KEYPREF_SCRIPT rc=%d\n",rc);return rc;
}
