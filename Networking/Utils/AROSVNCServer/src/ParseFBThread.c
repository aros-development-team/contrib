/*
**         $Filename: ParseFBThread.c $
**         $Release: 2 $
**         $Revision: 1 $
**         $Date: 2014 $
**
**         (C) Copyright 2010-2014 Yannick Erb
**         GNU General Public License
*/

#include <string.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <graphics/gfx.h>
#include <cybergraphx/cybergraphics.h>
#include <devices/timer.h>
#include <rfb/rfb.h>

#include "thread_compat.h"
#include "ArosVNCserver.h"

struct timerequest 		*tr = NULL;
FBthread *framebufThread_loc;

/* Copy tile from display to FramBuffer */
static inline void copytile(int column, int row)
{
	int x, y, w, h;
	
	if (screen)
	{
		/* upper left corner of the tile */
		x = column * tilewidth_pixel;
		y = row * tileheight;
		/* width and height of the tile */
		w = min(x + tilewidth_pixel, LocalFB->width) - x;
		h = min(y + tileheight, LocalFB->height) - y;
        
        set_dirty_bit(column, row);
		ReadPixelArray(LocalFB->data, x, y, LocalFB->bytes_per_line, &screen->RastPort, x, y, w, h, LocalFB->rectfmt);
	}
}

/* 
	probe horizontally, looking for changed spots

	Scan a width by 1 image, comparing a 1 tilewidth chunk at a time.  If a difference is discovered, mark the tile as dirty,
	then move ahead to the next tile.  Do this for each row of tiles in the live framebuffer.  Note that only one scan line is
	being checked in each row of tiles.
*/
void probe_line(int row, int line, UBYTE *srcline) 
{
	int        x = 0;                 							    /* pixel column, row is given as argument */
	int        column = 0;            								/* tile column index */
	ULONG*     src;
	ULONG*     dst;
	UBYTE*	   dstline;
	int    	   deltax = 4 / LocalFB->bytes_per_pixel;

	/* starting position in framebuffers.  Both line pointers are kept as
	   UBYTE * so that the per-tile jumps below index them in bytes - as
	   a ULONG * dstline advanced four times too far, desynchronising the
	   comparison from src after the first dirty tile in the row. */
	src = (ULONG *)(srcline);
	dstline = LocalFB->data + line*LocalFB->bytes_per_line;
	dst = (ULONG *)dstline;

	while (x<LocalFB->width)
	{
		if (test_dirty_bit(column, row))
		{
			/* Tile is already set as dirty, jump to next tile */
			column++;
			x = column * tilewidth_pixel;
			src = (ULONG *)(srcline + x*LocalFB->bytes_per_pixel);
			dst = (ULONG *)(dstline + x*LocalFB->bytes_per_pixel);
		}	
		else if (*src != *dst)
		{
			/* copy tile to frame buffer and set it as dirty */
			copytile(column, row);
			/* and jump to next one */
			column++;
			x = column * tilewidth_pixel;
			src = (ULONG *)(srcline + x*LocalFB->bytes_per_pixel);
			dst = (ULONG *)(dstline + x*LocalFB->bytes_per_pixel);
		}
		else
		{
			/* Pixel hasn't changed, test next one */
			x += deltax;
			column = x / tilewidth_pixel;		
			src++;
			dst++;
		}
	}
}

/* timing help functions */
ULONG GetTimeMs(void)
{
	tr->tr_node.io_Command = TR_GETSYSTIME;
	DoIO((struct IORequest *)tr);
	return (tr->tr_time.tv_secs * 1000 + tr->tr_time.tv_micro / 1000);
}

void WaitMs(ULONG msec)
{
	tr->tr_node.io_Command = TR_ADDREQUEST;
	tr->tr_time.tv_secs = msec / 1000;
	tr->tr_time.tv_micro = 1000 * (msec % 1000);
	DoIO((struct IORequest *)tr);
}

void FramebufferThread(void *data)
{
	int row;
	struct Screen *first;
	static int probe_y = 0;
	struct Task *ThisTask;
	LONG TaskPri = 0;
	struct MsgPort *mp = NULL;
	int updates = 0;
	ULONG StartTime = 0, TotalTime = 0, WaitTime = 1;
    int DeltaTime;
	static UBYTE *ParseLine = NULL;

    /* Get pointer to thread data */
    framebufThread_loc = (FBthread *)data;

	/* Get pointer to current task */
	ThisTask = FindTask(NULL);

	/* Init timer */
    mp = CreateMsgPort();
    if (mp)
    {
        tr = (struct timerequest *)CreateIORequest(mp, sizeof(struct timerequest));
        if (tr)
        {
            FreeSignal(mp->mp_SigBit);
            if (OpenDevice((UBYTE *)"timer.device", UNIT_MICROHZ, (struct IORequest *)tr, 0))
			{
				/* couldn't open timer.device */
				DeleteIORequest((struct IORequest *)tr);
				mp->mp_SigBit = AllocSignal(-1);
                return;
			}
			/* Allocate a signal within this task context */
			tr->tr_node.io_Message.mn_ReplyPort->mp_SigBit = SIGB_SINGLE;
			tr->tr_node.io_Message.mn_ReplyPort->mp_SigTask = FindTask(NULL);
	
			/* Allocate RAM buffer for scanned line when needed */
			ParseLine = AllocVec(LocalFB->bytes_per_pixel*LocalFB->width, MEMF_ANY);

			if (ParseLine != NULL)
			{
				/* Framebuffer probe loop */
				while(!shutDownServer)
				{
					/* Only perform job if clients connected */
					if (Clients)
					{	
						if (TaskPri != 1)
						{
							TaskPri = 1;
							/* Higher priority of this task as otherwise nothing is show under high CPU load */
							SetTaskPri(ThisTask, TaskPri  );
						}
						
						/* Check if screen has changed */
						first = ((struct IntuitionBase *) IntuitionBase) -> FirstScreen;
						if (!first)
						{
							/* No screen open (yet) - nothing to
							   broadcast, keep checking for one */
							WaitMs(100);
						}
						else if (screen != first)
						{
							LockMutex(framebufThread_loc->mutex);
							rfbLog("FB Thread : First screen has changed\n");
							ScreenChange = (ULONG)TRUE;
							rfbLog("FB Thread : Waiting for screen data updates to be finalised\n");
							/* Wait for screen change in main thread */
							WaitCondition(framebufThread_loc->ScreenChangecond, framebufThread_loc->mutex);
							rfbLog("FB Thread : New screen data available\n");
							UnlockMutex(framebufThread_loc->mutex);
							
							FreeVec(ParseLine);
							ParseLine = AllocVec(LocalFB->bytes_per_pixel*LocalFB->width, MEMF_ANY);
							if (ParseLine == NULL)
							{
								shutDownServer = TRUE;
								rfbLog("FB Thread : Couldn't allocate memory for new framebuffer parsing\n");
							}
						}
						else
						{						
							for (row=0;row<tiles_high;row++)
							{
								/* Update parsed row in RAM */
								ULONG FB_Line = row*tileheight + horizscanlines[probe_y];

								if (FB_Line < LocalFB->height)
								{
									ReadPixelArray(ParseLine, 0, 0, LocalFB->bytes_per_line, &screen->RastPort, 0, FB_Line, LocalFB->width, 1, LocalFB->rectfmt);
									probe_line(row, FB_Line, ParseLine);
								}
							}

							/* move index of scanline for next run */
							probe_y++;            										/* move on to the next interlaced scan row next time */
							probe_y %= tileheight;										/* wrap around to stay within tile size */
							
							/* Pause thread to let other tasks run */
							WaitMs(WaitTime);
							updates++;
							
							/* Timing measurements : done every 32 loops */
							if (updates>31)
							{
								TotalTime = GetTimeMs()-StartTime;
								CurrentFPS = 32000 / TotalTime;
								
								/* update WaitTime */
								DeltaTime = (((int)TotalTime - (int)(32000 / Target_fps)) / 32);
								WaitTime = ((int)WaitTime<DeltaTime)?1:WaitTime-DeltaTime;
								updates = 0;
								StartTime = GetTimeMs();
							}
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
						
						/* Wait for connection to be established in main thread */
						rfbLog("FB Thread : waiting\n");
						LockMutex(framebufThread_loc->mutex);
						WaitCondition(framebufThread_loc->cond, framebufThread_loc->mutex);
						updates = 0;
						UnlockMutex(framebufThread_loc->mutex);	
						rfbLog("FB Thread : Running\n");
					}
				}
			}

			/* Free thread memory */
			if (ParseLine != NULL)	FreeVec(ParseLine);
			
			/* Close Timer */
			if (tr)
			{
				tr->tr_node.io_Message.mn_ReplyPort->mp_SigTask = NULL;
				tr->tr_node.io_Message.mn_ReplyPort->mp_SigBit = AllocSignal(-1);
				CloseDevice((struct IORequest *) tr);
				DeleteMsgPort(tr->tr_node.io_Message.mn_ReplyPort);
				DeleteIORequest((struct IORequest *)tr);
			}
		}
		else
		{
			DeleteMsgPort(mp);
		}
	}
	
	rfbLog("FB Thread : Ended\n");
	
	ExitThread(NULL);
}
