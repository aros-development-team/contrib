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

#ifndef _LIBWRAPPER_H_
#define _LIBWRAPPER_H_

#include <exec/libraries.h>
#include <Generic/Types.h>

#include "LibWrapper_libdefs.h"

struct LibInitStruct
{
   unsigned long  LibSize;
   void          *FuncTable;
   void          *DataTable;
   void           (*InitFunc)(void);
};


#ifdef __cplusplus
extern "C"
{
#endif 

    extern void AROS_SLIB_ENTRY(GM_UNIQUENAME(OpenLib),LibWrapper,1)(void);
    extern void AROS_SLIB_ENTRY(GM_UNIQUENAME(CloseLib),LibWrapper,2)(void);
    extern void AROS_SLIB_ENTRY(GM_UNIQUENAME(ExpungeLib),LibWrapper,3)(void);
    extern void AROS_SLIB_ENTRY(GM_UNIQUENAME(ExtFuncLib),LibWrapper,4)(void);
    extern const struct LibInitStruct GM_UNIQUENAME(InitTable);
#ifdef __cplusplus
};
#endif

#define LIBRARY(name, id, version)                                              \
extern const char GM_UNIQUENAME(LibName)[];                                  \
extern const char GM_UNIQUENAME(LibID)[];                                    \
extern const char GM_UNIQUENAME(Copyright)[];                                \
                                                                            \
static struct Resident GM_UNIQUENAME(ROMTag) __attribute__((used)) =                                \
{                                                                            \
  RTC_MATCHWORD,                                                            \
  &GM_UNIQUENAME(ROMTag),                                                   \
  (APTR)(IPTR)&GM_UNIQUENAME(ROMTag)+1,                                     \
  RESIDENTFLAGS,                                                            \
  version,                                                                  \
  NT_LIBRARY,                                                               \
  0,                                                                        \
  (CONST_STRPTR)&GM_UNIQUENAME(LibName)[0],                                 \
  (CONST_STRPTR)&GM_UNIQUENAME(LibID)[6],                                   \
  (APTR)(IPTR)&GM_UNIQUENAME(InitTable),                                    \
};                                                   \
__section(".text.romtag") const char GM_UNIQUENAME(LibName)[] = name;        \
__section(".text.romtag") const char GM_UNIQUENAME(LibID)[] = id;            \
static int GM_UNIQUENAME(Version) __attribute__((used)) = (version);        \
static int GM_UNIQUENAME(Revision) __attribute__((used)) = (0);

#define LIB_FT_Begin                                                            \
static APTR GM_UNIQUENAME(FuncTable)[] __attribute__((used)) =                                      \
{                                                                            \
  (APTR)(IPTR)&AROS_SLIB_ENTRY(GM_UNIQUENAME(OpenLib),LibWrapper,1),        \
  (APTR)(IPTR)&AROS_SLIB_ENTRY(GM_UNIQUENAME(CloseLib),LibWrapper,2),       \
  (APTR)(IPTR)&AROS_SLIB_ENTRY(GM_UNIQUENAME(ExpungeLib),LibWrapper,3),     \
  (APTR)(IPTR)&AROS_SLIB_ENTRY(GM_UNIQUENAME(ExtFuncLib),LibWrapper,4), 

#define LIB_FT_Function(f) (APTR)(IPTR)(&f),

#define LIB_FT_End                                                              \
  (APTR)-1,                                                                 \
};                                                    \
static int GM_UNIQUENAME(FuncCount) __attribute__((used)) = (sizeof(GM_UNIQUENAME(FuncTable)) / sizeof(APTR));

#define GATE0(type, name)                                                       \
   type name();

#define GATE1(type, name, type1, reg1)                                          \
   type name(type1);

#define GATE2(type, name, type1, reg1, type2, reg2)                             \
   type name(type1, type2);

#define GATE3(type, name, type1, reg1, type2, reg2, type3, reg3)                \
   type name(type1, type2, type3);

#define GATE4(type, name, type1, reg1, type2, reg2, type3, reg3, type4, reg4)   \
   type name(type1, type2, type3, type4);

#define GATE5(type, name, type1, reg1, type2, reg2, type3, reg3, type4, reg4, type5, reg5) \
   type name(type1, type2, type3, type4, type5);




#endif //_LIBWRAPPER_H_
