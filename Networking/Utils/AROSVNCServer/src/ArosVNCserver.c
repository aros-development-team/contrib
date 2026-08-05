/*
**         $Filename: ArosVNCserver.c $
**         $Release: 2 $
**         $Revision: 1 $
**         $Date: 2014 $
**
**         (C) Copyright 2010-2014 Yannick Erb
**         GNU General Public License
*/

/* ArosVNCserver.c a VNC server for AROS based on gemsvnc.c */
/* gemsvnc.c -- a framebuffer server for GE Medical Systems, but still under the GPL */
/* gemsvnc was derivved from x11vnc.c, a small clone of x0rfbserver by HexoNet */
/* Modified by rj@elilabs.com into gemsvnc.c Wed Nov  6 09:45:25 CST 2002 */
/* Modified by yannick.erb@free.fr into ArosVNCserver.c March 2010 - June 2014 */
/* -------------------------------------------------------------------------------------------------------------------------------- */

/* License

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License <http://www.gnu.org/licenses/gpl.txt>
for more details.

You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* -------------------------------------------------------------------------------------------------------------------------------- */


#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define KEYSYM_H
#undef Bool
#define KeySym RFBKeySym

#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/bsdsocket.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/commodities.h>
#include <libraries/mui.h>
#include <proto/alib.h>
#include <proto/keymap.h>
#include <devices/rawkeycodes.h>
#include <devices/inputevent.h>
#include <utility/hooks.h>
#include <graphics/gfx.h>
#include <cybergraphx/cybergraphics.h>
#include <rfb/rfb.h>

#include "thread_compat.h"

#include "ArosVNCserver.h"
#include "ilace.h"  				/* interlace sequence generator */
#include "ievents.h"				/* Input events handling */
#include "ParseFBThread.h"			/* Screen changes detection and update */
#include "AVS_gui.h"				/* GUI handling */

/* Arguments */
#define ARG_TEMPLATE "HELP/S,RUNFOREVER/S,VIEWONLY/S,SHARED/S,TARGETFPS/N,TW/N,TH/N,LOG/S,STARTHIDDEN/S,FORCE16B/S,UNKNOWN_IP,IPFILTER,LIBVNCSERVER/M"

#define ARG_HELP			0
#define ARG_RUNFOREVER		1
#define ARG_VIEWONLY		2
#define ARG_SHARED			3
#define ARG_TARGETFPS		4
#define ARG_TW				5
#define ARG_TH				6
#define ARG_LOG				7
#define ARG_STARTHIDDEN		8
#define ARG_FORCE16B		9
#define ARG_UNKNOWN_IP		10
#define ARG_IPFILTER		11
#define ARG_LIBVNCSERVER	12

#define NUM_ARGS    		13


struct Screen 			*screen;
struct IOStdReq 		*inputReqBlk = NULL;
struct MsgPort 			*inputPort = NULL;
struct Library 			*MUIMasterBase = NULL;

/* command line / WB arguments handling */
static struct RDArgs	*myargs = NULL;
static UBYTE	       **wbargs;
static IPTR 	      	 args[NUM_ARGS];
static struct WBStartup *wbstartup;
/* Argument passed to vnc server library */
int   					 argc_vnc = 1;
char 					*argv_vnc[20];

/* Actual server variables */
rfbScreenInfoPtr vncscreen 		= NULL;			/* the live X-server's framebuffer, et al */
MyImage* LocalFB           		= NULL;			/* Local copy of the remote framebuffer */

/* Settings */
LONG Target_fps 				= 20;
int tilewidth  					= 32;			/* tile width in ULONGs*/
int tilewidth_pixel				= 32;			/* tile width in pixel */
int tileheight 					= 32;			/* tile height in pixel*/
BOOL viewOnly   				= FALSE;
BOOL sharedMode 				= FALSE;
ULONG rfbEnableLogging 			= FALSE;
BOOL RunForever 				= FALSE;
BOOL StartHidden 				= FALSE;
BOOL Force16Bits				= FALSE;
#define ASK   		0
#define ACCEPT		1
#define REJECT   	2
int UnknownIP					= ASK;
ULONG *IPFilter					= NULL;

char* cur =
		"                  "
		"                  "
		"  x               "
		"  xx              "
		"  xxx             "
		"  xxxx            "
		"  xxxxx           "
		"  xxxxxx          "
		"  xxxxxxx         "
		"  xxxxxxxx        "
		"  xxxxx           "
		"  xx xx           "
		"  x   xx          "
		"      xx          "
		"       xx         "
		"       xx         "
		"        x         "
		"                  ";

ULONG rfbPause			= (ULONG)FALSE;
ULONG shutDownServer 	= (ULONG)FALSE;
ULONG shutDownRequest	= (ULONG)FALSE;
ULONG ScreenChange 		= (ULONG)FALSE;
int  Clients    = 0;

int tiles_high;  							/* how many tiles high the framebuffer is */
int tiles_wide;  							/* how many tiles wide the framebuffer is */
char *dirty_bits = NULL;					/* points to an array of flags, one per tile */
int  *horizscanlines = NULL;				/* interlace pattern for scanning within a tile */
ULONG CurrentFPS = 0;

FBthread *framebufThread = NULL;

/* prototypes */
static void CreateMyImage(MyImage **i, LONG Width, LONG Height, LONG pixfmt);
static void FreeMyImage(MyImage **i);


/*---------------------------- the hooks ----------------------------*/

/* Check if Ip is accepted or prompt user for acceptance */
BOOL CheckIP(char* str_hostIP)
{
	ULONG hostIP;
	int a,b,c,d,i=0;
	BOOL knownIP = FALSE;
	char msg[255];
	
	if (UnknownIP==ACCEPT) return TRUE;
	
	if (IPFilter)
	{
		sscanf(str_hostIP,"%d.%d.%d.%d",&a,&b,&c,&d);
		hostIP = (ULONG)((ULONG)(a << 24) + (ULONG)(b << 16) + (ULONG)(c << 8) + (ULONG)d);
		
		while (!knownIP && IPFilter[i])
		{
			ULONG maskIP = 0xFFFFFFFF;
			
			if ((IPFilter[i] & 0xFF000000) == 0xFF000000) maskIP &= 0x00FFFFFF;
			if ((IPFilter[i] & 0x00FF0000) == 0x00FF0000) maskIP &= 0xFF00FFFF;
			if ((IPFilter[i] & 0x0000FF00) == 0x0000FF00) maskIP &= 0xFFFF00FF;
			if ((IPFilter[i] & 0x000000FF) == 0x000000FF) maskIP &= 0xFFFFFF00;		
			if ((hostIP & maskIP) == (IPFilter[i++] & maskIP)) knownIP = TRUE;
		}
	}
	
	if (knownIP)
		/* IP is known => accept connection */
		return TRUE;
	
	if (UnknownIP==REJECT)
		/* Unknown IP are always rejected */
		return FALSE;
	
	/* Ask if Unknown IP should be accepted */
	rfbLog("Unknown IP %s - asking user for confirmation\n", str_hostIP);
	sprintf(msg,"\33cConnection request received\nfrom %s",str_hostIP);
	if (MUI_Request(MuiApp,MuiWin,0,"AROS VNC server","ACCEPT|REJECT",msg,TAG_DONE))
		return TRUE;
	else
		return FALSE;
				
}

void clientGone(rfbClientPtr cl)
{
	Clients--;
	
	rfbLog("One client disconnected, %d client(s) remaining\n",Clients);
	if (!Clients)
	{
		if (!RunForever)
		{
			/* Exit if all client gone */
			rfbLog("Last client disconnected, server shutdown requested\n");
			shutDownServer = (ULONG)TRUE;
		}
	}
}

enum rfbNewClientAction newClient(rfbClientPtr cl)
{
	if (CheckIP(cl->host))
	{
		cl->clientGoneHook = clientGone;

		if (viewOnly) 
		{
			cl->clientData = (void*)-1;
		}
		else
		{
			cl->clientData = (void*)0;
		}
		Clients++;
		if (Clients == 1)
		{
			/* Unlock FramebufferThread */
			SignalCondition(framebufThread->cond);
		}
		rfbLog("One client connected, %d client(s) total\n",Clients);

		return(RFB_CLIENT_ACCEPT);
	}
	else
	{
		rfbLog("One client connection refused, %d client(s) total\n",Clients);
		return(RFB_CLIENT_REFUSE);
	}
}

/* keyboard handling */
void keyboard(rfbBool down, rfbKeySym keySym, rfbClientPtr cl) 
{
	if ((SIPTR)(cl->clientData) == -1) {
		return;                                                      /* viewOnly */
	}
	
	do_key(inputReqBlk, (int) down, (int) keySym);
}


/* mouse handling */
void mouse(int buttonMask, int x, int y, rfbClientPtr cl)      
{
	/* update server with vncviewer's mouse data */
	/* buttonMask: state of remote mouse buttons */
	/* x: remote mouse x position */
	/* y: remote mouse y position */
	/* cl: the client vncviewer's state vector */
	
	if ((SIPTR)cl->clientData == -1)
	{                               
		/* view only : remote isn't allowed to take control => skip setting mouse data */
	}
	else
	{	
		/* perform mouse actions on server */
		do_pointer(inputReqBlk, buttonMask, x, y, screen);
		/* Report mouse position to all other clients that might be connected */
		rfbDefaultPtrAddEvent(buttonMask,x,y,cl);
	}
}

/* Report local mouse movements to connected clients */
static void set_cursor_position()
{
	rfbClientIteratorPtr iter;
	rfbClientPtr cl;

	if ((vncscreen->cursorX != screen->MouseX) || (vncscreen->cursorY != screen->MouseY))
	{
		vncscreen->cursorX = screen->MouseX;
		vncscreen->cursorY = screen->MouseY;
		
		iter = rfbGetClientIterator(vncscreen);
		while( (cl = rfbClientIteratorNext(iter)) ) {
			cl->cursorWasMoved = TRUE;
		}
		rfbReleaseClientIterator(iter);
	}
}


/* send changed tiles to the VNC viewer */
int send_changed_tiles_to_viewer(int MaxTilesPerCycle)
{
	int x_ul, y_ul, x_lr, y_lr;
	static int CurrentTile = 0;
    int ProcessedTiles = 0;
	int TilesSent	= 0;
	int NumTiles = tiles_high * tiles_wide;
	
	while ((ProcessedTiles < NumTiles) && (TilesSent < MaxTilesPerCycle))
	{
		/* Non-zero test - see the note at the dirty-bit macros in
		   ArosVNCserver.h; "== TRUE" breaks on unsigned-char targets */
		if (dirty_bits[CurrentTile])
		{
			/* send tile(s) to the VNC viewer */
			
			/* upper left corner of tile */
			x_ul = CurrentTile % tiles_wide * tilewidth_pixel;
			y_ul = CurrentTile / tiles_wide * tileheight;
			/* lower right corner of tile */
			x_lr = min(x_ul + tilewidth_pixel, LocalFB->width);
			y_lr = min(y_ul + tileheight, LocalFB->height);
			
			rfbMarkRectAsModified(vncscreen, x_ul, y_ul, x_lr, y_lr);
			dirty_bits[CurrentTile] = FALSE;
			TilesSent++;
		}
		CurrentTile++;
        if (CurrentTile >= NumTiles) CurrentTile = 0;
        ProcessedTiles++;
	}
	return(TilesSent);
}

void ChangeRFBScreen(void)
{
	/* Need to change FB sizes and send changes to client */
#ifdef USE_NEWFRAMEBUFFER
	MyImage* NewScreen = NULL;
	int j;
	ULONG pixfmt;
#endif
	LockMutex(framebufThread->mutex);
	rfbLog("Looking for the new screen data\n");
	/* get screen information */
	screen = ((struct IntuitionBase *) IntuitionBase) -> FirstScreen;
	if ((LocalFB->height != screen->Height) || (LocalFB->width != screen->Width))
	{
#ifdef USE_NEWFRAMEBUFFER
		if (Force16Bits)
			pixfmt = PIXFMT_BGR16PC;
		else
			pixfmt = GetCyberMapAttr(screen->RastPort.BitMap,CYBRMATTR_PIXFMT);

		CreateMyImage(&NewScreen, screen->Width, screen->Height, pixfmt);
		if (NewScreen)
		{
			rfbNewFramebuffer(	vncscreen, 
								(char *)NewScreen->data,
								NewScreen->width,
								NewScreen->height,
								NewScreen->bits_per_pixel,
								4,
								NewScreen->bytes_per_pixel);
								
			FreeMyImage(&LocalFB);
			LocalFB = NewScreen;

			/* how many tiles high is the framebuffer */	
			tiles_high = (LocalFB->height + tileheight - 1)/tileheight;
			/* how many tiles wide is the framebuffer (tile width is defined in ULONGs not in pixel!)*/
			tilewidth_pixel = (tilewidth * 4 / LocalFB->bytes_per_pixel);
			tiles_wide = (LocalFB->width + tilewidth_pixel - 1)/tilewidth_pixel;
			
			/* dirty bit initialisation (represented as a byte) for each tile */
			if (dirty_bits) free(dirty_bits);
			dirty_bits = (char*)malloc(tiles_wide*tiles_high);			
			for (j = 0; j < tiles_high*tiles_wide; j++) {
				dirty_bits[j] = 0;
			}                 
			
			rfbLog("Screen change Done\n");
		}
		else
#endif
		{
			rfbLog("Couldn't allocate FrameBuffer for new screen, keeping old resolution\n");
			/* we just clean the framebuffers so that we do not keep old screen data */
			memset(LocalFB->data, 0, LocalFB->bytes_per_line * LocalFB->height);
		}
	}
	
	ScreenChange = (ULONG)FALSE;
    UnlockMutex(framebufThread->mutex);
	SignalCondition(framebufThread->ScreenChangecond);
}

/*----------------------------  MyImage structure helping function  ---------------------------- */
static void FreeMyImage(MyImage **i)
{
	if (*i)
	{
		if ((*i)->data!=NULL) FreeVec((*i)->data);
		FreeVec((*i));
		(*i) = NULL;
	}
}

static void CreateMyImage(MyImage **i, LONG Width, LONG Height, LONG pixfmt)
{
	*i = (MyImage *)AllocVec(sizeof(MyImage), MEMF_ANY);
	if (*i)
	{
		/*
		 * Read every screen as BGRA32 and let cybergraphics convert.
		 * On a little-endian machine that is the classic
		 * r<<16|g<<8|b layout every viewer asks for, so the server
		 * never translates, no palette handling is needed, and a
		 * screen changing depth underneath us needs no rebuild.
		 * Describing the native format here is what painted the
		 * whole view in the wrong colours - the format table lumped
		 * all the 15/16-bit variants together.
		 */
		(*i)->width = Width;
		(*i)->height = Height;
		(*i)->depth = 24;
		(*i)->bits_per_pixel = 32;
		(*i)->red_mask   = 0x00FF0000;
		(*i)->green_mask = 0x0000FF00;
		(*i)->blue_mask  = 0x000000FF;
		(*i)->rectfmt = RECTFMT_BGRA32;

		(*i)->bytes_per_pixel = (*i)->bits_per_pixel / 8;
		(*i)->bytes_per_line  = (*i)->bytes_per_pixel * (*i)->width;
		(*i)->data = AllocVec((*i)->bytes_per_pixel*Width*Height, MEMF_ANY);
	}
}
	
void cleanup(char *msg)
{
    int i;
    
	if (msg)
	{
		rfbLog("%s\n",msg);
	}

	/* Close connection */		
	/* Freeing the Image structs */
	FreeMyImage(&LocalFB);
	
	if (dirty_bits) 	free(dirty_bits);
	if (horizscanlines) free(horizscanlines);
	if (IPFilter) 		FreeVec(IPFilter);
				
	/* Close Devices and Libraries */
	if (inputReqBlk)
	{
		CloseDevice((struct IORequest *) inputReqBlk);
		DeleteExtIO((struct IORequest *) inputReqBlk);
	}
	if (inputPort) DeletePort(inputPort);

	/* Close Windows and MUI application */
	if (MuiApp)
	{
		DoMethod(MuiWin,  MUIM_Set, MUIA_Window_Open, FALSE);
		DoMethod(StatWin, MUIM_Set, MUIA_Window_Open, FALSE);
		DoMethod(LogWin,  MUIM_Set, MUIA_Window_Open, FALSE);
		MUI_DisposeObject(MuiApp);
	}
	
	if (framebufThread->mutex)
	{
		DestroyMutex(framebufThread->mutex);
		framebufThread->mutex=NULL;
	}
	if (framebufThread->cond)
	{
		DestroyCondition(framebufThread->cond);
		framebufThread->cond=NULL;
	}
	if (framebufThread->ScreenChangecond)
	{
		DestroyCondition(framebufThread->ScreenChangecond);
		framebufThread->ScreenChangecond=NULL;
	}
	
	if (framebufThread) FreeVec(framebufThread);
    
	if (MUIMasterBase) 	CloseLibrary(MUIMasterBase);

	/* Free argv table */
	for (i=0; i < argc_vnc; i++)
	{
		if(argv_vnc[i]) free(argv_vnc[i]);
	}
	
	if (myargs) FreeArgs(myargs);
	ArgArrayDone();

	exit(0);
}

static void openlibs(void)
{		
	if (!(MUIMasterBase = OpenLibrary((UBYTE *)"muimaster.library", MUIMASTER_VMIN)))
        cleanup("Can't open muimaster.library!");
		
	// Create input.device message structure
	inputPort = CreatePort(NULL, 0);
	if (!inputPort) cleanup("Couldn't create inputPort");
	inputReqBlk = (struct IOStdReq *) CreateExtIO(inputPort, sizeof(struct IOStdReq));
	if (!inputReqBlk) cleanup("Couldn't create ExtIO");
	OpenDevice((UBYTE *)"input.device", 0, (struct IORequest *) inputReqBlk, 0);
}

/* ---------------- Parse Arguments ---------------- */
void ParseArgs(int argc, char** argv)
{
	int i;
	
	for (i=0; i < 20; i++) {
		argv_vnc[i] = NULL;
	}
	argv_vnc[0] = strdup("ArosVNCServer");

	if (argc)
	{
		if ((myargs = ReadArgs((UBYTE *)ARG_TEMPLATE, args, NULL)) != NULL)
    	{			
			if (args[ARG_HELP])
			{
				fprintf(stderr,
					"AROS VNC Server, version " VERSION " , " DATE ", © " COPYRIGHT " Yannick Erb\n\n"
					"Usage: ArosVNCserver {options}, where the options are either libvncserver options\n"
					"                          or aros-specific options.\n"
					"\n"
					"These are the AROS VNC specific options:\n"
					"\n"
					"HELP              Display this help message.\n"
					"STARTHIDDEN       Doesn't show GUI on startup.\n"
					"RUNFOREVER        Do not time out waiting for a client, and do not exit\n"
					"                    after the last client has disconnected.\n"
					"VIEWONLY          Only permit the remote view to view, but not to interact\n"
					"                    with the display being served.\n"
					"SHARED            Allow multiple clients to be connected simultaneously.\n"
					"TARGETFPS         Specify target frame per seconds for updates\n"
					"                    Lower number are better for high speed machines\n"
					"TW                Specify the width in pixels of the tiles the framebuffer\n"
					"                    is divided into.\n"
					"TH                Specify the height in pixels of the tiles the framebuffer\n"
					"                    is divided into.\n"
					"FORCE16B          Force 16 bits per pixel framebuffers even on 24 or 32 bits depth\n"
					"                    screens. This speeds up operations at the expense of graphic quality\n"
					"LOG               Display log information\n"
					"UNKNOWN_IP        Specify how to react when an unknown IP tries to connect:\n"
					"                    - ASK     : Open a requester for the user to answer\n"
					"                    - ACCEPT  : Always Accept\n"
					"                    - REJECT  : Always Reject Unknown IPs\n"
					"IPFILTER          Specify IPs that are allowed to connect, several filters can used,\n"
					"                    they shall be separated by \";\" \n"
					"                    examples : \n"
					"                      192.168.0.1      : this IP is allowed to connect\n"
					"                      192.168.0.255    : all IPs starting with 192.168.0. are allowed\n"
					"                      255.255.255.255  : Everyone is allowed\n"
					"LIBVNCSERVER      Options to be passed to libvncserver\n"
					"\n"
					);
				argv_vnc[argc_vnc++] = strdup("-help");
			}
			if (args[ARG_STARTHIDDEN])
				StartHidden = TRUE;
			if (args[ARG_RUNFOREVER])
				RunForever = TRUE;
			if (args[ARG_VIEWONLY])
				viewOnly = TRUE;
			if (args[ARG_SHARED])
				sharedMode = TRUE;
			if (args[ARG_FORCE16B])
				Force16Bits = TRUE;
			if (args[ARG_TARGETFPS])
				Target_fps = (int)*(IPTR *)args[ARG_TARGETFPS];
			if (args[ARG_TW])
				tilewidth = (int)*(IPTR *)args[ARG_TW];
			if (args[ARG_TH])
				tileheight = (int)*(IPTR *)args[ARG_TH];
			if (args[ARG_LOG])
				rfbEnableLogging = (ULONG)TRUE;
			if (args[ARG_UNKNOWN_IP])
			{
				if (strcmp((char *)args[ARG_UNKNOWN_IP],"ASK") == 0)
					UnknownIP = ASK;
				else if (strcmp((char *)args[ARG_UNKNOWN_IP],"ACCEPT") == 0)
					UnknownIP = ACCEPT;
				else
					UnknownIP = REJECT;
			}
			if (args[ARG_IPFILTER])
			{
				char *tmp_arg = (char *)args[ARG_IPFILTER];
				int numIP = 1;
				int a,b,c,d;
				
				while((tmp_arg = strpbrk(tmp_arg,";")) != NULL )
				{
					numIP++;
					tmp_arg++;
				}
				
				IPFilter = AllocVec((numIP+1)*sizeof(ULONG),MEMF_ANY);
				tmp_arg = strtok ((char *)args[ARG_IPFILTER],";");
				i = 0;
				while (tmp_arg != NULL)
				{
					sscanf(tmp_arg,"%d.%d.%d.%d",&a,&b,&c,&d);
					IPFilter[i++] = (ULONG)((ULONG)(a << 24) + (ULONG)(b << 16) + (ULONG)(c << 8) + (ULONG)d);
					tmp_arg = strtok (NULL, ";");
				}
				IPFilter[i] = 0;
			}
			if (args[ARG_LIBVNCSERVER])
			{
				i = 0;
				while(((char **)args[ARG_LIBVNCSERVER])[i]!=NULL) 
					argv_vnc[argc_vnc++] = strdup(((char **)args[ARG_LIBVNCSERVER])[i++]);
			}
		}
	}
	else
	{
		char *tmp=NULL;
		int tmp_beg = 0,tmp_end = 0;
		wbstartup = (struct WBStartup *)argv;
		wbargs = ArgArrayInit(argc, (UBYTE **)argv);
		
		RunForever = (strnicmp((char *)ArgString(wbargs, (UBYTE *)"RUNFOREVER", (UBYTE *)"YES"), "Y", 1) == 0);
		viewOnly = (strnicmp((char *)ArgString(wbargs, (UBYTE *)"VIEWONLY", (UBYTE *)"NO"), "Y", 1) == 0);
		StartHidden = (strnicmp((char *)ArgString(wbargs, (UBYTE *)"STARTHIDDEN", (UBYTE *)"NO"), "Y", 1) == 0);
		Force16Bits = (strnicmp((char *)ArgString(wbargs, (UBYTE *)"FORCE16B", (UBYTE *)"NO"), "Y", 1) == 0);
		sharedMode = (strnicmp((char *)ArgString(wbargs, (UBYTE *)"SHARED", (UBYTE *)"NO"), "Y", 1) == 0);
		Target_fps = ArgInt(wbargs, (UBYTE *)"TARGETFPS", Target_fps);
		tilewidth = ArgInt(wbargs, (UBYTE *)"TW", tilewidth);
		tileheight = ArgInt(wbargs, (UBYTE *)"TH", tileheight);
		rfbEnableLogging = (strnicmp((char *)ArgString(wbargs, (UBYTE *)"LOG", (UBYTE *)"NO"), "Y", 1) == 0);
		if (strnicmp((char *)ArgString(wbargs, (UBYTE *)"UNKNOWN_IP", (UBYTE *)"ASK"), "ASK", 3) == 0)
			UnknownIP = ASK;
		else if (strnicmp((char *)ArgString(wbargs, (UBYTE *)"UNKNOWN_IP", (UBYTE *)"ASK"), "ACC", 3) == 0)
			UnknownIP = ACCEPT;
		else
			UnknownIP = REJECT;
		if (strcmp((char *)ArgString(wbargs, (UBYTE *)"IPFILTER", (UBYTE *)"NULL"),"NULL")!=0)
		{
			char *tmp_arg = (char *)ArgString(wbargs, (UBYTE *)"IPFILTER", (UBYTE *)"");
			int numIP = 1;
			int a,b,c,d;
			
			while((tmp_arg = strpbrk(tmp_arg,";")) != NULL)
			{
				numIP++;
				tmp_arg++;
			}
			
			IPFilter = AllocVec((numIP+1)*sizeof(ULONG),MEMF_ANY);
			tmp_arg = strtok ((char *)ArgString(wbargs, (UBYTE *)"IPFILTER", (UBYTE *)""),";");
			i = 0;
			while (tmp_arg != NULL)
			{
				sscanf(tmp_arg,"%d.%d.%d.%d",&a,&b,&c,&d);
				IPFilter[i++] = (ULONG)((ULONG)(a << 24) + (ULONG)(b << 16) + (ULONG)(c << 8) + (ULONG)d);
				tmp_arg = strtok (NULL, ";");
			}
			IPFilter[i] = 0;
		}
	
		tmp = AllocVec(strlen((char *)ArgString(wbargs, (UBYTE *)"LIBVNCSERVER", (UBYTE *)""))+1,MEMF_ANY);
		strcpy(tmp, (char *)ArgString(wbargs, (UBYTE *)"LIBVNCSERVER", (UBYTE *)""));
		while(tmp_beg < strlen(tmp))
		{
			char tmp2[256];
		
			while (isspace(tmp[tmp_beg])) tmp_beg++;
			tmp_end = tmp_beg;
			while (!isspace(tmp[tmp_end])) tmp_end++;

			strncpy(tmp2,&tmp[tmp_beg],tmp_end-tmp_beg);
			tmp2[tmp_end-tmp_beg] = 0;
			argv_vnc[argc_vnc++] = strdup(tmp2);

			tmp_beg = tmp_end+1;
		}
		FreeVec(tmp);
	}
}
/* ---------------- End Of Parsing Arguments  ---------------- */

void InitScreen(void)
{
	int i;
	ULONG pixfmt;
	
	if (Force16Bits)
		pixfmt = PIXFMT_BGR16PC;
	else
		pixfmt = GetCyberMapAttr(screen->RastPort.BitMap,CYBRMATTR_PIXFMT);
	
	/* Initialise framebuffer */
	CreateMyImage(&LocalFB, screen->Width, screen->Height, pixfmt);
	if (!LocalFB)
	{
		cleanup("Couldn't initialise Frame Buffer");
	}
		
	/* Then proceed with screen initialisation */
	vncscreen = rfbGetScreen(	&argc_vnc, argv_vnc,
								LocalFB->width,
								LocalFB->height,
								LocalFB->bits_per_pixel,
								4, // this one is never used anywhere, so it could be whatever!
								LocalFB->bytes_per_pixel);
	if (!vncscreen)
	{
		cleanup("Couldn't initialise screen");
	}
	else
	{								
		/* Set VNC framebuffer */
		vncscreen->frameBuffer = (char *)LocalFB->data;

		/* Name advertised to the viewer (window title) */
		vncscreen->desktopName = "AROSVNCServer";

		/* Set default cursor */
		vncscreen->cursor = rfbMakeXCursor(18,18,cur,NULL);
		vncscreen->cursor->xhot = 1;
		vncscreen->cursor->yhot = 2;
		
		/* Set hooks */
		vncscreen->newClientHook = newClient;
		vncscreen->kbdAddEvent = keyboard;
		vncscreen->ptrAddEvent = mouse;
		if (sharedMode) vncscreen->alwaysShared = TRUE;

		vncscreen->deferUpdateTime = 10;
							
		vncscreen->paddedWidthInBytes = LocalFB->bytes_per_line;
		vncscreen->serverFormat.bitsPerPixel = LocalFB->bits_per_pixel;
		vncscreen->serverFormat.depth = LocalFB->depth;
		vncscreen->serverFormat.trueColour = TRUE;
		
		{
			// Always truecolour - LUT8 screens are converted to
			// BGRA32 by ReadPixelArray() using the screen's palette
			vncscreen->serverFormat.redShift = 0;                        // red
			if (LocalFB->red_mask) {
				while (!(LocalFB->red_mask & (1 << vncscreen->serverFormat.redShift))) {
					vncscreen->serverFormat.redShift++;
				}
			}
			vncscreen->serverFormat.redMax   = LocalFB->red_mask   >> vncscreen->serverFormat.redShift;

			vncscreen->serverFormat.greenShift = 0;                      // green
			if (LocalFB->green_mask) {
				while (!(LocalFB->green_mask & (1 << vncscreen->serverFormat.greenShift))) {
					vncscreen->serverFormat.greenShift++;
				}
			}
			vncscreen->serverFormat.greenMax = LocalFB->green_mask >> vncscreen->serverFormat.greenShift;

			vncscreen->serverFormat.blueShift = 0;                       // blue
			if (LocalFB->blue_mask) {
				while (!(LocalFB->blue_mask & (1 << vncscreen->serverFormat.blueShift))) {
					vncscreen->serverFormat.blueShift++;
				}
			}
			vncscreen->serverFormat.blueMax  = LocalFB->blue_mask  >> vncscreen->serverFormat.blueShift;
		}
	}
		
	/* generate the interlace sequence */
	if (horizscanlines) free(horizscanlines);
	horizscanlines = ilace(tileheight);
	/* how many tiles high is the framebuffer */	
	tiles_high = (LocalFB->height + tileheight - 1)/tileheight;
	/* how many tiles wide is the framebuffer (tile width is defined in ULONGs not in pixel!) */
	tilewidth_pixel = (tilewidth * 4 / LocalFB->bytes_per_pixel);
	tiles_wide = (LocalFB->width + tilewidth_pixel - 1)/tilewidth_pixel;

	/* dirty bit initialisation (represented as a byte) for each tile */
	if (dirty_bits) free(dirty_bits);
	dirty_bits = (char*)malloc(tiles_wide*tiles_high);	
	for (i = 0; i < tiles_high*tiles_wide; i++)	dirty_bits[i] = 0;       

	rfbLog("Screen initialisation Done\n");
}

/* ---------------- The Main Program ---------------- */
int main(int argc, char** argv) 
{
	int Events = 0,  NbTilesSent = 0;
    int MaxEventsPerCycle = 16;
    int MaxTilesPerCycle = 128;
	int gotInput;
	ULONG DispFPS = 0;
	LONG TaskPri = 0;
	struct Task *ThisTask;
			
	/* Initialise log functions early so failures are visible on the
	   debug console even when started with output redirected to NIL: */
	rfbLog=rfbArosLog;
	rfbErr=rfbArosLog;

	/* first open libraries and devices for AROS interactions */
	openlibs();

	/* Get pointer to current task */
	ThisTask = FindTask(NULL);

	/* Initialise thread structure */
	framebufThread = AllocVec(sizeof(FBthread), MEMF_ANY);
	framebufThread->mutex = NULL;
	framebufThread->cond  = NULL;
	framebufThread->ScreenChangecond = NULL;
	
	/* Parse arguments */
	ParseArgs(argc, argv);

	/* start GUI */
	if (!MakeMUIApp())
		cleanup("could not create window");
	
	/* Display window if requested */
	if ((!StartHidden) || (rfbEnableLogging))
    {
		DoMethod(MuiWin, MUIM_Set, MUIA_Window_Open, TRUE);
    }
    
	/* get screen information */
	screen = ((struct IntuitionBase *) IntuitionBase) -> FirstScreen;
	if (!screen)
		cleanup("Couldn't locate screen to broadcast");
	rfbLog("Broadcasting screen %dx%d\n", screen->Width, screen->Height);

	InitScreen();
    
    /* allows to update 1/2 of the screen at each cycle */
	MaxTilesPerCycle = (tiles_high*tiles_wide) / 2;
    
	if (vncscreen)
	{
		/* now that the data structure is ready, start the rfb server */
		rfbInitServer(vncscreen);
		rfbLog("Server initialised (listen socket %d, port %d)\n",
			vncscreen->listenSock, vncscreen->port);
		
		/* Initialise Framebuffers update thread */
		framebufThread->mutex				= CreateMutex();
		framebufThread->cond				= CreateCondition();
		framebufThread->ScreenChangecond 	= CreateCondition();
		/* Start Display Thread */
		framebufThread->thread = CreateThread((void *)FramebufferThread,(void *)framebufThread);

		/* Signal readiness. */
		rfbLog("AROS VNC Server ready, waiting for connection\n");
				
		/* the big loop -- we do this until something makes us exit the program */
		while(!shutDownServer)
		{
			/* check for GUI interaction */
			static ULONG Signals;

			DoMethod(MuiApp, MUIM_Application_NewInput, (IPTR)&Signals);
			if (rfbPause)
			{
				if ((!shutDownServer) && Signals) Wait(Signals);
			}
			else
			{
				if (Clients)
				{
					if (TaskPri != 1)
					{
						TaskPri = 1;
						/* Higher priority of this task as otherwise nothing is show under high CPU load */
						SetTaskPri(ThisTask, TaskPri  );
					}

					/* Check for Change Screen */
					if (ScreenChange)
					{
						ChangeRFBScreen();
					}

					/* Process events */
					Events = 0;
					do
					{
						gotInput = rfbProcessEvents(vncscreen, -1);
						Events += gotInput;
					}
					while ((Events < MaxEventsPerCycle) && gotInput);

					/* send next dirty tiles to the VNC viewer */
					NbTilesSent = send_changed_tiles_to_viewer(MaxTilesPerCycle);

					/* check for local mouse movements and report them to clients */
					set_cursor_position();
					
					/* Update statistics */
					if (DispFPS != CurrentFPS) 
					{
						DispFPS = CurrentFPS;
						UpdateStat(DispFPS);
					}					
				}
				else
				{
					if (TaskPri != -1)
					{
						TaskPri = -1;
						/* Lower priority of this task as we're waiting */
						SetTaskPri(ThisTask, TaskPri  );
					}

					/* No client is connected , check every 100ms for connection */
					rfbProcessEvents(vncscreen, -1);
					Delay(5);
				}
			}
			
			/* if shutdown request was received, ask for confirmation */
			if (shutDownRequest == TRUE)
			{
				if (MUI_Request(MuiApp,MuiWin,0,"AROS VNC server","YES|NO","Are you sure you want to close the server?",TAG_DONE))
					shutDownServer = TRUE;
				else
					shutDownRequest = FALSE;
			}
		}

		/* Wait for FBThread to finish */
		SignalCondition(framebufThread->cond);
		WaitThread(framebufThread->thread,NULL);

		rfbScreenCleanup(vncscreen);
	}
		
	cleanup("Server Shut Down");
	exit (0);
}
