/*
**         $Filename: AVS_gui.h $
**         $Release: 2 $
**         $Revision: 1 $
**         $Date: 2014 $
**
**         (C) Copyright 2010-2014 Yannick Erb
**         GNU General Public License
*/

#define MAKEID(a,b,c,d) ((ULONG)(a)<<24|(ULONG)(b)<<16|(ULONG)(c)<<8|(ULONG)(d))

/* GUI variables */
extern APTR MuiApp, MuiWin, StatWin, LogWin;

/* Proto */
BOOL MakeMUIApp(void);
void UpdateStat(ULONG FB_Time);
void rfbArosLog(const char *format, ...);
