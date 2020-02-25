
#include "common.h"

struct WindowList*          globalActiveWindow;
struct oCanvas*             globalActiveCanvas;
int                         globalWindowIncrement;
int                         globalEvents;
int                         globalCurrentTool;
int                         globalGrid; // Using grid?
int                         globalCurrentGrid; // Grid size

int                         eMouseX; // event mouse coordinate
int                         eMouseY; // --||--
double                      cMouseX; // Registered coordinates..
double                      cMouseY; // --||--
double                      dMouseX; // Coordinate for drawing
double                      dMouseY; // --||--
double                      pMouseX; // Prev coordinate for drawing
double                      pMouseY; // --||--

BOOL                        MouseButtonL;
BOOL                        MouseButtonR;
BOOL                        globalAntialiasing;
BOOL                        globalFeather;
int                         globalBrushMode; // Normal generated or clipbrush
int                         globalColorMode; // How to use colors
UWORD                       lastKeyPressed;
BOOL                        MouseHasMoved;
