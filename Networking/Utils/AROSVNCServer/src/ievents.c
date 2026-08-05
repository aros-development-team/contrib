/*
**         $Filename: ievents.c $
**         $Release: 2 $
**         $Revision: 1 $
**         $Date: 2014 $
**
**         (C) Copyright 2010-2014 Yannick Erb
**         GNU General Public License
*/

/*
	This file is modified from AmiVNC
	
	AmiVNC - Amiga experimental VNC server - Protocol ORL version 3.3
	(c) Stephane Guillard - stephane.guillard@steria.com
	This module is the mouse / key event handler.
	It is largely based on another module written by my friend Denis Spach for his own AVNC server,
	and as it solved a NEWPOINTERPOS problem, it went into AmiVNC as well.
*/

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/keymap.h>
#include <devices/rawkeycodes.h>
#include <devices/inputevent.h>
#include <intuition/intuition.h>
#include <clib/exec_protos.h>
#include <clib/commodities_protos.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <proto/commodities.h>

#include <rfb/rfb.h>
#include <rfb/keysym.h>

/*
* mouse buttons events preset
*/
static struct InputEvent btevent =
{
	NULL,							/* NextEvent			*/
	IECLASS_RAWMOUSE,				/* Class			*/
	0L,								/* SubClass			*/
	0L,								/* Code event	*/
	0L,								/* Qualifier event	*/
	{{ 0L, 0L }},					/* Position			*/
	{{ 0L }}						/* TimeStamp		event	*/
};

/*
* mouse move events preset
*/
static struct {
	struct InputEvent	ie;
	struct IEPointerPixel	pp;
} ppevent =
{
	{
		NULL,						/* NextEvent			*/
		IECLASS_NEWPOINTERPOS,		/* Class			*/
		IESUBCLASS_PIXEL,			/* SubClass			*/
		IECODE_NOBUTTON,			/* Code 			*/
		0L,							/* Qualifier		event	*/
		{{ 0L, 0L }},					/* APTR on pp		init	*/
		{{ 0L }}					/* TimeStamp		event	*/
	},	
	{	
		NULL,						/* Screen		event	*/
		{0, 0}						/* position		event	*/
	}
};

/*
* key events preset
*/
static struct InputEvent kevent =
{
	NULL,				/* NextEvent			*/
	IECLASS_RAWKEY, 	/* Class			*/
	0L,					/* SubClass			*/
	0L,					/* Code 		event	*/
	0L,					/* Qualifier		event	*/
	{{ 0L, 0L }}, 		/* Position			*/
	/* position qualifier will be	*/
	/* always RELATIVE, with delta	*/
	/* sets to 0 (no need to know	*/
	/* the screen dimensions)	*/
	{{ 0L }}			/* TimeStamp		event	*/
};

int qual = 0;

void	do_key(struct IOStdReq *inputReqBlk, int dn, int key)
{
	int	code = 0;
	UBYTE rBuffer[6];
	UBYTE Buffer[6];

	kevent.ie_NextEvent = 0; //NULL;
	kevent.ie_Class     = IECLASS_RAWKEY;
	kevent.ie_SubClass  = 0;//IESUBCLASS_COMPATIBLE;
	
	switch(key)
	{
		case XK_Home: 		code = RAWKEY_HOME; 		break;
		case XK_Left: 		code = RAWKEY_LEFT; 		break;
		case XK_Up: 		code = RAWKEY_UP; 			break;
		case XK_Right: 		code = RAWKEY_RIGHT; 		break;
		case XK_Down: 		code = RAWKEY_DOWN; 		break;
		case XK_Page_Up: 	code = RAWKEY_PAGEUP; 		break;
		case XK_Page_Down: 	code = RAWKEY_PAGEDOWN; 	break;
		case XK_End: 		code = RAWKEY_END; 			break;
		
		case XK_KP_0:		code = RAWKEY_KP_0;			break;
		case XK_KP_1:		code = RAWKEY_KP_1;			break;
		case XK_KP_2:		code = RAWKEY_KP_2;			break;
		case XK_KP_3:		code = RAWKEY_KP_3;			break;
		case XK_KP_4:		code = RAWKEY_KP_4;			break;
		case XK_KP_5:		code = RAWKEY_KP_5;			break;
		case XK_KP_6:		code = RAWKEY_KP_6;			break;
		case XK_KP_7:		code = RAWKEY_KP_7;			break;
		case XK_KP_8:		code = RAWKEY_KP_8;			break;
		case XK_KP_9:		code = RAWKEY_KP_9;			break;
		case XK_KP_Decimal:	code = RAWKEY_KP_DECIMAL;	break;	
		case XK_KP_Add:		code = RAWKEY_KP_PLUS;		break;
		case XK_KP_Enter:	code = RAWKEY_KP_ENTER;		break;
//		case XK_KP_Multiply:
//		case XK_KP_Separator:
//		case XK_KP_Subtract:
//		case XK_KP_Divide:
		
		case XK_BackSpace:	code = RAWKEY_BACKSPACE;	break;
		case XK_Tab:        code = RAWKEY_TAB;			break;
		case XK_Return:     code = RAWKEY_RETURN;		break;
		case XK_Escape:     code = RAWKEY_ESCAPE;		break;
		case XK_Delete:     code = RAWKEY_DELETE;		break;
		case XK_Insert:     code = RAWKEY_INSERT;		break;
		case XK_Help:       code = RAWKEY_HELP;			break;

		case XK_F1:			code = RAWKEY_F1;			break;
		case XK_F2:			code = RAWKEY_F2;			break;
		case XK_F3:			code = RAWKEY_F3;			break;
		case XK_F4:			code = RAWKEY_F4;			break;
		case XK_F5:			code = RAWKEY_F5;			break;
		case XK_F6:			code = RAWKEY_F6;			break;
		case XK_F7:			code = RAWKEY_F7;			break;
		case XK_F8:			code = RAWKEY_F8;			break;
		case XK_F9:			code = RAWKEY_F9;			break;
		case XK_F10:		code = RAWKEY_F10;			break;
		case XK_F11:		code = RAWKEY_F11;			break;
		case XK_F12:		code = RAWKEY_F12;			break;

		case XK_Shift_L:
			code = RAWKEY_LSHIFT;
			if (dn) qual |=  IEQUALIFIER_LSHIFT;
			else qual &= ~IEQUALIFIER_LSHIFT;
			break;
			
		case XK_Shift_R:
			code = RAWKEY_RSHIFT;
			if (dn) qual |=  IEQUALIFIER_RSHIFT;
			else qual &= ~IEQUALIFIER_RSHIFT;
			break;
			
		case XK_Control_L:
		case XK_Control_R:
			code = RAWKEY_CONTROL;
			if (dn) qual |=  IEQUALIFIER_CONTROL;
			else qual &= ~IEQUALIFIER_CONTROL;
			break;
			
		case XK_Alt_L:
			code = RAWKEY_LALT;
			if (dn) qual |=  IEQUALIFIER_LALT;
			else qual &= ~IEQUALIFIER_LALT;
			break;
			
		case XK_Alt_R:
			code = RAWKEY_RALT;
			if (dn) qual |=  IEQUALIFIER_RALT;
			else qual &= ~IEQUALIFIER_RALT;
			break;

		case XK_Super_L:
			code = RAWKEY_LAMIGA;
			if (dn) qual |=  IEQUALIFIER_LCOMMAND;
			else qual &= ~IEQUALIFIER_LCOMMAND;
			break;
			
		case XK_Super_R:
			code = RAWKEY_RAMIGA;
			if (dn) qual |=  IEQUALIFIER_RCOMMAND;
			else qual &= ~IEQUALIFIER_RCOMMAND;
			break;

		default:
			code = -1;	
			// Map out other 0xFF'ed keys
			key &= 0xFF;
			if (qual & IEQUALIFIER_CONTROL) key &= 0x1f;
			Buffer[0] = (UBYTE)key;
			if (MapANSI(Buffer, 1, rBuffer, 6, 0) == 1)
			{
				kevent.ie_Code		= rBuffer[0];
				kevent.ie_Qualifier	= rBuffer[1];
			}
			else
				return;
	}

	if (code == 0 || !dn) return;

	if (code > 0)
	{
		kevent.ie_Prev2DownCode = kevent.ie_Prev1DownCode;
		kevent.ie_Prev2DownQual = kevent.ie_Prev1DownQual;
		kevent.ie_Prev1DownCode = (UBYTE)kevent.ie_Code;
		kevent.ie_Prev1DownQual = (UBYTE)kevent.ie_Qualifier;
		kevent.ie_Code	= code & 0xff;
		kevent.ie_Qualifier = qual | ((code > 0xff) ? IEQUALIFIER_LSHIFT : 0);
	}
	
	inputReqBlk -> io_Length  = sizeof(kevent);
	inputReqBlk -> io_Data    = (APTR) &kevent;
	inputReqBlk -> io_Command = IND_WRITEEVENT;
	inputReqBlk -> io_Flags   = 0;

	DoIO((struct IORequest *) inputReqBlk);
}

void	do_pointer(struct IOStdReq *inputReqBlk, int buttons, int x, int y, struct Screen *screen)
{
	static int butn = 0;
	static BOOL alt = FALSE;

	if (x != screen->MouseX || y != screen->MouseY)
	{
		if(alt) 
		{
			ppevent.ie.ie_Class     = IECLASS_RAWMOUSE;
			ppevent.ie.ie_SubClass  = 0;
			ppevent.ie.ie_Qualifier = qual|IEQUALIFIER_RELATIVEMOUSE;
			ppevent.ie.ie_X = x-screen->MouseX;
			ppevent.ie.ie_Y = y-screen->MouseY;
        }
		else
		{
			ppevent.ie.ie_NextEvent 	= NULL;
			ppevent.ie.ie_Code	    	= IECODE_NOBUTTON;
			ppevent.ie.ie_Qualifier 	= qual;
			ppevent.ie.ie_Class	   		= IECLASS_NEWPOINTERPOS;
			ppevent.ie.ie_SubClass		= IESUBCLASS_PIXEL;
			ppevent.ie.ie_EventAddress 	= (APTR) &ppevent.pp;
			ppevent.pp.iepp_Position.X 	= x;
			ppevent.pp.iepp_Position.Y 	= y;
			ppevent.pp.iepp_Screen	   	= screen;
		}

		inputReqBlk -> io_Data    	= (APTR) &ppevent.ie;
		inputReqBlk -> io_Command 	= IND_WRITEEVENT;
		inputReqBlk -> io_Flags   	= 0;
		inputReqBlk -> io_Length = sizeof(ppevent);

		DoIO((struct IORequest *) inputReqBlk);
	}

	/*
	* do mouse buttons
	*/

	buttons &= (rfbButton1Mask | rfbButton2Mask | rfbButton3Mask);

	if (buttons != butn)
	{
		/* check left button change */
		if ((buttons ^ butn) & rfbButton1Mask)
		{
			btevent.ie_NextEvent = NULL;
			btevent.ie_Class     = IECLASS_RAWMOUSE;
			btevent.ie_SubClass  = IESUBCLASS_COMPATIBLE;
			btevent.ie_Code      = IECODE_LBUTTON;
			btevent.ie_Qualifier = IEQUALIFIER_RELATIVEMOUSE; // | IEQUALIFIER_LEFTBUTTON
			btevent.ie_X	     = 0;
			btevent.ie_Y	     = 0;

			if (buttons & rfbButton1Mask)
			{
				/* button down */
				qual		 |= IEQUALIFIER_LEFTBUTTON;
				alt = TRUE;
			}
			else
			{
				/* button up */
				btevent.ie_Code	 |= IECODE_UP_PREFIX;
				qual		 &= ~IEQUALIFIER_LEFTBUTTON;
				alt = FALSE;
			}

			btevent.ie_Qualifier |= qual;

			inputReqBlk -> io_Length    = sizeof(btevent);
			inputReqBlk -> io_Data      = (APTR) &btevent;
			inputReqBlk -> io_Command   = IND_WRITEEVENT;
			inputReqBlk -> io_Flags     = 0;

			DoIO((struct IORequest *) inputReqBlk);
		}

		/* check middle button change */
		if ((buttons ^ butn) & rfbButton2Mask)
		{
			btevent.ie_NextEvent = NULL;
			btevent.ie_Class     = IECLASS_RAWMOUSE;
			btevent.ie_SubClass  = IESUBCLASS_COMPATIBLE;
			btevent.ie_Code      = IECODE_MBUTTON;
			btevent.ie_Qualifier = IEQUALIFIER_RELATIVEMOUSE; // | IEQUALIFIER_MIDBUTTON
			btevent.ie_X	     = 0;
			btevent.ie_Y	     = 0;

			if (buttons & rfbButton2Mask)
			{
				/* button down */
				qual		 |= IEQUALIFIER_MIDBUTTON;
				alt = TRUE;
			}
			else
			{
				/* button up */
				btevent.ie_Code	 |= IECODE_UP_PREFIX;
				qual		 &= ~IEQUALIFIER_MIDBUTTON;
				alt = FALSE;
			}

			btevent.ie_Qualifier |= qual;

			inputReqBlk -> io_Length    = sizeof(btevent);
			inputReqBlk -> io_Data      = (APTR) &btevent;
			inputReqBlk -> io_Command   = IND_WRITEEVENT;
			inputReqBlk -> io_Flags     = 0;

			DoIO((struct IORequest *) inputReqBlk);
		}

		/* check right button change */
		if ((buttons ^ butn) & rfbButton3Mask)
		{
			btevent.ie_NextEvent = NULL;
			btevent.ie_Class     = IECLASS_RAWMOUSE;
			btevent.ie_SubClass  = IESUBCLASS_COMPATIBLE;
			btevent.ie_Code      = IECODE_RBUTTON;
			btevent.ie_Qualifier = IEQUALIFIER_RELATIVEMOUSE; // | IEQUALIFIER_RBUTTON
			btevent.ie_X	     = 0;
			btevent.ie_Y	     = 0;

			if (buttons & rfbButton3Mask)
			{
				/* button down */
				qual		 |= IEQUALIFIER_RBUTTON;
				alt = TRUE;
			}
			else
			{
				/* button up */
				btevent.ie_Code	 |= IECODE_UP_PREFIX;
				qual		 &= ~IEQUALIFIER_RBUTTON;
				alt = FALSE;
			}

			btevent.ie_Qualifier |= qual;

			inputReqBlk -> io_Length    = sizeof(btevent);
			inputReqBlk -> io_Data      = (APTR) &btevent;
			inputReqBlk -> io_Command   = IND_WRITEEVENT;
			inputReqBlk -> io_Flags     = 0;

			DoIO((struct IORequest *) inputReqBlk);
		}

		butn = buttons;
	}
}
