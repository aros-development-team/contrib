/*
**         $Filename: ArosVNCserver.h $
**         $Release: 2 $
**         $Revision: 1 $
**         $Date: 2014 $
**
**         (C) Copyright 2010-2014 Yannick Erb
**         GNU General Public License
*/

#define VERSION 	"2.1"
#define DATE		"August 2026"
#define COPYRIGHT	"2010 - 2026"

#ifndef max
#define max(i, j) ((i) > (j) ? (i) : (j))              			/* return the maximum of 2 numbers */
#endif
#ifndef min
#define min(i, j) ((i) < (j) ? (i) : (j))             			/* return the minimum of 2 numbers */
#endif
#define clip(lo, var, hi) max((lo), min((var), (hi)))			/* clip var to lie between lo and hi */

#ifndef isspace
#define isspace(x) ((x==' ') || (x=='\t') || (x=='\n') || (x=='\0'))
#endif

/*
 * Do NOT store or compare TRUE here: libvncserver's rfbproto.h defines
 * TRUE as -1, which written through a char becomes 0xFF. On targets where
 * plain char is unsigned (riscv64, arm, ppc) that reads back as 255 and
 * an "== TRUE" comparison never matches, so no tile is ever sent. Store
 * an explicit 1 and test for non-zero instead.
 */
#define set_dirty_bit(x,y) dirty_bits[x + y*tiles_wide] = 1			/* set the dirty bit for a pixel */
#define test_dirty_bit(x,y) (dirty_bits[x + y*tiles_wide] != 0)		/* check to see if a dirty bit is set */
#define reset_dirty_bit(x,y) dirty_bits[x + y*tiles_wide] = 0		/* reset the dirty bit for a pixel */

typedef struct _MyImage {
	int width;
	int height;					/* size of image */
	int depth;					/* depth of image */
	int bytes_per_line;			/* accelerator to next scanline */
	int bits_per_pixel;			/* bits per pixel */
	int bytes_per_pixel;		/* bytes per pixel */
	ULONG rectfmt;				/* RECTFMT of data */
	ULONG red_mask;
	ULONG green_mask;
	ULONG blue_mask;
	UBYTE *data;				/* pointer to image data */
} MyImage;

#include <stdint.h>

typedef struct ThreadData
{
	uint32_t thread;				// FB thread
	void *mutex;					// Thread mutex
	void *cond;						// Sync condition
	void *ScreenChangecond;			// Screen Change condition
} FBthread;

extern struct Screen *screen;
extern rfbScreenInfoPtr vncscreen;;
extern MyImage* LocalFB;
extern FBthread *framebufThread;

extern int tilewidth;								/* tile width in ULONGs*/
extern int tilewidth_pixel;							/* tile width in pixel */
extern int tileheight;								/* tile height in pixel*/

extern int tiles_high;  							/* how many tiles high the framebuffer is */
extern int tiles_wide;  							/* how many tiles wide the framebuffer is */
extern char* dirty_bits;							/* points to an array of flags, one per tile */
extern int* horizscanlines;							/* interlace pattern for scanning horizontally within a tile */
extern int* vertscanlines; 							/* interlace pattern for scanning vertically within a tile */
extern LONG Target_fps;								/* How often should the FB be updated */
extern ULONG CurrentFPS;

extern ULONG rfbEnableLogging;
extern ULONG shutDownServer;
extern ULONG shutDownRequest;
extern ULONG ScreenChange;
extern ULONG rfbPause;
extern int  Clients;
extern BOOL StartHidden;

void InitScreen(void);
void cleanup(char *msg);
