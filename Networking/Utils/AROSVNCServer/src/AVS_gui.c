/*
**         $Filename: AVS_gui.c $
**         $Release: 2 $
**         $Revision: 1 $
**         $Date: 2014 $
**
**         (C) Copyright 2010-2014 Yannick Erb
**         GNU General Public License
*/

#include <proto/exec.h>
#include <proto/dos.h>
#include <aros/debug.h>
#include <proto/alib.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/utility.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <proto/commodities.h>
#include <proto/bsdsocket.h>

#include <libraries/mui.h>

#include <rfb/rfb.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "ArosVNCserver.h"
#include "AVS_gui.h"


/* GUI variables */
APTR MuiApp = NULL, MuiWin, StatWin, LogWin;
APTR But_Play, But_Pause, But_LogON, But_LogOFF, But_Stats, But_Shutdown, But_Hide, Text_Log;
APTR Level_FBTime, Slider_FPS;

static struct Hook broker_hook;

/*---------------------------- MUI Application ----------------------------*/

/*** broker_func ************************************************************/
AROS_UFH3(void, broker_func,
    AROS_UFHA(struct Hook *, h,      A0),
    AROS_UFHA(Object *     , object, A2),
    AROS_UFHA(CxMsg *      , msg,    A1))
{
    AROS_USERFUNC_INIT
    if (CxMsgType(msg) == CXM_COMMAND)
    {
		switch(CxMsgID(msg))
		{
			case CXCMD_APPEAR:
				DoMethod(MuiWin, MUIM_Set, MUIA_Window_Open, TRUE);
				break;
			case CXCMD_DISAPPEAR:
				DoMethod(MuiWin, MUIM_Set, MUIA_Window_Open, FALSE);
				break;
			case CXCMD_KILL:
				shutDownRequest = (ULONG)TRUE;
				break;
		}
    }
    AROS_USERFUNC_EXIT
}

/*----------------------------------------------------------------------------*/
/*    MakeButton                                                              */
/*----------------------------------------------------------------------------*/
static APTR MakeButton(UBYTE *Image, UBYTE Key, UBYTE *Help)
{
   return(MUI_NewObject("Dtpic.mui",
                        MUIA_Dtpic_Name,Image,
                        MUIA_InputMode, MUIV_InputMode_RelVerify,
                        MUIA_ControlChar, Key,
                        MUIA_Background, MUII_ButtonBack,
                        MUIA_ShortHelp, Help,
                        ImageButtonFrame,
                        TAG_DONE));
}

/*----------------------------------------------------------------------------*/
/*    MakeToggle                                                              */
/*----------------------------------------------------------------------------*/
static APTR MakeToggle(UBYTE *Image, UBYTE Key, UBYTE *Help)
{
   return(MUI_NewObject("Dtpic.mui",
                        MUIA_Dtpic_Name,Image,
                        MUIA_InputMode, MUIV_InputMode_Toggle,
						MUIA_ShowSelState, FALSE,
                        MUIA_ControlChar, Key,
                        MUIA_Background, MUII_ButtonBack,
                        MUIA_ShortHelp, Help,
                        ImageButtonFrame,
                        TAG_DONE));
}

/*----------------------------------------------------------------------------*/
/*    MakeLevel                                                               */
/*----------------------------------------------------------------------------*/
static APTR MakeLevel(UBYTE *Label, LONG Max)
{
	return (GaugeObject,
			MUIA_Gauge_InfoText, Label,
			MUIA_Gauge_Max, Max,
			MUIA_Frame, MUIV_Frame_Gauge,
			MUIA_Gauge_Horiz, TRUE,
			End);
}

/*----------------------------------------------------------------------------*/
/*    MakeSlider                                                              */
/*----------------------------------------------------------------------------*/
static APTR MakeSlider(LONG Min, LONG Max, LONG Current)
{
	return (SliderObject,
			MUIA_Numeric_Value , Current,
			MUIA_Numeric_Max, Max,
			MUIA_Numeric_Min, Min,
			MUIA_Slider_Horiz, TRUE,
			End);
}


/*----------------------------------------------------------------------------*/
/*    MakeMUIApp                                                              */
/*----------------------------------------------------------------------------*/
BOOL MakeMUIApp(void)
{
	broker_hook.h_Entry = (HOOKFUNC)broker_func;
	
	if (!(MuiApp =	ApplicationObject,
					MUIA_Application_Title,       "ArosVNCserver",
					MUIA_Application_Version,     "$VER: ArosVNCserver " VERSION " (" DATE ")",
					MUIA_Application_Copyright,   "© " COPYRIGHT " Yannick Erb",
					MUIA_Application_Author,      "Yannick Erb",
					MUIA_Application_Description, "A VNC Server for AROS",
					MUIA_Application_Base,        "AVNCS",
					MUIA_Application_SingleTask,  TRUE,
					MUIA_Application_BrokerPri,   0,
					MUIA_Application_BrokerHook,  (IPTR)&broker_hook,

					SubWindow, MuiWin = WindowObject,
										MUIA_Window_Title, "ArosVNCserver",
										MUIA_Window_ID, MAKEID('A','V','N','C'),     
                                        MUIA_Window_CloseGadget, FALSE,
                                        
										WindowContents, HGroup, 
														MUIA_Group_Spacing, 5, 
														MUIA_Group_SameWidth, TRUE,
														Child, But_Play 		= MakeButton((UBYTE *)"PROGDIR:Icons/transmit.png",	'p', (UBYTE *)"Play"),
														Child, But_Pause 		= MakeButton((UBYTE *)"PROGDIR:Icons/pause.png",	'p', (UBYTE *)"Pause"),
														Child, But_LogON		= MakeButton((UBYTE *)"PROGDIR:Icons/log_on.png",	'l', (UBYTE *)"Log On"),
														Child, But_LogOFF		= MakeButton((UBYTE *)"PROGDIR:Icons/log_off.png",	'l', (UBYTE *)"Log Off"),
														Child, But_Stats 		= MakeToggle((UBYTE *)"PROGDIR:Icons/settings.png", 's', (UBYTE *)"Statistics"),
														Child, But_Hide 		= MakeButton((UBYTE *)"PROGDIR:Icons/hide.png",		'h', (UBYTE *)"Hide"),
														Child, But_Shutdown 	= MakeButton((UBYTE *)"PROGDIR:Icons/shutdown.png",	'x', (UBYTE *)"Shutdown server"),
										End,        
					End,
					SubWindow, StatWin = WindowObject,
										MUIA_Window_Title, "ArosVNCserver Stat",
										MUIA_Window_ID, MAKEID('A','V','N','S'),
                                        MUIA_Window_CloseGadget, FALSE,                                        

										WindowContents, VGroup, 
														MUIA_Group_Spacing, 5, 
														Child, HGroup,
															Child, VGroup,
																Child, Label1((UBYTE *)"Current"),
																Child, Label1((UBYTE *)"Target FPS"),
															End,
															Child, VGroup,
																Child, Level_FBTime = MakeLevel((UBYTE *)"FPS" , 100),
																Child, Slider_FPS 	= MakeSlider(1, 100, Target_fps),
															End,
														End,
										End,
					End,
					SubWindow, LogWin = WindowObject,     
										MUIA_Window_Title, "ArosVNCserver Logs",
										MUIA_Window_ID, MAKEID('A','V','N','L'), 
										MUIA_Window_CloseGadget, FALSE,
                                        
										WindowContents,	VGroup,
														MUIA_Group_Spacing, 5, 
														Child, HGroup,
															MUIA_Frame, MUIV_Frame_Group,
															MUIA_FrameTitle, "Last messages",
															Child, Text_Log = ListviewObject,
																MUIA_Listview_Input, FALSE,
																MUIA_Listview_List , ListObject,
																	ReadListFrame,
																End,
															End,
														End,
										End,				
					End,
	End)) return(FALSE);

	/* Second application start shall bring up first instance */
    DoMethod(MuiWin,		MUIM_Notify, MUIA_Application_DoubleStart,	TRUE,			MuiWin, 		3, MUIM_Set, 		MUIA_Window_Open,			TRUE);

	/* Iconification of the application is linked to window opening */
    DoMethod(MuiApp,		MUIM_Notify, MUIA_Application_Iconified,	FALSE,			MuiWin, 		3, MUIM_Set, 		MUIA_Window_Open,			TRUE);
    DoMethod(MuiApp,		MUIM_Notify, MUIA_Application_Iconified,	TRUE,			MuiWin, 		3, MUIM_Set, 		MUIA_Window_Open,			FALSE);
	DoMethod(But_Shutdown, 	MUIM_Notify, MUIA_Pressed, 					FALSE,			But_Shutdown,	3, MUIM_WriteLong,	(ULONG)TRUE,				&shutDownRequest);
	DoMethod(But_Hide, 		MUIM_Notify, MUIA_Pressed, 					FALSE,			MuiApp, 		3, MUIM_Set, 		MUIA_Application_Iconified, TRUE);
	DoMethod(Slider_FPS, 	MUIM_Notify, MUIA_Numeric_Value, 			MUIV_EveryTime,	Slider_FPS,		3, MUIM_WriteLong,	MUIV_TriggerValue,			&Target_fps);

	DoMethod(But_Play, 		MUIM_Notify, MUIA_Pressed, 					FALSE,			MuiApp, 		3, MUIM_WriteLong, 	FALSE,						&rfbPause);
	DoMethod(But_Pause, 	MUIM_Notify, MUIA_Pressed, 					FALSE,			MuiApp, 		3, MUIM_WriteLong, 	TRUE,						&rfbPause);
	DoMethod(But_Play, 		MUIM_Notify, MUIA_Pressed, 					FALSE,			But_Pause, 		3, MUIM_Set, 		MUIA_ShowMe,				TRUE);
	DoMethod(But_Play, 		MUIM_Notify, MUIA_Pressed, 					FALSE,			But_Play, 		3, MUIM_Set, 		MUIA_ShowMe,				FALSE);
	DoMethod(But_Pause, 	MUIM_Notify, MUIA_Pressed, 					FALSE,			But_Play, 		3, MUIM_Set, 		MUIA_ShowMe,				TRUE);
	DoMethod(But_Pause, 	MUIM_Notify, MUIA_Pressed, 					FALSE,			But_Pause, 		3, MUIM_Set, 		MUIA_ShowMe,				FALSE);
	
	DoMethod(But_LogON, 	MUIM_Notify, MUIA_Pressed, 					FALSE,			MuiApp, 		3, MUIM_WriteLong, 	TRUE,						&rfbEnableLogging);
	DoMethod(But_LogOFF, 	MUIM_Notify, MUIA_Pressed, 					FALSE,			MuiApp, 		3, MUIM_WriteLong, 	FALSE,						&rfbEnableLogging);
	DoMethod(But_LogON, 	MUIM_Notify, MUIA_Pressed, 					FALSE,			LogWin, 		3, MUIM_Set, 		MUIA_Window_Open,			TRUE);
	DoMethod(But_LogOFF, 	MUIM_Notify, MUIA_Pressed, 					FALSE,			LogWin, 		3, MUIM_Set, 		MUIA_Window_Open,			FALSE);
	DoMethod(But_LogON,		MUIM_Notify, MUIA_Pressed, 					FALSE,			But_LogOFF,		3, MUIM_Set, 		MUIA_ShowMe,				TRUE);
	DoMethod(But_LogON,		MUIM_Notify, MUIA_Pressed, 					FALSE,			But_LogON, 		3, MUIM_Set, 		MUIA_ShowMe,				FALSE);
	DoMethod(But_LogOFF, 	MUIM_Notify, MUIA_Pressed, 					FALSE,			But_LogON, 		3, MUIM_Set, 		MUIA_ShowMe,				TRUE);
	DoMethod(But_LogOFF, 	MUIM_Notify, MUIA_Pressed, 					FALSE,			But_LogOFF,		3, MUIM_Set, 		MUIA_ShowMe,				FALSE);

	DoMethod(But_Stats,		MUIM_Notify, MUIA_Selected, 				MUIV_EveryTime,	StatWin, 		3, MUIM_Set, 		MUIA_Window_Open,			MUIV_TriggerValue);
			
	/* initialise buttons according to settings */
	if (rfbPause)
		DoMethod(But_Pause, MUIM_Set, MUIA_Pressed, FALSE);
	else
		DoMethod(But_Play, MUIM_Set, MUIA_Pressed, FALSE);
		
	if (rfbEnableLogging)
		DoMethod(But_LogON, MUIM_Set, MUIA_Pressed, FALSE);
	else
		DoMethod(But_LogOFF, MUIM_Set, MUIA_Pressed, FALSE);

	return(TRUE);
}

/* Updates statistics window */
void UpdateStat(ULONG FB_FPS)
{
	char fpsstr[32];
	IPTR StatWinOpen = (IPTR)FALSE;
    
    get(StatWin, MUIA_Window_Open, &StatWinOpen);    
	if (StatWinOpen)
	{
		sprintf(fpsstr,"\33b\0332%d FPS", 		FB_FPS);
		DoMethod(Level_FBTime, MUIM_Set, MUIA_Gauge_Current, 	FB_FPS);
		DoMethod(Level_FBTime, MUIM_Set, MUIA_Gauge_InfoText, 	fpsstr);
	}
}

/* Aros specific log function */
#define MAX_MESSAGES	128
#define MESSAGE_LENGTH	128
char messages[MAX_MESSAGES][MESSAGE_LENGTH];
int  nb_messages = 0;

void rfbArosLog(const char *format, ...)
{
	va_list Arg;
    char buf1[MESSAGE_LENGTH];
	char buf2[MESSAGE_LENGTH];
    time_t log_clock;

	va_start(Arg, (char *)format);
	vsnprintf(buf2, MESSAGE_LENGTH-1, (char *)format, Arg);
	va_end(Arg);

	/* Always mirror log messages to the debug console */
	kprintf("[VNCserver] %s", buf2);

    if (LogWin && rfbEnableLogging)
    {
        if (nb_messages > MAX_MESSAGES)
        {
            DoMethod(Text_Log, MUIM_List_Remove, MUIV_List_Remove_Last);
        }
		time(&log_clock);
		strftime(buf1, MESSAGE_LENGTH-1, "%d/%m/%Y %X", localtime(&log_clock));
        if (buf2[0] && buf2[strlen(buf2)-1]=='\n')
            buf2[strlen(buf2)-1]=0; /* remove \n from end of line */
        snprintf(messages[nb_messages%MAX_MESSAGES], MESSAGE_LENGTH-1, "%s : %s", buf1,buf2);
        DoMethod(Text_Log, MUIM_List_InsertSingle, messages[nb_messages%MAX_MESSAGES], MUIV_List_Insert_Top);
        nb_messages++;
	}
}
