/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// r_vars.c: global refresh variables

#if	!id386

#include   "quakeapi.h"
#include	"quakedef.h"

// all global and static refresh variables are collected in a contiguous block
// to avoid cache conflicts.

//-------------------------------------------------------
// global refresh variables
//-------------------------------------------------------

// FIXME: make into one big structure, like cl or sv
// FIXME: do separately for refresh engine and driver

QEXTERN float	d_sdivzstepu, d_tdivzstepu, d_zistepu;
QEXTERN float	d_sdivzstepv, d_tdivzstepv, d_zistepv;
QEXTERN float	d_sdivzorigin, d_tdivzorigin, d_ziorigin;

QEXTERN fixed16_t	sadjust, tadjust, bbextents, bbextentt;

QEXTERN pixel_t			*cacheblock;
QEXTERN int				cachewidth;
QEXTERN pixel_t			*d_viewbuffer;
QEXTERN short			*d_pzbuffer;
QEXTERN unsigned int	d_zrowbytes;
QEXTERN unsigned int	d_zwidth;

#endif	// !id386

