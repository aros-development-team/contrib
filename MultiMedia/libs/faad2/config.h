/*
    Hand-crafted config.h for building libfaad on AROS, replacing the
    autoconf-generated one.  Enabled features match what faad2 2.7's
    configure detects on a modern C99 toolchain/libc.
*/

#ifndef FAAD2_AROS_CONFIG_H
#define FAAD2_AROS_CONFIG_H

#define STDC_HEADERS    1
#define HAVE_STDLIB_H   1
#define HAVE_STRING_H   1
#define HAVE_STDINT_H   1
#define HAVE_INTTYPES_H 1
#define HAVE_MEMCPY     1
#define HAVE_STRCHR     1
#define HAVE_LRINTF     1

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define WORDS_BIGENDIAN 1
#endif

/* syntax.c emits LATM diagnostics via fprintf(stderr, ...), which would
   drag posixc's stdio into what is otherwise a pure stdc link; silence
   them (the affected LATM configurations simply return unsupported). */
#ifdef __AROS__
#include <stdio.h>
#define fprintf(...) ((void)0)
#endif

#endif /* FAAD2_AROS_CONFIG_H */
