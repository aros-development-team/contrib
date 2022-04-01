/*
 *
 * aga.h
 *
 */

#ifndef AMIGAOS_AGA_H
#define AMIGAOS_AGA_H

#include <exec/types.h>

extern int open_screen_aga(int width, int height, ULONG id);
extern void close_screen_aga(void);

#endif
