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

#ifndef _GENERIC_TYPES_H_
#define _GENERIC_TYPES_H_

#include "Generic.h"

#include <intuition/classusr.h>

/** \file Types.h
 * \brief This file defines types for all four architectures.\n
 * Carries all the type specifications, array macros and other defines.
 */

#if defined(__AROS__)
#include <aros/cpu.h>
#endif

/*
 * default common types & platform specific includes */
#ifdef AROS_64BIT_TYPE
typedef unsigned AROS_64BIT_TYPE    uint64;
typedef signed AROS_64BIT_TYPE      int64;
#else
typedef unsigned long long uint64;  /**< @brief unsigned 64bit integer */
typedef signed long long   int64;   /**< @brief signed 64bit integer */
#endif
#ifdef AROS_32BIT_TYPE
typedef unsigned AROS_32BIT_TYPE    uint32;
typedef signed AROS_32BIT_TYPE      int32;
#else
typedef unsigned long int  uint32;  /**< @brief unsigned 32bit integer */
typedef signed long int    int32;   /**< @brief signed 32bit integer */
#endif
typedef unsigned short     uint16;  /**< @brief unsigned 16bit integer */
typedef unsigned char      uint8;   /**< @brief unsigned 8bit integer  */
typedef signed short       int16;   /**< @brief signed 16bit integer */
typedef signed char        int8;    /**< @brief signed 8bit integer  */

typedef signed int        sint;    /**< @brief architecture specific signed int */
#if !defined(__posixc_misctypes_defined)
typedef unsigned int      uint;    /**< @brief architecture specific unsigned int */
#endif

#if defined(__AROS__)
typedef long unsigned int size_t;
#elif defined(__mc68000)
typedef long unsigned int size_t;
typedef long IPTR;
#elif defined(__AMIGAOS4__)
typedef unsigned int size_t;
typedef long IPTR;
#elif defined(__MORPHOS__)
typedef unsigned int size_t;
#else
#error no size_t defined
#endif

/**< @brief architecture specific signed int, large enough to hold a pointer: sizeof(s_int) = sizeof(void*) */
/**< @brief architecture specific unsigned int, large enough to hold a pointer: sizeof(u_int) = sizeof(void*) */
#ifdef AROS_INTPTR_TYPE
typedef signed AROS_INTPTR_TYPE     siptr;
typedef unsigned AROS_INTPTR_TYPE   iptr;
#else
typedef signed long        siptr;    
typedef unsigned long      iptr;
#endif

#define PACKED __attribute__((packed))

/**
 * \enum TriState 
 * \brief Suggested enum for all three-state functions that accept or return
 * true, false or unknown states.
 */
enum TriState
{
   stUnknown   = -1,    /**< Unknown state */
   stNo        = 0,     /**< No = False    */
   stFalse     = 0,     /**< No = False    */
   stYes       = 1,     /**< Yes = True    */
   stTrue      = 1      /**< Yes = True    */
};


#ifdef __cplusplus
#include <utility/tagitem.h>

#include <cstdint>
#include <utility>
/* Deliberately not <array>: it drags in libc++'s <atomic> and threading
   support, which needs POSIX functions that -noposixc builds do not have.
   A plain array serves this helper just as well.  */

//! Safer, cross-platform tag array initializers

template <std::size_t N>
struct ArrayImpl
{
    template <typename... Ts>
    constexpr ArrayImpl(Ts&&... vs)
        : array_{ (SIPTR)std::forward<Ts>(vs)... }
    {}

    operator SIPTR() const
    {
        return (SIPTR)array_;
    }

    operator TagItem*() const
    {
        return (TagItem*)(char*)array_;
    }

    operator Msg() const
    {
        return (Msg)(char*)array_;
    }

    operator APTR() const
    {
        return (APTR)(char*)array_;
    }

private:
    SIPTR array_[N];
};

//! Factory function – call this as ARRAY(...)
template <typename... Ts>
ArrayImpl<sizeof...(Ts)> ARRAY(Ts&&... vs)
{
    return { std::forward<Ts>(vs)... };
}

template <size_t N>
struct SizeArrayImpl
{
    template <typename... Ts>
    SizeArrayImpl(Ts&&... vs)
        : array_{ (SIPTR)N, (SIPTR)std::forward<Ts>(vs)... }
    {}

    operator IPTR () {
        return (IPTR)&array_[1];
    }

    operator TagItem* () {
        return (TagItem *)(char*)&array_[1];
    }

    operator Msg() const
    {
        return (Msg)(char*)&array_[1];
    }

    operator APTR () {
        return (APTR)(char*)&array_[1];
    }

private:
    /* element 0 holds the count, the values follow.  The previous builder
       dropped its first argument and then read one element past the end of
       its temporary; initialising directly avoids both.  */
    SIPTR array_[N + 1];
};

//! Factory function – call this as SIZEARRAY(...)
template <typename... Ts>
SizeArrayImpl<sizeof...(Ts)> SIZEARRAY(Ts&&... vs)
{
    return SizeArrayImpl<sizeof...(Ts)>(std::forward<Ts>(vs)...);
}

#endif

/**
 * \def OFFSET(type, field)
 * \brief this macro calculates offset of field within structure.
 */
#define OFFSETOF(type, field) \
   ((size_t)(&((type*)1)->field)-1)

/**
 * \def OFFSETWITH(type, field)
 * \brief this macro calculates offset of first element after the selected field
 */
#define OFFSETWITH(type, field) \
   (((size_t)(&((type*)1)->field))+sizeof(type::field)-1)

/**
 * \def SWAP_WORD(data)
 * \brief switches endian of a 16-bit word
 */
#define SWAP_WORD(data) (((data & 0xff) << 8) | ((data >> 8) & 0xff))

/**
 * \def SWAP_LONG(data)
 * \brief switches endian of a 32-bit word
 */
#define SWAP_LONG(data) ({ register uint32 t = (((data & 0xffff) << 16) | ((data >> 16) &0xffff)); \
                           t = ((t >> 8) & 0xff00ff) | ((t & 0xff00ff) << 8); t; })

#if !defined(__AROS__) || AROS_BIG_ENDIAN

#define ENDIAN BIG   /**< @brief Defines endianess for current architecture. Can be either @b BIG or @b LITTLE */

/** 
 * @brief Way to change any word into BigEndian 
 * or change any BigEndian word to current arch. 
 */
#define W2BE(x) (x)
/** 
 * @brief Way to change any word into LittleEndian 
 * or change andy LittleEndian word to current arch.
 */
#define W2LE(x) ((((x)>>8)&255) | (((x)&255)<<8))
/** 
 * @brief Way to change any long into BigEndian 
 * or change any BigEndian long to current arch. 
 */
#define L2BE(x) (x)
/** 
 * @brief Way to change any long into LittleEndian 
 * or change andy LittleEndian long to current arch.
 */
#define L2LE(x) ((((x)>>24)&255) | (((x)>>8)&0xff00) | (((x)<<8)&0xff0000) | (((x)<<24)&0xff000000))

#else /* defined(__AROS__) && !AROS_BIG_ENDIAN */

#define ENDIAN LITTLE
#define W2LE(x)   (x)
#define W2BE(x)   ((((x)>>8)&255) | (((x)&255)<<8))
#define L2LE(x)   (x)
#define L2BE(x)   ((((x)>>24)&255) | (((x)>>8)&0xff00) | (((x)<<8)&0xff0000) | (((x)<<24)&0xff000000))

#endif /* !defined(__AROS__) || AROS_BIG_ENDIAN */

#endif

