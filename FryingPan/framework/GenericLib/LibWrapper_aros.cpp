/*
 * Amiga Generic Set - set of libraries and includes to ease sw development for all Amiga platforms
 * Copyright (C) 2001-2011 Tomasz Wiszkowski Tomasz.Wiszkowski at gmail.com.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "Library.h"

extern "C" {
#define __NOLIBBASE__
#include <exec/resident.h>
#include <proto/exec.h>
}

#include "LibWrapper_aros.h"

static int LibLibrary_Init(struct Library *pOurBase)
{
    if (true == Lib_SetUp())
        return TRUE;
    return FALSE;
}

static int LibLibrary_Expunge(struct Library *pOurBase)
{
    Lib_CleanUp();
    return TRUE;
}

static int LibLibrary_Open(struct Library *pOurBase)
{
    if (false == Lib_Acquire())
        return FALSE;
    return TRUE;
}

static int LibLibrary_Close(struct Library *pOurBase)
{
    Lib_Release();
    return TRUE;
}

ADD2INITLIB(LibLibrary_Init, 0);
ADD2OPENLIB(LibLibrary_Open, 0);
ADD2CLOSELIB(LibLibrary_Close, 0);
ADD2EXPUNGELIB(LibLibrary_Expunge, 0);
