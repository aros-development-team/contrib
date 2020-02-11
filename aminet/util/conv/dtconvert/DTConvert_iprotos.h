#include <exec/types.h>
#include <stdio.h>

/* DTConvert.c */
void chkabort ( void );
long wbmain ( struct WBStartup *wbstartup );
int main ( int ac , STRPTR *av );
void Message ( STRPTR fmt , ...);

/* gui.c */
int SetupScreen ( void );
void CloseDownScreen ( void );
int OpenDTConvert_MAINWindow ( void );
void CloseDTConvert_MAINWindow ( void );
int OpenDTConvert_DATATYPEWindow ( void );
void CloseDTConvert_DATATYPEWindow ( void );

/* misc.c */
struct Node *GetNumNode ( struct List *list , ULONG num );
#if !defined(__AROS__)
void mysprintf ( STRPTR buffer , STRPTR fmt , ...);
APTR AllocVecPooled ( APTR poolheader , ULONG memsize );
void FreeVecPooled ( APTR poolheader , APTR mem );
#else
#define mysprintf sprintf
#endif
STRPTR GetLockName ( BPTR lock , STRPTR name );
void AttemptOpenLibrary ( struct Library **library , STRPTR title , STRPTR libname , ULONG libversion );
void ClearMsgPort ( struct MsgPort *port );

/* convert.c */
void Convert ( STRPTR srcname , STRPTR datatypename , STRPTR destname );
