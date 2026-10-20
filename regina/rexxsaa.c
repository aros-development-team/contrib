/*
 *  The Regina Rexx Interpreter
 *  Copyright (C) 1993-1994  Anders Christensen <anders@pvv.unit.no>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * This file implements the client part of the SAA API when Regina
 * is linked into a program using SAA API. There is one routine for
 * each of the functions in SAA API, and the functionality is partly
 * implemented here, and partly by calling subroutines in Regina.
 * Note that the interface to Regina is as simple as possible, so that
 * a multitude of different transport mechanisms can be used (although
 * normal linking is probably the most common.
 *
 * The following SAA API functions is defined in this source file:
 *
 *    RexxStart()               --- execute Rexx code
 *    RexxRegisterSubcomExe()   --- register subcommand handler
 *    RexxRegisterSubcomDll()   --- ditto (from dynamic library)
 *    RexxQuerySubcom()         --- query subcommand handler
 *    RexxDeregisterSubcom()    --- deregister subcommand handler
 *    RexxVariablePool()        --- handle Rexx variable manipulation
 *    RexxRegisterExitExe()     --- register exit handler
 *    RexxRegisterExitDll()     --- ditto (from dynamic library)
 *    RexxDeregisterExit()      --- deregister exit handler
 *    RexxQueryExit()           --- query exit handler
 *    RexxRegisterFunctionExe() --- register external function handler
 *    RexxRegisterFunctionDll() --- ditto (from dynamic library)
 *    RexxQueryFunction()       --- query external function
 *    RexxDeregisterFunction()  --- deregister external function
 *    RexxSetHalt()             --- set Halt and Trace
 *    RexxCreateQueue()         --- create named queued
 *    RexxDeleteQueue()         --- delete named queued
 *    RexxQueryQueue()          --- query named queued
 *    RexxAddQueue()            --- add line to named queued
 *    RexxPullQueue()           --- pull line from named queued
 *    RexxAddMacro()            --- add a macro to macrospace
 *    RexxClearMacroSpace()     --- remove all macros from macrospace
 *    RexxDropMacro()           --- remove macro from macrospace
 *    RexxLoadMacroSpace()      --- load macrospace macros from file
 *    RexxQueryMacro()          --- find a macro's search order
 *    RexxReorderMacro()        --- change the search order for a macro
 *    RexxSaveMacroSpace()      --- save macrospace to file
 *
 * These functions are Regina extensions
 *    RexxFreeMemory()          --- free memory allocated by Rexx API
 *    RexxAllocateMemory()      --- allocate memory to be freed by Rexx API
 *    RexxCallBack()            --- execute an internal procedure within the running script
 *    ReginaVersion()           --- version information
 *    ReginaCleanup()           --- generic cleanup routine
 *
 * These functions are all defined in the doc for SAA API. In addition,
 * a number of calls in Regina are called, as well as a number of calls
 * are defined for use by Regina. These all start with the prefix Ifc.
 * First the one defined in rexxsaa.c, which can be called from other
 * parts of Regina:
 *
 *    IfcSubCmd()               --- invoke a subcommand
 *    IfcDoExit()               --- invoke a system exit handler
 *    IfcExecFunc()             --- invoke an external function handler
 *    IfcExecFuncDll()          --- invoke an external function handler in a DLL
 *
 * Then the functions which are defined elsewhere, which can be called
 * by this source code:
 *
 *    IfcExecScript()           --- start to execute Rexx code
 *    IfcExecCallBack()         --- start to execute Rexx procedure
 *    IfcVarPool()              --- handle a variable manipulation request
 *    IfcRegFunc()              --- register an external function name
 *    IfcDelFunc()              --- deregister an external function name
 *    IfcQueryFunc()            --- queries an external function name
 *    IfcAllocateMemory()       --- allocate memory for API user
 *    IfcFreeMemory()           --- free memory from API user
 *    IfcDeleteQueue()          --- delete queue
 *    IfcAddQueue()             --- add a line to the queue
 *    IfcPullQueue()            --- pull a line off the queue
 *
 * All these routines are properly defined in the documentation for
 * Regina. Other than the functions listed, the code in this file has
 * been isolated as far as possible, and no functions specific to
 * Regina is used, not even for memory allocation.
 */

/*
 * We need to define these symbols in order to get the proper macros,
 * datatypes, and declaration when including rexxsaa.h.
 */
#define INCL_RXSHV
#define INCL_RXSUBCOM
#define INCL_RXFUNC
#define INCL_RXSYSEXIT
#define INCL_RXARI
#define INCL_RXQUEUE
#define INCL_RXMACRO

#include "regina_c.h"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/*
 * The rexxsaa.h header file defines the interface between this file and
 * the client program which uses SAA API. The rxiface.h header file
 * defines the interface between this file and Regina.
 */
#include "configur.h"
/*
 * The following #define __REGINA_INTERNAL stops an error with MingW32
 *
 */
#define __REGINA_INTERNAL
#include "rexxsaa.h"
#include "defs.h"
#define DONT_TYPEDEF_PFN
#ifndef RXLIB
#define RXLIB
#endif
#include "rexx.h"
#if defined(DYNAMIC) && defined(HAVE_GCI)
# include "gci/gci.h"
#endif
#include "rxiface.h"
#include "extstack.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_ASSERT_H
# include <assert.h>
#endif
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <setjmp.h>

#if defined(__EPOC32__) || defined(__WINS__)
# ifdef APIRET
#  undef APIRET
# endif
# define APIRET unsigned long
# ifdef APIENTRY
#  undef APIENTRY
# endif
#else
# define EXPORT_C
#endif

typedef union {
   RXFNCCAL_PARM fnccal ;
   RXCMDHST_PARM cmdhst ;
   RXMSQPLL_PARM msqpll ;
   RXMSQPSH_PARM msqpsh ;
   RXMSQSIZ_PARM msqsiz ;
   RXMSQNAM_PARM msqnam ;
   RXSIOSAY_PARM siosay ;
   RXSIOTRC_PARM siotrc ;
   RXSIOTRD_PARM siotrd ;
   RXSIODTR_PARM siodtr ;
   RXHLTTST_PARM hlttst ;
   RXTRCTST_PARM trctst ;
   RXENVGET_PARM envget ;
   RXENVSET_PARM envset ;
   RXCWDGET_PARM cwdget ;
   RXCWDSET_PARM cwdset ;
} EXIT ;

/* The following value allows called programs to call "free" to the return
 * parameters without destroying our stack.
 */
#define ILLEGAL_USE_SIZE (8 * sizeof(void *))

typedef struct { /* rex_tsd: static variables of this module (thread-safe) */
   struct ExitHandlers *CurrentHandlers ;
} rex_tsd_t; /* thread-specific but only needed by this module. see
              * init_rexxsaa
              */

#define EXT_FUNCS_COUNT (sizeof(rt->saafuncs) / sizeof(rt->saafuncs[0]))

struct ExitHandlers
{
   RexxExitHandler *(Handlers[RXNOOFEXITS]) ; /* for RexxRegisterExitExe */
   struct ExitHandlers *prev ;
} ;

/*
 * The following RXMAP_TYPE() macro maps from the SAA API macros holding
 * the type of an invocation (function, subroutine or command), to its
 * equivalent value in the internal interface of Regina (as defined in
 * the file rxiface.h
 */
#define RXMAP_TYPE(a) ((a)==RXCOMMAND ? RX_TYPE_COMMAND : \
              (a)==RXFUNCTION ? RX_TYPE_FUNCTION : RX_TYPE_SUBROUTINE)


/* init_rexxsaa initializes the module.
 * Currently, we set up the thread specific data.
 * The function returns 1 on success, 0 if memory is short.
 */
int init_rexxsaa( tsd_t *TSD )
{
   rex_tsd_t *rt;

   if (TSD->rex_tsd != NULL)
      return(1);

   if ( ( TSD->rex_tsd = MallocTSD( sizeof(rex_tsd_t) ) ) == NULL )
      return(0);
   rt = (rex_tsd_t *)TSD->rex_tsd;
   memset( rt, 0, sizeof(rex_tsd_t) );  /* correct for all values */
   return(1);
}

/* deinit_rexxsaa deinitializes the module and frees used memory blocks not
 * allocated by the Malloc-Interface. There isn't anything to do currently.
 */
void deinit_rexxsaa( tsd_t *TSD )
{
}


/* StartupInterface initializes the Rexx system once per thread. Values
 * like __regina_get_tsd()->systeminfo are set. The true purpose of this
 * function is to create an environment which allows the run of the
 * interpreter. This is exactly the case when systeminfo exists. The last
 * systeminfo is hopefully never deleted but that won't do any harm if we
 * reinitialize the Rexx environment.
 *
 * There is a three-stage step to let a Rexx program run:
 * 1) Initialize the runtime system and programming environment. This is
 *    done by __regina_get_tsd() done in GLOBAL_ENTRY_POINT().
 *    After this call you may use Malloc() and friends: basic things.
 * 2) Initialize the Rexx system. This is done by
 *    setup_system() here or directly by any other caller of
 *    __regina_faked_main() or main():
 *    You are then allowed to access variables from the variable pool, load
 *    a Rexx program or just call a Rexx API function.
 *    Hint: The detection of this step is done as follows:
 *    Rexx_system_is_running = (__regina_get_tsd()->systeminfo != NULL);
 *    or a similar compare.
 * 3) Load the Rexx program in memory.
 *    Although you don't know, what program is currently loaded, you can
 *    check if a program is loaded (and running) by checking:
 *    Program_running = (__regina_get_tsd()->systeminfo->tree.root != NULL);
 *    This step is done in RexxStart or by main()/__regina_faked_main() when
 *    called as a program.
 * Never use __regina_get_tsd() when it is not needed, of course.
 * This function should be called as GLOBAL_ENTRY_POINT() at the very
 * start of the interpreter but after GLOBAL_ENTRY_POINT.
 */
static void StartupInterface( tsd_t *TSD )
{
   if (TSD->systeminfo != NULL)
      return;

   setup_system( TSD, 1 );
   signal_setup( TSD );
}


/*
 * FillReq prepares a String with a given Length to be exported to an
 * external application.
 * The FillReq() function takes as parameter a pointer to a VarPool()
 * request structure variable, and the definition of a string, and
 * fill the content of the string into the request block. Note that the
 * third parameter is gobbled up, so it can not be used or released by
 * the calling function afterwards. Also, there are two macros defined,
 * which gives a better access to the contents of the function
 */
#define FillReqName(a,b,c) FillReq(a,b,c,1)
#define FillReqValue(a,b,c) FillReq(a,b,c,0)

static void FillReq( PSHVBLOCK Req, ULONG Length, const char *String, int name )
{
   RXSTRING *string;
   ULONG *strlen;

   string = name ? &Req->shvname : &Req->shvvalue;
   strlen = name ? &Req->shvnamelen : &Req->shvvaluelen;
   /*
    * SAA DOCUMENTATION BREAKAGE:
    * As stated in README.08h the OS/2 REXX modifies shvvaluelen instead of
    * shvvalue.strlength (name in the same way). Some software out there
    * relies on this behaviour. Sigh.
    * Thus we assign the shv???len parts too, as ORexx does.
    */

   /*
    * If the string is undefined, set ->strptr to NULL. It is not required
    * that the lengths parameters are set to zero, but I'll do this as
    * nice gest to the users; someone is probably going to believe that
    * this is how the data is returned.
    * shvnamelen and shvvaluelen are read-only values describing the maximum
    * size of the destination buffer, but see above at SAA DOCUMENTATION.
    */
   if ( (LONG)Length == RX_NO_STRING )
   {
      MAKERXSTRING( *string, NULL, 0 );
      *strlen = 0;
      return;
   }

   /*
    * If a string was supplied, use it, else allocate sufficient space.
    * The then part of the if will just copy the data to the user-supplied
    * return string data area, noting a truncation is one occurred.
    */
   if ( RXSTRPTR( *string ) )
   {
      /*
       * We need a terminator, therefore we need one byte more for allocation.
       * We may come to the funny situation indicating a truncation but have
       * copied all bytes from the string.
       */
      if ( *strlen <= Length )
      {
          Req->shvret |= RXSHV_TRUNC;
          Length = *strlen;
      }
      else
      {
         string->strptr[Length] = '\0';
      }
      memcpy(string->strptr, String, Length );
      string->strlength = Length;
      /*
       * shvnamelen and shvvaluelen are read-only values describing the maximum
       * size of the destination buffer, but see above at SAA DOCUMENTATION.
       */
      *strlen = Length;
   }
   else
   {
      /*
       * The else part of the if will allocate new space for the data, and
       * fills in the data, or return a memory fault if data could not
       * properly be allocated.
       *
       * We have to ASCII0-terminate the string silently
       */
      string->strptr = (char *)IfcAllocateMemory( Length + 1 );
      if ( string->strptr )
      {
         if ( Length )
            memcpy( string->strptr, String, Length );
         string->strptr[Length] = '\0';
         string->strlength = Length;
      }
      else
      {
         Req->shvret |= RXSHV_MEMFL;
         /*
          * Set strlength for convenience.
          */
         string->strlength = 0;
         Length = 0;
      }
      /*
       * shvnamelen and shvvaluelen are read-only values describing the maximum
       * size of the destination buffer, but see above at SAA DOCUMENTATION.
       */
      *strlen = Length;
   }
}

/* ======================================================================== */

/* RetLen and RetStr should point to {0,NULL}. They will be filled with
 * freshly allocated values. A return value will always exist.
 */
int IfcSubCmd( tsd_t *TSD, int EnvLen, const char *EnvStr,
               int CmdLen, const char *CmdStr,
               int *RetLen, char **RetStr )
{
   RXSTRING Cmd, Ret ;
   char *OldResult= NULL ;
   USHORT Flags=0 ;
   char *Command ;
   char *EnvNam;
   struct entry_point *Envir;
   int rvalue=0, RCode=0, rc=RXEXIT_NOT_HANDLED ;
   char subcmd_result[ILLEGAL_USE_SIZE+RXAUTOBUFLEN] ;
   RXCMDHST_PARM cmdhst;
   PUCHAR parm=NULL;
   rex_tsd_t *rt;

   rt = (rex_tsd_t *)TSD->rex_tsd;

   Command = (char *)MallocTSD( CmdLen + 1);
   memcpy(Command,CmdStr,CmdLen);
   Command[CmdLen] = '\0';
   memset( subcmd_result, 0, sizeof( subcmd_result ) ) ;
   MAKERXSTRING( Cmd, Command, CmdLen ) ;
   MAKERXSTRING( Ret, subcmd_result + ILLEGAL_USE_SIZE, RXAUTOBUFLEN) ;
   OldResult = subcmd_result + ILLEGAL_USE_SIZE;
   /*
    * Terminate the command string with nul character
    */
   Envir = subcom_hook( TSD, EnvStr , EnvLen ) ;
   if ( rt->CurrentHandlers && rt->CurrentHandlers->Handlers[RXCMD] )
   {
      EnvNam = (char *)MallocTSD( EnvLen + 1 ) ;
      memcpy(EnvNam, EnvStr, EnvLen ) ;
      EnvNam[EnvLen] = '\0';
      cmdhst.rxcmd_flags.rxfcfail = 0;
      cmdhst.rxcmd_flags.rxfcerr = 0;
      cmdhst.rxcmd_command = Cmd ;
      cmdhst.rxcmd_address = (unsigned char *)EnvNam ;
      cmdhst.rxcmd_addressl = (USHORT) EnvLen ;
      cmdhst.rxcmd_retc = Ret;
      cmdhst.rxcmd_dll = NULL;
      cmdhst.rxcmd_dll_len = 0;
      if ( Envir != NULL )
      {
         if ( Envir->lib != NULL )
         {
            cmdhst.rxcmd_dll = (unsigned char*) Str_val( Envir->lib->name ) ;
            cmdhst.rxcmd_dll_len = Str_len( Envir->lib->name );
         }
      }
      parm = (PUCHAR)&cmdhst;
      rc = (*(rt->CurrentHandlers->Handlers[RXCMD]))(RXCMD, RXCMDHST, parm);
      TSD->var_indicator = 0;
      assert( rc==RXEXIT_HANDLED || rc==RXEXIT_NOT_HANDLED ||
              rc==RXEXIT_RAISE_ERROR ) ;
      if (cmdhst.rxcmd_flags.rxfcerr)
         RCode = RXFLAG_ERROR ;
      else if (cmdhst.rxcmd_flags.rxfcfail)
         RCode = RXFLAG_FAILURE ;
      else
         RCode = RXFLAG_OK;
      Ret = cmdhst.rxcmd_retc;
      FreeTSD( EnvNam ) ;
   }
   if (rc == RXEXIT_NOT_HANDLED)
   {
      if ( Envir )
      {
         RexxSubcomHandler *handler;
         handler = (RexxSubcomHandler *) Envir->addr;
         MAKERXSTRING( Cmd, Command, CmdLen ) ;
         if (Ret.strlength && OldResult != Ret.strptr) /* Ignore return values*/
            IfcFreeMemory( Ret.strptr ) ;
         MAKERXSTRING( Ret, subcmd_result + ILLEGAL_USE_SIZE, RXAUTOBUFLEN) ;
         OldResult = subcmd_result + ILLEGAL_USE_SIZE ;
         rvalue = handler( &Cmd, &Flags, &Ret ) ;
         (void)rvalue; // FIXME: Should this be tested?
         TSD->var_indicator = 0;
         if (Flags==RXSUBCOM_OK)
            RCode = RXFLAG_OK ;
         else if (Flags==RXSUBCOM_ERROR)
            RCode = RXFLAG_ERROR ;
         else if (Flags==RXSUBCOM_FAILURE)
            RCode = RXFLAG_FAILURE ;
         else
            exiterror( ERR_INTERPRETER_FAILURE, 1, __FILE__, __LINE__, "" )  ;
      }
      else
      {
         RCode = RXFLAG_NOTREG ;
         Ret.strlength = 0 ;
      }
   }

   if (Ret.strlength)
   {
      *RetLen = Ret.strlength ;
      *RetStr = (char *)MallocTSD( Ret.strlength ) ;
      memcpy( *RetStr, Ret.strptr, Ret.strlength ) ;
   }
   else
   {
      *RetLen = 1 ;
      *RetStr = (char *)MallocTSD( 1 ) ;
      (*RetStr)[0] = '0' ;
   }

   if (Ret.strlength && OldResult != Ret.strptr)
      IfcFreeMemory( Ret.strptr ) ;

   FreeTSD(Command);
   return RCode ;
}

/* IfcDoExit calls an exit handler with one of the following codes set in Code.
 * The arguments may either be input or output or nothing but not both.
 * Parameter      INIT TERMIN PULL TRCIN STDOUT STDERR GETENV PUTENV
 * ---------------------------------------------------------------------------------------
 * InputLength    NULL NULL   set  set   NULL   NULL   set    NULL
 * InputString    NULL NULL   set  set   NULL   NULL   set    NULL
 * OutputLength1  0    0      0    0     set    set    set    set
 * OutputString1  NULL NULL   NULL NULL  set    set    set    set
 * OutputLength2  0    0      0    0     0      0      0      set
 * OutputString2  NULL NULL   NULL NULL  NULL   NULL   NULL   set
 *
 * Notes:
 * 1) An output string should always be a fresh copy. Although it is not
 *    allowed the user program may destroy the contents.
 *    For this reason OutputString is not declared as const.
 *    OutputString should be 0-terminated (0 not counted in OutputLength).
 * 2) An input string is normally NOT required. Just provide a position
 *    where to place the input to. Example:
 *    char *in = NULL;
 *    int inlen = 0;
 *    IfcDoExit(?,?,0,NULL,0,NULL,&inlen,&in);
 *    If the caller of this function provides a valid input string it is
 *    ignored on exit. This function always returns back a freshly allocated
 *    string in InputString (an empty string in case of errors).
 * 3) The user may change or overwrite the outcome of an exit like the return
 *    values to functions. A user-allocated string will be freed.
 */
int IfcDoExit( tsd_t *TSD, int Code,
               int OutputLength1, char *OutputString1,
               int OutputLength2, char *OutputString2,
               int *InputLength, char **InputString )
{
   int rc=0;
   LONG SubCode=0, MainCode=0 ;
   ULONG retlen=0;
   char *retstr=NULL;
   char *mustFree;
   RXSIOSAY_PARM siosay;
   RXSIOTRD_PARM siotrd;
   RXSIODTR_PARM siodtr;
   RXENVSET_PARM envset;
   RXENVGET_PARM envget;
   RXCWDSET_PARM cwdset;
   RXCWDGET_PARM cwdget;
   PEXIT parm=NULL;
   rex_tsd_t *rt;

   rt = (rex_tsd_t *)TSD->rex_tsd;

   MAKERXSTRING( siodtr.rxsiodtr_retc, NULL, 0) ; /* Make compiler happy */
   MAKERXSTRING( siotrd.rxsiotrd_retc, NULL, 0) ; /* Make compiler happy */
   MAKERXSTRING( envget.rxenv_value, NULL, 0) ; /* Make compiler happy */
   MAKERXSTRING( cwdget.rxcwd_value, NULL, 0) ; /* Make compiler happy */

   switch (Code)
   {
      case RX_EXIT_STDERR:
      case RX_EXIT_STDOUT:
         assert(InputLength == NULL &&
                InputString == NULL &&
                OutputLength2 == 0 &&
                OutputString2 == NULL &&
                OutputLength2 == 0 &&
                OutputString2 == NULL);
         siosay.rxsio_string.strptr = OutputString1 ;
         siosay.rxsio_string.strlength = OutputLength1 ;
         parm = (PEXIT)&siosay;
         SubCode = (Code==RX_EXIT_STDOUT) ? RXSIOSAY : RXSIOTRC ;
         MainCode = RXSIO ;

         break ;

      case RX_EXIT_TRCIN:
         assert(OutputLength1 == 0 &&
                OutputString1 == NULL &&
                InputLength != NULL &&
                InputString != NULL &&
                OutputLength2 == 0 &&
                OutputString2 == NULL);
         siodtr.rxsiodtr_retc.strlength = *InputLength ;
         siodtr.rxsiodtr_retc.strptr = *InputString ;
         parm = (PEXIT)&siodtr;
         SubCode = RXSIODTR ;
         MainCode = RXSIO ;
         break ;

      case RX_EXIT_PULL:
         assert(OutputLength1 == 0 &&
                OutputString1 == NULL &&
                InputLength != NULL &&
                InputString != NULL &&
                OutputLength2 == 0 &&
                OutputString2 == NULL);
         siotrd.rxsiotrd_retc.strlength = *InputLength ;
         siotrd.rxsiotrd_retc.strptr = *InputString ;
         parm = (PEXIT)&siotrd;
         SubCode = RXSIOTRD ;
         MainCode = RXSIO ;
         break ;

      case RX_EXIT_INIT:
         assert(OutputLength1 == 0 &&
                OutputString1 == NULL &&
                InputLength == NULL &&
                InputString == NULL &&
                OutputLength2 == 0 &&
                OutputString2 == NULL);
         MainCode = RXINI ;
         SubCode = RXINIEXT ;
         break ;

      case RX_EXIT_TERMIN:
         assert(OutputLength1 == 0 &&
                OutputString1 == NULL &&
                InputLength == NULL &&
                InputString == NULL &&
                OutputLength2 == 0 &&
                OutputString2 == NULL);
         MainCode = RXTER ;
         SubCode = RXTEREXT ;
         break ;

      case RX_EXIT_SETENV:
         assert(InputLength == NULL &&
                InputString == NULL &&
                OutputLength1 != 0 &&
                OutputString1 != NULL &&
                OutputLength2 != 0 &&
                OutputString2 != NULL);
         envset.rxenv_name.strptr = OutputString1 ;
         envset.rxenv_name.strlength = OutputLength1 ;
         envset.rxenv_value.strptr = OutputString2 ;
         envset.rxenv_value.strlength = OutputLength2 ;
         parm = (PEXIT)&envset;
         MainCode = RXENV ;
         SubCode = RXENVSET ;
         break ;

      case RX_EXIT_GETENV:
         assert(OutputLength1 != 0 &&
                OutputString1 != NULL &&
                InputLength != NULL &&
                InputString != NULL &&
                OutputLength2 == 0 &&
                OutputString2 == NULL);
         envget.rxenv_value.strlength = *InputLength ;
         envget.rxenv_value.strptr = *InputString ;
         envget.rxenv_name.strptr = OutputString1 ;
         envget.rxenv_name.strlength = OutputLength1 ;
         parm = (PEXIT)&envget;
         SubCode = RXENVGET ;
         MainCode = RXENV ;
         break ;

      case RX_EXIT_SETCWD:
         assert(InputLength == NULL &&
                InputString == NULL &&
                OutputLength1 != 0 &&
                OutputString1 != NULL);
         cwdset.rxcwd_value.strptr = OutputString1 ;
         cwdset.rxcwd_value.strlength = OutputLength1 ;
         parm = (PEXIT)&cwdset;
         MainCode = RXENV ;
         SubCode = RXCWDSET ;
         break ;

      case RX_EXIT_GETCWD:
         assert(OutputLength1 == 0 &&
                OutputString1 == NULL &&
                InputLength != NULL &&
                InputString != NULL &&
                OutputLength2 == 0 &&
                OutputString2 == NULL);
         cwdget.rxcwd_value.strlength = *InputLength ;
         cwdget.rxcwd_value.strptr = *InputString ;
         parm = (PEXIT)&cwdget;
         SubCode = RXCWDGET ;
         MainCode = RXENV ;
         break ;

      default:
         exiterror( ERR_INTERPRETER_FAILURE, 1, __FILE__, __LINE__, "" )  ;
         break;
   }

   assert( rt->CurrentHandlers->Handlers[MainCode] ) ;

   rc = (*(rt->CurrentHandlers->Handlers[MainCode]))(MainCode, SubCode, parm);
   TSD->var_indicator = 0;
   assert( rc==RXEXIT_HANDLED || rc==RXEXIT_NOT_HANDLED ||
           rc==RXEXIT_RAISE_ERROR ) ;

   mustFree = NULL;
   switch (Code)
   {
      case RX_EXIT_STDERR:
      case RX_EXIT_STDOUT:
      case RX_EXIT_INIT:
      case RX_EXIT_TERMIN:
      case RX_EXIT_SETENV:
      case RX_EXIT_SETCWD:
         break ;

     case RX_EXIT_TRCIN:
         retlen = siodtr.rxsiodtr_retc.strlength ;
         retstr = siodtr.rxsiodtr_retc.strptr ;
         mustFree = ( retstr != *InputString ) ? retstr : NULL;
         break ;

     case RX_EXIT_PULL:
         retlen = siotrd.rxsiotrd_retc.strlength ;
         retstr = siotrd.rxsiotrd_retc.strptr ;
         mustFree = ( retstr != *InputString ) ? retstr : NULL;
         break ;

     case RX_EXIT_GETENV:
         retlen = envget.rxenv_value.strlength ;
         retstr = envget.rxenv_value.strptr ;
         mustFree = ( retstr != *InputString ) ? retstr : NULL;
         break ;

     case RX_EXIT_GETCWD:
         retlen = cwdget.rxcwd_value.strlength ;
         retstr = cwdget.rxcwd_value.strptr ;
         mustFree = ( retstr != *InputString ) ? retstr : NULL;
         break ;

      default:
         exiterror( ERR_INTERPRETER_FAILURE, 1, __FILE__, __LINE__, "" )  ;
   }

   if (rc==RXEXIT_HANDLED)
      rc = RX_HOOK_NOPE ;
   else if (rc==RXEXIT_NOT_HANDLED)
      rc = RX_HOOK_GO_ON ;
   else if (rc==RXEXIT_RAISE_ERROR)
      rc = RX_HOOK_ERROR ;

   if (InputLength != NULL) /* retlen and retstr forms a return string. */
   {
      if ((retlen == 0) || (retstr == NULL))
      {
         retlen = 0;
         retstr = "";
      }

      /* Make a fresh copy, the user may change the value very fast. */
      *InputString = (char *)MallocTSD( (retlen < 1) ? 1 : retlen );
      memcpy(*InputString, retstr, retlen);
      *InputLength = retlen;
   }

   if ( mustFree )
      IfcFreeMemory( mustFree );
   return rc ;
}

/* ================================================================ */
/* ================ general purpose API functions ================= */

/* You are not allowed to use TSD or __regina_get_tsd() here! */
/*****************************************************************************

    NAME
        APIRET RexxFreeMemory()

    SYNOPSIS
        PVOID MemoryBlock );

    FUNCTION
        Releases memory allocated by the Regina API allocator.

    INPUTS
        MemoryBlock - Pointer returned by RexxAllocateMemory() or by a Regina
                      interface that allocates through the same allocator.

    RESULT
        The allocator release status. A null pointer produces RXFUNC_BADTYPE.

    NOTES
        Do not release a buffer twice or use it after release.

    SEE ALSO
        RexxAllocateMemory(), ReginaVersion()

*****************************************************************************/

EXPORT_C APIRET APIENTRY RexxFreeMemory(PVOID MemoryBlock )
{
   if (!MemoryBlock)
      return(RXFUNC_BADTYPE);

   return IfcFreeMemory( MemoryBlock );
}

/* You are not allowed to use TSD or __regina_get_tsd() here! */
/*****************************************************************************

    NAME
        PVOID RexxAllocateMemory()

    SYNOPSIS
        ULONG size );

    FUNCTION
        Allocates a buffer using the Regina API allocator.

    INPUTS
        size - Requested allocation size in bytes.

    RESULT
        Pointer to the allocation, or NULL when allocation fails.

    NOTES
        Release the buffer using RexxFreeMemory().

    SEE ALSO
        RexxFreeMemory()

*****************************************************************************/

EXPORT_C PVOID APIENTRY RexxAllocateMemory(ULONG size )
{
   return IfcAllocateMemory( size );
}

/* ================================================================ */
/* ================ in order to start Rexx scripts ================ */

/*****************************************************************************

    NAME
        APIRET RexxStart()

    SYNOPSIS
        LONG ArgCount,
        PRXSTRING ArgList,
        PCSZ ProgramName,
        PRXSTRING Instore,
        PCSZ EnvName,
        LONG CallType,
        PRXSYSEXIT Exits,
        PSHORT ReturnCode,
        PRXSTRING Result );

    FUNCTION
        Starts a Rexx program from a file, source buffer, or tokenized in-
        storage image.

    INPUTS
        ArgCount, ArgList - Argument count and RXSTRING array.
        ProgramName - Program identity or filename; must not be NULL.
        Instore - Optional source/tokenized image pair.
        EnvName - Optional initial command environment.
        CallType - RXCOMMAND, RXSUBROUTINE, or RXFUNCTION, optionally combined
                   with RXRESTRICTED.
        Exits - Optional RXENDLST-terminated system-exit array.
        ReturnCode - Optional program numeric result.
        Result - Optional RXSTRING receiving the textual result.

    RESULT
        API status of the invocation; the program return code is reported
        separately through ReturnCode. Invalid parameters produce
        RXFUNC_BADTYPE or RX_START_BADP. Too many arguments produce
        RX_START_TOOMANYP.

    NOTES
        RXCOMMAND accepts at most one argument string. Result storage depends
        on the supplied RXSTRING buffer and interpreter output.

    SEE ALSO
        RexxCallBack(), RexxRegisterExitExe(), RexxFreeMemory()

*****************************************************************************/

EXPORT_C APIRET APIENTRY RexxStart(LONG       ArgCount,
                          PRXSTRING  ArgList,
                          PCSZ       ProgName,
                          PRXSTRING  Instore,
                          PCSZ       EnvName,
                          LONG       CallType,
                          PRXSYSEXIT Exits,
                          PSHORT     ReturnCode,
                          PRXSTRING  Result )
{
   int cnt, RLength;
   char *RString;
   int ParLengths[MAX_ARGS_TO_REXXSTART];
   const char *ParStrings[MAX_ARGS_TO_REXXSTART];
   int ExitFlags;
   int EnvNamLen;
   const char *EnvNamStr;
   int WhereCode,rc;
   const char *SourcePtr;
   const void *TinPtr;
   unsigned long SourceLen,TinLen;
   struct ExitHandlers *Handlers;
   RexxExitHandler *handler;
   struct entry_point *EnvPtr;
   unsigned long instore_length;
   void *instore_buf;
   PCSZ ProgramName=ProgName;
   tsd_t *TSD;
   rex_tsd_t *rt;
   int restricted = 0;

   TSD = GLOBAL_ENTRY_POINT();
   rt = (rex_tsd_t *)TSD->rex_tsd;
   StartupInterface( TSD );

   if ( ( ArgCount < 0 ) || ( ( ArgCount > 0 ) && ( ArgList == NULL ) ) )
      return RXFUNC_BADTYPE;
   if ( !ProgName )
      return RXFUNC_BADTYPE;
   /*
    * Check if running in restricted mode first.
    */
   if ( CallType & RXRESTRICTED )
   {
      restricted = 1;
      CallType -= RXRESTRICTED;
   }
   if ( ( CallType != RXCOMMAND ) &&
        ( CallType != RXSUBROUTINE ) &&
        ( CallType != RXFUNCTION ) )
      return RXFUNC_BADTYPE;
   if ( ( CallType == RXCOMMAND ) && ( ArgCount > 1 ) )
      return RX_START_TOOMANYP;
   if ( ArgCount > (int) ( sizeof( ParLengths ) / sizeof( ParLengths[0] ) ) )
      return RX_START_TOOMANYP;

   if ( Instore )
   {
      if ( Instore[1].strptr && ( Instore[1].strlength < 1 ) )
         return RX_START_BADP;
   }

   for ( cnt = 0; cnt < ArgCount; cnt++ )
   {
      ParLengths[cnt] = ArgList[cnt].strlength;
      ParStrings[cnt] = ArgList[cnt].strptr;
      if ( ParStrings[cnt] == NULL )
         ParLengths[cnt] = RX_NO_STRING;
   }
   if ( Result != NULL )
   {
      RLength = (int) RXSTRLEN( *Result );
      if ( ( RString = RXSTRPTR( *Result ) ) == NULL )
         RLength = RX_NO_STRING;
   }
   else
   {
      RString = NULL;
      RLength = RX_NO_STRING;
   }

   Handlers = (struct ExitHandlers *)TSD->MTMalloc( TSD, sizeof( struct ExitHandlers ) );
   Handlers->prev = rt->CurrentHandlers;
   rt->CurrentHandlers = Handlers;
   for ( cnt = 0; cnt < RXNOOFEXITS; cnt++ )
      rt->CurrentHandlers->Handlers[cnt] = NULL;

   ExitFlags = 0;
   for ( cnt = 0; Exits && ( Exits->sysexit_code != RXENDLST ); Exits++ )
   {
      if ( ( Exits->sysexit_name == NULL )
      ||   ( strlen( Exits->sysexit_name ) == 0 ) )
         return RX_START_BADP;

      EnvPtr = exit_hook( TSD, Exits->sysexit_name,
                          strlen( Exits->sysexit_name ) );
      if ( !EnvPtr )
         continue;

      handler = (RexxExitHandler *) EnvPtr->addr;
      switch ( Exits->sysexit_code )
      {
         case RXSIO:
            ExitFlags |= ( 1 << RX_EXIT_STDOUT ) | ( 1 << RX_EXIT_STDERR ) |
                         ( 1 << RX_EXIT_TRCIN ) |  ( 1 << RX_EXIT_PULL );
            rt->CurrentHandlers->Handlers[RXSIO] = handler;
            break ;

         case RXINI:
            ExitFlags |= 1 << RX_EXIT_INIT;
            rt->CurrentHandlers->Handlers[RXINI] = handler;
            break ;

         case RXTER:
            ExitFlags |= 1 << RX_EXIT_TERMIN;
            rt->CurrentHandlers->Handlers[RXTER] = handler;
            break;

         case RXCMD:
            ExitFlags |= 1 << RX_EXIT_SUBCOM;
            rt->CurrentHandlers->Handlers[RXCMD] = handler;
            break;

         case RXFNC:
            ExitFlags |= 1 << RX_EXIT_FUNC;
            rt->CurrentHandlers->Handlers[RXFNC] = handler;
            break;

         case RXENV:
            ExitFlags |= ( 1 << RX_EXIT_GETENV ) | ( 1 << RX_EXIT_SETENV ) |
                         ( 1 << RX_EXIT_GETCWD ) | ( 1 << RX_EXIT_SETCWD );
            rt->CurrentHandlers->Handlers[RXENV] = handler;
            break;

         default:
            return RX_START_BADP;
      }
   }

   if ( EnvName )
   {
      EnvNamLen = strlen( EnvName );
      EnvNamStr = EnvName;
   }
   else
   {
      EnvNamLen = RX_NO_STRING;
      EnvNamStr = NULL;
   }

   SourcePtr = NULL;
   SourceLen = 0;
   TinPtr = NULL;
   TinLen = 0;
   if ( Instore && Instore[1].strptr )
   {
      WhereCode = RX_TYPE_INSTORE;
      TinPtr = Instore[1].strptr;
      TinLen = Instore[1].strlength;
      SourcePtr = Instore[0].strptr;
      SourceLen = Instore[0].strlength;
   }
   else if ( Instore && Instore[0].strptr )
   {
      WhereCode = RX_TYPE_SOURCE;
      SourcePtr = Instore[0].strptr;
      SourceLen = Instore[0].strlength;
   }
   else if ( Instore )
      WhereCode = RX_TYPE_MACRO;
   else
      WhereCode = RX_TYPE_EXTERNAL;

   starttrace( TSD );

   rc = IfcExecScript( TSD, strlen(ProgramName), ProgramName,
                       ArgCount, ParLengths, (const char **) ParStrings,
                       RXMAP_TYPE( CallType ), ExitFlags, EnvNamLen, EnvNamStr,
                       WhereCode, restricted, SourcePtr, SourceLen,
                       TinPtr, TinLen, &RLength, &RString,
                       &instore_buf, &instore_length);
   Handlers = rt->CurrentHandlers;
   rt->CurrentHandlers = Handlers->prev;
   TSD->MTFree( TSD, Handlers );

   if ( WhereCode == RX_TYPE_SOURCE )
   {
      Instore[1].strptr = (char *)instore_buf;
      Instore[1].strlength = instore_length;
   }

   if ( ReturnCode )
   {
      if ( RLength > 0 )
         *ReturnCode = (SHORT) atoi( RString );
      else
         *ReturnCode = 0;
   }

   if ( Result != NULL )
   {
      MAKERXSTRING( *Result, RString, RLength );
   }
   else
   {
      if ( RString != NULL )
         IfcFreeMemory( RString );
   }

   /*
    * Close all open files.
    */
   CloseOpenFiles( TSD, fpdCLEAR );
   if ( TSD->systeminfo->input_file != NULL )
   {
      Free_stringTSD( TSD->systeminfo->input_file );
      TSD->systeminfo->input_file = NULL;
   }
   free_orphaned_libs( TSD );

   return rc;
}

/*
 * In opposite to the documentation we accept a NULL parameter of Result
 * silently.
 */
/*****************************************************************************

    NAME
        APIRET RexxCallBack()

    SYNOPSIS
        PCSZ ProcedureName,
        LONG ArgCount,
        PRXSTRING ArgList,
        PSHORT ReturnCode,
        PRXSTRING Result );

    FUNCTION
        Calls a named Rexx procedure within an active interpreter session.

    INPUTS
        ProcedureName - Internal procedure to call.
        ArgCount, ArgList - Parameters supplied to the procedure.
        ReturnCode - Optional numeric result.
        Result - Optional RXSTRING receiving the textual result.

    RESULT
        The callback status. RX_CB_NOTSTARTED indicates no active interpreter;
        RX_CB_BADP, RX_CB_TOOMANYP, and RX_CB_BADN cover invalid arguments and
        unknown procedures.

    NOTES
        Normally call from the thread executing the interpreter. A NULL Result
        is accepted.

    SEE ALSO
        RexxStart()

*****************************************************************************/

EXPORT_C APIRET APIENTRY RexxCallBack( PCSZ       ProcedureName,
                                       LONG       ArgCount,
                                       PRXSTRING  ArgList,
                                       PSHORT     ReturnCode,
                                       PRXSTRING  Result )
{
   int rc, cnt, RLength;
   char *RString;
   int ParLengths[MAX_ARGS_TO_REXXSTART];
   const char *ParStrings[MAX_ARGS_TO_REXXSTART];
   tsd_t *TSD;

   /*
    * This can only be called with an active Rexx session running
    * and from the same thread as the interpreter is running in
    * The above is true UNLESS you have userd OPTIONS SINGLE_INTERPRETER
    */
   TSD = getGlobalTSD();
   if ( TSD == NULL )
      TSD = __regina_get_tsd();

   if ( TSD == NULL || TSD->systeminfo == NULL )
      return RX_CB_NOTSTARTED;

   if ( ( ArgCount < 0 ) || ( ( ArgCount > 0 ) && ( ArgList == NULL ) ) )
      return RX_CB_BADP;
   if ( !ProcedureName )
      return RX_CB_BADP;

   if (ArgCount > (int) (sizeof( ParLengths ) / sizeof( ParLengths[0] ) ) )
      return RX_CB_TOOMANYP;

   for ( cnt = 0; cnt < ArgCount; cnt++ )
   {
      ParLengths[cnt] = (int) RXSTRLEN( ArgList[cnt] );
      ParStrings[cnt] = RXSTRPTR( ArgList[cnt] );
      if ( ParStrings[cnt] == NULL )
         ParLengths[cnt] = RX_NO_STRING;
   }
   if ( Result != NULL )
   {
      RLength = (int) RXSTRLEN( *Result );
      if ( ( RString = RXSTRPTR( *Result ) ) == NULL )
         RLength = RX_NO_STRING;
   }
   else
   {
      RString = NULL;
      RLength = RX_NO_STRING;
   }

   rc = IfcExecCallBack( TSD, strlen(ProcedureName), ProcedureName,
          ArgCount, ParLengths, (const char **) ParStrings,
          &RLength, &RString );
   if ( rc == RX_CODE_NOSUCH )
      rc = RX_CB_BADN;

   /*
    * Determine numeric return code and pass it back
    */
   if ( ReturnCode )
   {
      if ( RLength > 0 )
         *ReturnCode = (SHORT) atoi( RString );
      else
         *ReturnCode = 0;
   }

   /*
    * Determine text return code and pass it back
    */
   if ( Result != NULL )
   {
      MAKERXSTRING( *Result, RString, RLength );
   }
   else
   {
      if ( RString != NULL )
         IfcFreeMemory( RString );
   }

   return rc;
}


/* ============================================================= */
/* subcom handler subsystem */

/*****************************************************************************

    NAME
        APIRET RexxRegisterSubcomExe()

    SYNOPSIS
        PCSZ EnvName,
        RexxSubcomHandler *EntryPoint,
        PUCHAR UserArea );

    FUNCTION
        Registers a direct handler for a named Rexx command environment.

    INPUTS
        EnvName - Command-environment name.
        EntryPoint - Subcommand handler callback.
        UserArea - Optional associated caller data.

    RESULT
        The registration status; RXSUBCOM_BADTYPE is returned when EnvName or
        EntryPoint is NULL.

    SEE ALSO
        RexxRegisterSubcomDll(), RexxDeregisterSubcom(), RexxQuerySubcom()

*****************************************************************************/

EXPORT_C APIRET APIENTRY RexxRegisterSubcomExe(PCSZ EnvName,
#ifdef RX_WEAKTYPING
                                      PFN              EntryPoint,
#else
                                      RexxSubcomHandler *EntryPoint,
#endif
                                      PUCHAR UserArea )
{
   tsd_t *TSD = getGlobalTSD();

   if ( TSD == NULL )
      TSD = GLOBAL_ENTRY_POINT();
   StartupInterface( TSD );

   /*
    * Perform sanity check on the parameters; UserArea may be NULL
    */
   if ( !EnvName || !EntryPoint )
      return RXSUBCOM_BADTYPE;

   return IfcRegSubcom( TSD, EnvName, NULL, NULL, (PFN)EntryPoint, UserArea );
}


/*****************************************************************************

    NAME
        APIRET RexxRegisterSubcomDll()

    SYNOPSIS
        PCSZ EnvName,
        PCSZ ModuleName,
        PCSZ ProcedureName,
        PUCHAR UserArea,
        ULONG DropAuth );

    FUNCTION
        Registers a command environment implemented by a dynamically loaded
        procedure.

    INPUTS
        EnvName - Name of the command environment.
        ModuleName - Module to load.
        ProcedureName - Exported handler name.
        UserArea - Optional associated data.
        DropAuth - RXSUBCOM_DROPPABLE or RXSUBCOM_NONDROP.

    RESULT
        Registration status. Missing required names or an invalid DropAuth
        produce RXSUBCOM_BADTYPE.

    NOTES
        DropAuth is checked but is not passed to the registration helper.

    SEE ALSO
        RexxRegisterSubcomExe(), RexxDeregisterSubcom()

*****************************************************************************/

EXPORT_C APIRET APIENTRY RexxRegisterSubcomDll(PCSZ EnvName,
                                      PCSZ ModuleName,
                                      PCSZ ProcedureName,
                                      PUCHAR UserArea,
                                      ULONG DropAuth )
{
   tsd_t *TSD = getGlobalTSD();

   if ( TSD == NULL )
      TSD = GLOBAL_ENTRY_POINT();
   StartupInterface( TSD );

   if ( !EnvName || !ModuleName || !ProcedureName )
      return RXSUBCOM_BADTYPE;
   if ( ( DropAuth != RXSUBCOM_DROPPABLE ) && ( DropAuth != RXSUBCOM_NONDROP ) )
      return RXSUBCOM_BADTYPE;

   return IfcRegSubcom( TSD, EnvName, ModuleName, ProcedureName, NULL,
                        UserArea );
}


/*****************************************************************************

    NAME
        APIRET RexxQuerySubcom()

    SYNOPSIS
        PCSZ EnvName,
        PCSZ ModuleName,
        PUSHORT Flag,
        PUCHAR UserArea );

    FUNCTION
        Queries a registered Rexx command environment.

    INPUTS
        EnvName - Environment name.
        ModuleName - Optional module name.
        Flag - Receives RXSUBCOM_ISREG if found, otherwise zero.
        UserArea - Receives associated registration data where applicable.

    RESULT
        The query status. A null EnvName or Flag produces RXSUBCOM_BADTYPE.

    SEE ALSO
        RexxRegisterSubcomExe(), RexxDeregisterSubcom()

*****************************************************************************/

EXPORT_C APIRET APIENTRY RexxQuerySubcom(PCSZ EnvName,
                                PCSZ ModuleName,
                                PUSHORT Flag,
                                PUCHAR UserWord )
{
   int ret;
   tsd_t *TSD = getGlobalTSD();

   if ( TSD == NULL )
      TSD = GLOBAL_ENTRY_POINT();
   StartupInterface( TSD );

   if ( !EnvName || !Flag || !Flag )
      return RXSUBCOM_BADTYPE;

   if ( ( ret = IfcQuerySubcom( TSD, EnvName, ModuleName, UserWord ) ) ==
                                                                  RXSUBCOM_OK )
      *Flag = RXSUBCOM_ISREG;
   else
      *Flag = 0;

   return ret;
}

/*****************************************************************************

    NAME
        APIRET RexxDeregisterSubcom()

    SYNOPSIS
        PCSZ EnvName,
        PCSZ ModuleName );

    FUNCTION
        Removes a registered Rexx subcommand environment.

    INPUTS
        EnvName - Name of the registered environment.
        ModuleName - Optional backing module name.

    RESULT
        The deregistration status. Null EnvName produces RXSUBCOM_BADTYPE.

    SEE ALSO
        RexxRegisterSubcomExe(), RexxRegisterSubcomDll()

*****************************************************************************/

EXPORT_C APIRET APIENTRY RexxDeregisterSubcom(PCSZ EnvName,
                                     PCSZ ModuleName )
{
   tsd_t *TSD = getGlobalTSD();

   if ( TSD == NULL )
      TSD = GLOBAL_ENTRY_POINT();
   StartupInterface( TSD );

   if ( !EnvName )
      return RXSUBCOM_BADTYPE;

   return IfcDelSubcom( TSD, EnvName, ModuleName );
}



/* ============================================================ */
/* Variable subsystem */
/* JH 20-10-99 */  /* To make Direct setting of stems Direct and not Symbolic. */
/****************************************************************************
 *
 *  JH 13/12/1999 (Original code changes on 20/10/1999)
 *
 *  BUG022            To make Direct setting of stems Direct and not Symbolic.
 *   - Added checks for the direct variable functions RX_GETVAR and RX_SETVAR.
 *     In the switch that determines what to do, based on the value passed in
 *     shvcode, symbolics still fall through to the code that is under the
 *     direct labels, but it sets a variable to denote that symbolic processing
 *     is to take place.  The direct section only sets this variable if it has
 *     not been set before.
 *   - Added new variable IVPcode (IfcVariablePool) that will contain the code
 *     used to call IfcVariablePool(), instead of hard coding the parameter,
 *
 *  NB that this routine lumps the Drop's and Set's together, before calling
 *    IfcVarPool().  At some point it might be better to pass the shvcode
 *    value, rather than translating it and later performing additional
 *    checks to split it back out.
 *
 ****************************************************************************/
/*****************************************************************************

    NAME
        APIRET RexxVariablePool()

    SYNOPSIS
        PSHVBLOCK RequestBlockList );

    FUNCTION
        Processes a linked sequence of Rexx variable-pool requests in the
        running interpreter.

    INPUTS
        RequestBlockList - First SHVBLOCK request. Each block selects an
                           operation with shvcode and reports its outcome in
                           shvret.

    RESULT
        Bitwise combination of the request status bits. RXSHV_NOAVL is
        returned when no interpreter variable tree is available; a null
        request list produces RXFUNC_BADTYPE.

    NOTES
        For each request, inspect shvret and any returned name or value. The
        interface requires an active interpreter context.

    SEE ALSO
        RexxStart(), RexxCallBack()

*****************************************************************************/

EXPORT_C APIRET APIENTRY RexxVariablePool(PSHVBLOCK RequestBlockList )
{
   int Code=0, RetCode=0, IVPcode;
   int Lengths[2] ;
   int rc=0, allocated ;
   char *Strings[2] ;
   PSHVBLOCK Req=RequestBlockList ;
   tsd_t *TSD = getGlobalTSD();

   if ( TSD == NULL )
      TSD = GLOBAL_ENTRY_POINT();
   StartupInterface(TSD);

   if (!RequestBlockList) /* FGC: I assume we must have at least one param */
      return(RXFUNC_BADTYPE);

   if (TSD->systeminfo->tree.root==NULL) /* Doesn't the interpreter run? */
      return RXSHV_NOAVL ;

   RetCode = 0 ;

   for (;Req;Req=Req->shvnext)
   {
      IVPcode = 0; /* Needed for a correct IVPcode on a second request */
      allocated = 0;
      switch (Req->shvcode)
      {
         case RXSHV_SYDRO:
         case RXSHV_SYSET:
           IVPcode = RX_SETSVAR;                        /* JH 20-10-99 */
         case RXSHV_DROPV:                              /* MH 26-12-95 */
         case RXSHV_SET:                                /* MH 26-12-95 */
         {
            IVPcode = IVPcode ? IVPcode : RX_SETVAR;    /* JH 20-10-99 */
            Lengths[0] = Req->shvname.strlength ;
            Strings[0] = Req->shvname.strptr ;
            if (Req->shvcode==RXSHV_SYSET               /* MH 26-12-95 */
            ||  Req->shvcode==RXSHV_SET)                /* MH 26-12-95 */
            {
               Lengths[1] = Req->shvvalue.strlength ;
               Strings[1] = Req->shvvalue.strptr ;
            }
            else
               Lengths[1] = RX_NO_STRING ;

            Code = IfcVarPool( TSD, IVPcode, Lengths, Strings, &allocated );

            Req->shvret = RXSHV_OK ;
            if (Code==RX_CODE_NOVALUE)
               Req->shvret |= RXSHV_NEWV ;
            else if (Code==RX_CODE_INVNAME)
               Req->shvret |= RXSHV_BADN ;
            else if (Code!=RXSHV_OK)
               exiterror( ERR_INTERPRETER_FAILURE, 1, __FILE__, __LINE__, "" )  ;
            TSD->var_indicator=0;
            break ;
         }
         case RXSHV_SYFET:
           IVPcode = RX_GETSVAR;                        /* JH 20-10-99 */
         case RXSHV_FETCH:                              /* MH 26-12-95 */
         {
            IVPcode = IVPcode ? IVPcode : RX_GETVAR;    /* JH 20-10-99 */
            Lengths[0] = Req->shvname.strlength ;
            Strings[0] = Req->shvname.strptr ;
            Code = IfcVarPool( TSD, IVPcode, Lengths, Strings, &allocated );

            Req->shvret = RXSHV_OK ;
            if (Code==RX_CODE_NOVALUE)
               Req->shvret |= RXSHV_NEWV ;
            else if (Code==RX_CODE_INVNAME)
               Req->shvret |= RXSHV_BADN ;
            else if (Code!=RXSHV_OK)
               exiterror( ERR_INTERPRETER_FAILURE, 1, __FILE__, __LINE__, "" )  ;
            FillReqValue( Req, Lengths[1], Strings[1] ) ;
            TSD->var_indicator=0;
            break ;
         }

         case RXSHV_PRIV:
         {
            Req->shvret = RXSHV_OK ;
            if (Req->shvname.strlength==4 && Req->shvname.strptr &&
                !strncmp(Req->shvname.strptr, "PARM", 4 ))
            {
               rc = IfcVarPool( TSD, RX_CODE_PARAMS, Lengths, Strings, &allocated );
               FillReqValue( Req, Lengths[0], Strings[0] ) ;
            }

            else if (Req->shvname.strlength>=5 && Req->shvname.strptr &&
                !strncmp(Req->shvname.strptr, "PARM.", 5 ))
            {
               Lengths[0] = Req->shvname.strlength - 5 ;
               Strings[0] = Req->shvname.strptr + 5 ;

               rc = IfcVarPool( TSD, RX_CODE_PARAM, Lengths, Strings, &allocated );
               if (rc == RX_CODE_OK)
                  FillReqValue( Req, Lengths[1], Strings[1] ) ;
               else
                  Req->shvret |= RXSHV_BADN ;
            }

            else
            {
               int Code=0 ;
               if (Req->shvname.strptr)
               {
                  if (Req->shvname.strlength==7 &&
                         !memcmp(Req->shvname.strptr, "QUENAME", 7))
                  {
                     Code = RX_CODE_QUEUE ;
                  }
                  else if (Req->shvname.strlength==7 &&
                         !memcmp(Req->shvname.strptr, "VERSION", 7))
                  {
                     Code = RX_CODE_VERSION ;
                  }
                  else if (Req->shvname.strlength==6 &&
                         !memcmp(Req->shvname.strptr, "SOURCE", 6))
                  {
                     Code = RX_CODE_SOURCE ;
                  }
                  else
                     Req->shvret |= RXSHV_BADN ;

                  if (!(Req->shvret & RXSHV_BADN))
                  {
                     rc=IfcVarPool( TSD, Code, Lengths, Strings, &allocated );
                     FillReqValue( Req, Lengths[0], Strings[0] ) ;
                  }
               }
               else
                  Req->shvret |= RXSHV_BADN ;
            }
            break ;
         }

         case RXSHV_NEXTV:
         {
            int Items ;

            Req->shvret = RXSHV_OK ;
            Items = IfcVarPool( TSD, RX_NEXTVAR, Lengths, Strings, &allocated );
            assert( Items==0 || Items==2 ) ;

            if (Items==2)
            {
               FillReqValue( Req, Lengths[1], Strings[1] ) ;
               FillReqName( Req, Lengths[0], Strings[0] ) ;
            }
            else
               Req->shvret |= RXSHV_LVAR ;

            break ;
         }

         default:
            Req->shvret = RXSHV_BADF ;
      }
      if (allocated & 1) /* fixes bug 596686 */
         FreeTSD( Strings[0] );
      if (allocated & 2)
         FreeTSD( Strings[1] );
      RetCode |= ( Req->shvret & 0x007f ) ;
   }

   return RetCode ;
}



/* ================================================================ */
/* system exit handler subsystem */

/*****************************************************************************

    NAME
        APIRET RexxRegisterExitExe()

    SYNOPSIS
        PCSZ EnvName,
        RexxExitHandler *EntryPoint,
        PUCHAR UserArea );

    FUNCTION
        Registers a process-resident system exit handler for RexxStart().

    INPUTS
        EnvName - Name used to identify the exit.
        EntryPoint - Callback to invoke.
        UserArea - Optional caller data associated with the registration.

    RESULT
        The status returned by the exit-registration interface. RXEXIT_BADTYPE
        is returned for a null name or callback.

    SEE ALSO
        RexxRegisterExitDll(), RexxDeregisterExit(), RexxQueryExit()

*****************************************************************************/

EXPORT_C APIRET APIENTRY RexxRegisterExitExe(PCSZ EnvName,
#ifdef RX_WEAKTYPING
                                             PFN              EntryPoint,
#else
                                             RexxExitHandler *EntryPoint,
#endif
                                             PUCHAR UserArea )
{
   tsd_t *TSD = getGlobalTSD();

   if ( TSD == NULL )
      TSD = GLOBAL_ENTRY_POINT();
   StartupInterface( TSD );

   /*
    * Perform sanity check on the parameters; UserArea may be NULL
    */
   if ( !EnvName || !EntryPoint )
      return RXEXIT_BADTYPE;

   return IfcRegExit( TSD, EnvName, NULL, NULL, (PFN)EntryPoint, UserArea );
}

/*****************************************************************************

    NAME
        APIRET RexxRegisterExitDll()

    SYNOPSIS
        PCSZ EnvName,
        PCSZ ModuleName,
        PCSZ ProcedureName,
        PUCHAR UserArea,
        ULONG DropAuth );

    FUNCTION
        Registers a system exit handler identified by a dynamic module and
        procedure.

    INPUTS
        EnvName - Registration name.
        ModuleName - Module providing the handler.
        ProcedureName - Exported handler name.
        UserArea - Optional caller data.
        DropAuth - RXEXIT_DROPPABLE or RXEXIT_NONDROP.

    RESULT
        The exit-registration status. Null required names or an invalid
        DropAuth produce RXEXIT_BADTYPE.

    NOTES
        The implementation validates DropAuth but does not pass it to the
        registration helper.

    SEE ALSO
        RexxRegisterExitExe(), RexxDeregisterExit(), RexxQueryExit()

*****************************************************************************/

EXPORT_C APIRET APIENTRY RexxRegisterExitDll(PCSZ EnvName,
                                    PCSZ ModuleName,
                                    PCSZ ProcedureName,
                                    PUCHAR UserArea,
                                    ULONG DropAuth )
{
   tsd_t *TSD = getGlobalTSD();

   if ( TSD == NULL )
      TSD = GLOBAL_ENTRY_POINT();
   StartupInterface( TSD );

   if ( !EnvName || !ModuleName || !ProcedureName )
      return RXEXIT_BADTYPE;
   if ( ( DropAuth != RXEXIT_DROPPABLE ) && ( DropAuth != RXEXIT_NONDROP ) )
      return RXEXIT_BADTYPE;

   return IfcRegExit( TSD, EnvName, ModuleName, ProcedureName, NULL, UserArea );
}


/*****************************************************************************

    NAME
        APIRET RexxDeregisterExit()

    SYNOPSIS
        PCSZ EnvName,
        PCSZ ModuleName );

    FUNCTION
        Removes a previously registered system exit handler.

    INPUTS
        EnvName - Name of the exit registration.
        ModuleName - Module name for a module-backed registration, or NULL for
                     a direct handler.

    RESULT
        The deregistration status. A null EnvName produces RXEXIT_BADTYPE.

    SEE ALSO
        RexxRegisterExitExe(), RexxRegisterExitDll(), RexxQueryExit()

*****************************************************************************/

EXPORT_C APIRET APIENTRY RexxDeregisterExit(PCSZ EnvName,
                                   PCSZ ModuleName )
{
   tsd_t *TSD = getGlobalTSD();

   if ( TSD == NULL )
      TSD = GLOBAL_ENTRY_POINT();
   StartupInterface( TSD );

   if ( !EnvName )
      return RXEXIT_BADTYPE;

   return IfcDelExit( TSD, EnvName, ModuleName );
}

/*****************************************************************************

    NAME
        APIRET RexxQueryExit()

    SYNOPSIS
        PCSZ EnvName,
        PCSZ ModuleName,
        PUSHORT Flag,
        PUCHAR UserArea );

    FUNCTION
        Checks for a registered system exit handler.

    INPUTS
        EnvName - Registration name.
        ModuleName - Optional module name.
        Flag - Receives RXEXIT_ISREG on success, otherwise zero.
        UserArea - Receives the registered user data where applicable.

    RESULT
        The query status. A null EnvName or Flag produces RXEXIT_BADTYPE.

    SEE ALSO
        RexxRegisterExitExe(), RexxDeregisterExit()

*****************************************************************************/

EXPORT_C APIRET APIENTRY RexxQueryExit(PCSZ EnvName,
                              PCSZ ModuleName,
                              PUSHORT Flag,
                              PUCHAR UserArea)
{
   int ret;
   tsd_t *TSD = getGlobalTSD();

   if ( TSD == NULL )
      TSD = GLOBAL_ENTRY_POINT();
   StartupInterface( TSD );

   if ( !EnvName || !Flag || !Flag )
      return RXEXIT_BADTYPE;

   if ( ( ret = IfcQueryExit( TSD, EnvName, ModuleName, UserArea ) ) ==
                                                                    RXEXIT_OK )
      *Flag = RXEXIT_ISREG;
   else
      *Flag = 0;

   return ret;
}

/* =================================================================== */

/*
 * This section contains the support for the external functions
 */

/*****************************************************************************

    NAME
        APIRET RexxRegisterFunctionExe()

    SYNOPSIS
        PCSZ Name,
        RexxFunctionHandler *EntryPoint );

    FUNCTION
        Registers a direct external Rexx function handler.

    INPUTS
        Name - Rexx-callable function name.
        EntryPoint - Handler callback.

    RESULT
        The registration status. A null Name or EntryPoint produces
        RXFUNC_BADTYPE.

    SEE ALSO
        RexxRegisterFunctionDll(), RexxDeregisterFunction(), RexxQueryFunction()

*****************************************************************************/

EXPORT_C APIRET APIENTRY RexxRegisterFunctionExe( PCSZ Name,
#ifdef RX_WEAKTYPING
                                                  PFN                  EntryPoint )
#else
                                                  RexxFunctionHandler *EntryPoint )
#endif
{
   tsd_t *TSD = getGlobalTSD();

   if ( TSD == NULL )
      TSD = GLOBAL_ENTRY_POINT();
   StartupInterface( TSD );

   if ( !Name || !EntryPoint )
      return RXFUNC_BADTYPE;

   return IfcRegFunc( TSD, Name, NULL, NULL, (PFN)EntryPoint );
}

/*****************************************************************************

    NAME
        APIRET RexxRegisterFunctionDll()

    SYNOPSIS
        PCSZ ExternalName,
        PCSZ LibraryName,
        PCSZ InternalName );

    FUNCTION
        Registers a Rexx function backed by a named dynamic library export.

    INPUTS
        ExternalName - Rexx-visible name.
        LibraryName - Module containing the implementation.
        InternalName - Exported procedure name.

    RESULT
        The registration status. A null required name produces RXFUNC_BADTYPE.

    SEE ALSO
        RexxRegisterFunctionExe(), RexxDeregisterFunction()

*****************************************************************************/

EXPORT_C APIRET APIENTRY RexxRegisterFunctionDll( PCSZ ExternalName,
                                                  PCSZ LibraryName,
                                                  PCSZ InternalName )
{
   tsd_t *TSD = getGlobalTSD();

   if ( TSD == NULL )
      TSD = GLOBAL_ENTRY_POINT();
   StartupInterface( TSD );

   if ( !ExternalName || !LibraryName || !InternalName )
      return RXFUNC_BADTYPE;

   return IfcRegFunc( TSD, ExternalName, LibraryName, InternalName, NULL );
}

/*****************************************************************************

    NAME
        APIRET RexxQueryFunction()

    SYNOPSIS
        PCSZ Name );

    FUNCTION
        Looks up an external Rexx function registration.

    INPUTS
        Name - Rexx function name to query.

    RESULT
        The query status. A null Name produces RXFUNC_BADTYPE.

    SEE ALSO
        RexxRegisterFunctionExe(), RexxDeregisterFunction()

*****************************************************************************/

EXPORT_C APIRET APIENTRY RexxQueryFunction( PCSZ Name )
{
   tsd_t *TSD = getGlobalTSD();

   if ( TSD == NULL )
      TSD = GLOBAL_ENTRY_POINT();
   StartupInterface( TSD );

   if ( !Name )
      return RXFUNC_BADTYPE;
   return IfcQueryFunc( TSD, Name );
}


/*****************************************************************************

    NAME
        APIRET RexxDeregisterFunction()

    SYNOPSIS
        PCSZ Name );

    FUNCTION
        Removes an external Rexx function registration.

    INPUTS
        Name - Registered Rexx function name.

    RESULT
        The deregistration status. A null Name produces RXFUNC_BADTYPE.

    SEE ALSO
        RexxRegisterFunctionExe(), RexxQueryFunction()

*****************************************************************************/

EXPORT_C APIRET APIENTRY RexxDeregisterFunction( PCSZ Name )
{
   tsd_t *TSD = getGlobalTSD();

   if ( TSD == NULL )
      TSD = GLOBAL_ENTRY_POINT();
   StartupInterface( TSD );

   if ( !Name )
      return RXFUNC_BADTYPE;

   return IfcDelFunc( TSD, Name );
}

/* The caller of IfcFunctionExit should call this function with fresh copies of
 * Name and params to be bullet-proof. The called function MAY
 * change the values although this is illegal.
 */
static int IfcFunctionExit( tsd_t *TSD,
                            PSZ Name,
                            int Params,
                            RXSTRING *params,
                            PCSZ queuename,
                            int queuelen,
                            PRXSTRING Retstr,
                            int *RCode,
                            char called )
{
   int rc=0 ;
   RXFNCCAL_PARM fnccal;
   PUCHAR parm=NULL;
   rex_tsd_t *rt;

   rt = (rex_tsd_t *)TSD->rex_tsd;

   if ( rt->CurrentHandlers && rt->CurrentHandlers->Handlers[RXFNC] )
   {
      fnccal.rxfnc_flags.rxfferr = 0;
      fnccal.rxfnc_flags.rxffnfnd = 0;
      fnccal.rxfnc_flags.rxffsub = (called) ? 1 : 0;
      fnccal.rxfnc_name = (unsigned char *)Name;
      fnccal.rxfnc_namel = (USHORT) strlen(Name);
      fnccal.rxfnc_que = (unsigned char *)queuename;
      fnccal.rxfnc_quel = (USHORT) queuelen;
      fnccal.rxfnc_argc = (USHORT) Params;
      fnccal.rxfnc_argv = params;
      fnccal.rxfnc_retc = *Retstr;
      parm = (PUCHAR)&fnccal;
      rc = (*(rt->CurrentHandlers->Handlers[RXFNC]))(RXFNC, RXFNCCAL, parm);
      TSD->var_indicator = 0;
      assert( rc==RXEXIT_HANDLED || rc==RXEXIT_NOT_HANDLED ||
              rc==RXEXIT_RAISE_ERROR ) ;
      if (rc == RXEXIT_HANDLED)
      {
         if (fnccal.rxfnc_flags.rxfferr)
            *RCode = RXFLAG_ERROR ;
         else if (fnccal.rxfnc_flags.rxffnfnd)
            *RCode = RXFLAG_FAILURE ;
         else
            *RCode = RXFLAG_OK;
      }
      *Retstr = fnccal.rxfnc_retc;
      return(rc);
   }
   else
   {
      return (RXEXIT_NOT_HANDLED);
   }
}

/* The caller of IfcExecFunc should call this function with fresh copies of
 * Name, Length and Strings to be bullet-proof. The called function MAY
 * change the values although this is illegal.
 * RetLength and RetString should point to {0,NULL}. They will be filled with
 * freshly allocated values if there are some.
 */
int IfcExecFunc( tsd_t *TSD,
                 PFN Func,
                 char *Name, int Params,
                 int *Lengths, char **Strings,
                 int queue_name_len, char *queue_name,
                 int *RetLength, char **RetString,
                 int *RC, char called, void *gci_info )
{
   int i=0, length=0, rc=0, RCode=0 ;
   RXSTRING *params, retstr ;
   char execfunc_result[ILLEGAL_USE_SIZE+RXAUTOBUFLEN] ;
   RexxFunctionHandler *FullFunc;

   assert( Name ) ;
   assert( Params >= 0 ) ;
   FullFunc = (RexxFunctionHandler *)Func;

   params = (RXSTRING *)MallocTSD( sizeof(RXSTRING)*Params ) ;
   for (i=0; i<Params; i++)
   {
      length = Lengths[i] ;
      if (length==RX_NO_STRING)
      {
         params[i].strptr = NULL ;
         params[i].strlength = 0 ;
      }
      else
      {
         params[i].strptr = Strings[i] ;
         params[i].strlength = length ;
      }
   }

   memset( execfunc_result, 0, sizeof( execfunc_result ) ) ;
   retstr.strptr = execfunc_result + ILLEGAL_USE_SIZE ;
   retstr.strlength = RXAUTOBUFLEN;        /* MH 26-12-95 */

   rc = IfcFunctionExit( TSD, Name, Params, params, queue_name, queue_name_len,
                         &retstr, &RCode, called );
   switch(rc)
   {
      case RXEXIT_NOT_HANDLED:
         if ( Func == NULL )
         {
           *RC = ERR_ROUTINE_NOT_FOUND;
         }
         else
         {
#if defined(DYNAMIC) && defined(HAVE_GCI)
            if ( gci_info != NULL )
               rc = GCI_Dispatcher( TSD, (PFN)Func, gci_info, Params, params, &retstr );
            else
#endif
            /* Func will inherit a possible return value in
             * retstr. This might be a problem, expect suspicious results
             * if the called functions are not error free.
             */
               rc = (*(FullFunc))( Name, Params, params, queue_name, &retstr ) ;

            if (rc)
               *RC = ERR_INCORRECT_CALL;
            else
               *RC = 0;
            TSD->var_indicator = 0;
         }
         break;
      case RXEXIT_HANDLED:
         if (RCode == RXFLAG_ERROR)
            *RC = ERR_INCORRECT_CALL;
         else if (RCode == RXFLAG_FAILURE)
            *RC = ERR_ROUTINE_NOT_FOUND;
         else
            *RC = 0;
         break;
      case RXEXIT_RAISE_ERROR:
         *RC = ERR_SYSTEM_FAILURE;
         break;
   }

   FreeTSD( params ) ;

   if (!(*RC) && retstr.strptr)
   {
      *RetString = (char *)MallocTSD( (retstr.strlength < 1) ? 1 : retstr.strlength ) ;
      memcpy( *RetString, retstr.strptr, retstr.strlength ) ;
      *RetLength = retstr.strlength ;
   }
   else
      *RetLength = RX_NO_STRING ;

   if (retstr.strptr && retstr.strptr != execfunc_result + ILLEGAL_USE_SIZE)
      IfcFreeMemory( retstr.strptr ) ;

   return RX_CODE_OK ;
}

int IfcHaveFunctionExit(const tsd_t *TSD)
{
   rex_tsd_t *rt;

   rt = (rex_tsd_t *)TSD->rex_tsd;
   if ( rt->CurrentHandlers && rt->CurrentHandlers->Handlers[RXFNC] )
      return 1;
   else
      return 0;
}

/* ============================================================= */
/* Asynchronous Rexx API interface */

/*****************************************************************************

    NAME
        APIRET RexxSetHalt()

    SYNOPSIS
        LONG pid,
        LONG tid );

    FUNCTION
        Requests interruption of the current Rexx interpreter context.

    INPUTS
        pid, tid - API process and thread identifiers.

    RESULT
        RXARI_OK after setting the current context halt flag.

    NOTES
        This implementation ignores pid and tid; it does not select a
        different process or thread.

    SEE ALSO
        RexxStart()

*****************************************************************************/

EXPORT_C APIRET APIENTRY RexxSetHalt(LONG dummyProcess,
                            LONG dummyThread )
{
   tsd_t *TSD = getGlobalTSD();

   if ( TSD == NULL )
      TSD = GLOBAL_ENTRY_POINT();
   StartupInterface(TSD);
   /*
    * Perform sanity check on the parameters; is process id me ?
    */
   set_rexx_halt( TSD );
   return RXARI_OK ;
}

/* ============================================================= */
/* Named queue interface */

/*****************************************************************************

    NAME
        APIRET RexxCreateQueue()

    SYNOPSIS
        PSZ Buffer,
        ULONG BuffLen,
        PSZ RequestedName,
        ULONG *DupFlag );

    FUNCTION
        Creates a named queue in the Regina queue subsystem.

    INPUTS
        Buffer - Receives the resulting queue name.
        BuffLen - Size of Buffer.
        RequestedName - Optional requested queue name.
        DupFlag - Receives the duplicate-name outcome.

    RESULT
        The queue creation status returned by the queue subsystem.

    NOTES
        The AROS build disables external queues with NO_EXTERNAL_QUEUES. Do
        not assume a named queue is shared between processes.

    SEE ALSO
        RexxDeleteQueue(), RexxQueryQueue()

*****************************************************************************/

EXPORT_C APIRET APIENTRY RexxCreateQueue( PSZ Buffer,
                                 ULONG BuffLen,
                                 PSZ RequestedName,
                                 ULONG* DupFlag)
{
   int code;
   unsigned long dupflag = *DupFlag;
   tsd_t *TSD = getGlobalTSD();

   if ( TSD == NULL )
      TSD = GLOBAL_ENTRY_POINT();
   StartupInterface(TSD);

   TSD->called_from_saa = 1;
   code = IfcCreateQueue( TSD, RequestedName, (RequestedName) ? strlen( RequestedName): 0, Buffer, &dupflag, BuffLen );
   *DupFlag = (ULONG)dupflag;
   TSD->called_from_saa = 0;
   return code;
}

/*****************************************************************************

    NAME
        APIRET RexxDeleteQueue()

    SYNOPSIS
        PSZ QueueName );

    FUNCTION
        Deletes a queue by name.

    INPUTS
        QueueName - Nonempty queue name.

    RESULT
        The queue operation status; a missing or empty name produces
        RXQUEUE_BADQNAME.

    NOTES
        The AROS build provides local queue handling but disables external
        queues.

    SEE ALSO
        RexxCreateQueue()

*****************************************************************************/

EXPORT_C APIRET APIENTRY RexxDeleteQueue( PSZ QueueName )
{
   int code;
   tsd_t *TSD = getGlobalTSD();

   if ( TSD == NULL )
      TSD = GLOBAL_ENTRY_POINT();
   StartupInterface(TSD);

   TSD->called_from_saa = 1;
   if (!QueueName || !strlen(QueueName))
      code = RXQUEUE_BADQNAME;
   else
      code = IfcDeleteQueue( TSD, QueueName, strlen( QueueName ) );
   TSD->called_from_saa = 0;
   return code;
}

/*****************************************************************************

    NAME
        APIRET RexxQueryQueue()

    SYNOPSIS
        PSZ QueueName,
        ULONG *Count );

    FUNCTION
        Obtains the number of entries in a named queue.

    INPUTS
        QueueName - Nonempty queue name.
        Count - Receives the number of entries.

    RESULT
        The query status; a missing or empty name produces RXQUEUE_BADQNAME.

    NOTES
        The AROS build disables external queues. Queue visibility is limited
        by the local queue subsystem.

    SEE ALSO
        RexxCreateQueue(), RexxAddQueue(), RexxPullQueue()

*****************************************************************************/

EXPORT_C APIRET APIENTRY RexxQueryQueue( PSZ QueueName,
                                ULONG* Count)
{
   int code;
   unsigned long count = *Count;
   tsd_t *TSD = getGlobalTSD();

   if ( TSD == NULL )
      TSD = GLOBAL_ENTRY_POINT();
   StartupInterface(TSD);

   TSD->called_from_saa = 1;
   if (!QueueName || !strlen(QueueName))
      code = RXQUEUE_BADQNAME;
   else
      code = IfcQueryQueue( TSD, QueueName, strlen( QueueName ), &count );
   *Count = (ULONG)count;
   TSD->called_from_saa = 0;
   return code;
}

/*****************************************************************************

    NAME
        APIRET RexxAddQueue()

    SYNOPSIS
        PSZ QueueName,
        PRXSTRING EntryData,
        ULONG AddFlag );

    FUNCTION
        Adds an RXSTRING entry to a named queue.

    INPUTS
        QueueName - Nonempty queue name.
        EntryData - String and length to add.
        AddFlag - RXQUEUE_FIFO or RXQUEUE_LIFO.

    RESULT
        The queue operation status; a missing or empty name produces
        RXQUEUE_BADQNAME.

    NOTES
        The AROS build disables external queues.

    SEE ALSO
        RexxPullQueue(), RexxQueryQueue()

*****************************************************************************/

EXPORT_C APIRET APIENTRY RexxAddQueue( PSZ QueueName,
                              PRXSTRING EntryData,
                              ULONG AddFlag)
{
   int code;
   tsd_t *TSD = getGlobalTSD();

   if ( TSD == NULL )
      TSD = GLOBAL_ENTRY_POINT();
   StartupInterface(TSD);

   TSD->called_from_saa = 1;
   if (!QueueName || !strlen(QueueName))
      code = RXQUEUE_BADQNAME;
   else
      code = IfcAddQueue( TSD, QueueName, strlen( QueueName), EntryData->strptr, EntryData->strlength, AddFlag==RXQUEUE_LIFO );
   TSD->called_from_saa = 0;
   return code;
}

/*****************************************************************************

    NAME
        APIRET RexxPullQueue()

    SYNOPSIS
        PSZ QueueName,
        PRXSTRING DataBuf,
        PDATETIME TimeStamp,
        ULONG WaitFlag );

    FUNCTION
        Removes an entry from the named queue.

    INPUTS
        QueueName - Nonempty queue name.
        DataBuf - Receives the entry data and its length.
        TimeStamp - Optional timestamp structure; valid is cleared on success.
        WaitFlag - RXQUEUE_WAIT to wait, or RXQUEUE_NOWAIT to return without
                   waiting.

    RESULT
        The queue operation status. Invalid WaitFlag produces
        RXQUEUE_BADWAITFLAG; a null DataBuf produces RXQUEUE_MEMFAIL.

    NOTES
        The AROS build disables external queues.

    SEE ALSO
        RexxAddQueue(), RexxQueryQueue()

*****************************************************************************/

EXPORT_C APIRET APIENTRY RexxPullQueue( PSZ QueueName,
                               PRXSTRING DataBuf,
                               PDATETIME TimeStamp,
                               ULONG WaitFlag)
{
   int code;
   tsd_t *TSD = getGlobalTSD();

   if ( TSD == NULL )
      TSD = GLOBAL_ENTRY_POINT();
   StartupInterface( TSD );

   if ( WaitFlag != RXQUEUE_WAIT && WaitFlag != RXQUEUE_NOWAIT )
      return RXQUEUE_BADWAITFLAG;

   if ( DataBuf == NULL )
      return RXQUEUE_MEMFAIL;

   TSD->called_from_saa = 1;
   if ( !QueueName || !strlen( QueueName ) )
      code = RXQUEUE_BADQNAME;
   else
   {
      unsigned long strlength = 0;
      code = IfcPullQueue( TSD,
                           QueueName, strlen( QueueName ),
                           &DataBuf->strptr, &strlength,
                           WaitFlag==RXQUEUE_WAIT );
      DataBuf->strlength = strlength;
      if ( code == 0 )
      {
         if ( TimeStamp )
            TimeStamp->valid = 0;
      }
   }
   TSD->called_from_saa = 0;
   return code;
}

/* ============================================================= */
/* MacroSpace Rexx API interface */

/*****************************************************************************

    NAME
        APIRET RexxAddMacro()

    SYNOPSIS
        PSZ FuncName,
        PSZ SourceFile,
        ULONG Position );

    FUNCTION
        This entry point is intended to register a source program in the macro
        space, but is not implemented.

    INPUTS
        FuncName - Macro registration name.
        SourceFile - Rexx source file.
        Position - Requested search position.

    RESULT
        Returns RXMACRO_OK (zero) without performing the requested operation.

    SEE ALSO
        RexxStart()

*****************************************************************************/

EXPORT_C APIRET APIENTRY RexxAddMacro( PSZ FuncName,
                              PSZ SourceFile,
                              ULONG Position )
{
   return 0;
}

/*****************************************************************************

    NAME
        APIRET RexxDropMacro()

    SYNOPSIS
        PSZ FuncName );

    FUNCTION
        This entry point is intended to remove a program from the macro space,
        but is not implemented.

    INPUTS
        FuncName - Macro registration name.

    RESULT
        Returns RXMACRO_OK (zero) without performing the requested operation.

    SEE ALSO
        RexxStart()

*****************************************************************************/

EXPORT_C APIRET APIENTRY RexxDropMacro( PSZ FuncName)
{
   return 0;
}

/*****************************************************************************

    NAME
        APIRET RexxSaveMacroSpace()

    SYNOPSIS
        ULONG FuncCount,
        PSZ *FuncNames,
        PSZ MacroLibFile );

    FUNCTION
        This entry point is intended to save macro-space entries to a file,
        but is not implemented.

    INPUTS
        FuncCount - Number of listed names.
        FuncNames - Names to save.
        MacroLibFile - Destination file.

    RESULT
        Returns RXMACRO_OK (zero) without performing the requested operation.

    SEE ALSO
        RexxStart()

*****************************************************************************/

EXPORT_C APIRET APIENTRY RexxSaveMacroSpace( ULONG FuncCount,
                                    PSZ * FuncNames,
                                    PSZ MacroLibFile)
{
   return 0;
}

/*****************************************************************************

    NAME
        APIRET RexxLoadMacroSpace()

    SYNOPSIS
        ULONG FuncCount,
        PSZ *FuncNames,
        PSZ MacroLibFile );

    FUNCTION
        This entry point is intended to load macro-space entries from a file,
        but is not implemented.

    INPUTS
        FuncCount - Number of listed names.
        FuncNames - Names to load.
        MacroLibFile - Source file.

    RESULT
        Returns RXMACRO_OK (zero) without performing the requested operation.

    SEE ALSO
        RexxStart()

*****************************************************************************/

EXPORT_C APIRET APIENTRY RexxLoadMacroSpace( ULONG FuncCount,
                                    PSZ * FuncNames,
                                    PSZ MacroLibFile)
{
   return 0;
}

/*****************************************************************************

    NAME
        APIRET RexxQueryMacro()

    SYNOPSIS
        PSZ FuncName,
        PUSHORT Position );

    FUNCTION
        This entry point is intended to query a macro-space entry, but is not
        implemented.

    INPUTS
        FuncName - Macro name.
        Position - Intended output for the search position; unchanged by this
                   implementation.

    RESULT
        Returns RXMACRO_OK (zero) without performing the requested operation.

    SEE ALSO
        RexxStart()

*****************************************************************************/

EXPORT_C APIRET APIENTRY RexxQueryMacro( PSZ FuncName,
                                PUSHORT Position )
{
   return 0;
}

/*****************************************************************************

    NAME
        APIRET RexxReorderMacro()

    SYNOPSIS
        PSZ FuncName,
        ULONG Position );

    FUNCTION
        This entry point is intended to change the search order of a macro-
        space entry, but is not implemented.

    INPUTS
        FuncName - Macro name.
        Position - Requested search position.

    RESULT
        Returns RXMACRO_OK (zero) without performing the requested operation.

    SEE ALSO
        RexxStart()

*****************************************************************************/

EXPORT_C APIRET APIENTRY RexxReorderMacro( PSZ FuncName,
                                  ULONG Position )
{
   return 0;
}

/*****************************************************************************

    NAME
        APIRET RexxClearMacroSpace()

    SYNOPSIS
        );

    FUNCTION
        This entry point is intended to clear the macro space, but is not
        implemented.

    RESULT
        Returns RXMACRO_OK (zero) without performing the requested operation.

    SEE ALSO
        RexxStart()

*****************************************************************************/

EXPORT_C APIRET APIENTRY RexxClearMacroSpace( VOID )
{
   return 0;
}

/* ============================================================= */
/* Regina extensions */
/* see rexxsaa.h for a description */
/*****************************************************************************

    NAME
        APIRET ReginaVersion()

    SYNOPSIS
        PRXSTRING VersionString );

    FUNCTION
        Returns the Regina library version and can optionally fill a version
        string.

    INPUTS
        VersionString - Optional RXSTRING output. If its length is zero,
                        Regina allocates a string buffer; otherwise it writes
                        into the supplied buffer up to its declared length.

    RESULT
        A numeric version code with the major version in the upper byte and a
        two-digit minor component in the low byte.

    NOTES
        VersionString.strlength excludes the terminating NUL. Release an API-
        allocated version buffer with RexxFreeMemory().

    SEE ALSO
        ReginaCleanup(), RexxFreeMemory()

*****************************************************************************/

EXPORT_C APIRET APIENTRY ReginaVersion( PRXSTRING VersionString )
{
   char low[3];
   unsigned len;
   tsd_t *TSD = getGlobalTSD();

   if ( TSD == NULL )
      TSD = GLOBAL_ENTRY_POINT();
   StartupInterface(TSD);

   if (!VersionString)
      goto fastexit;

   if ( VersionString->strlength == 0 )
   {
      if ( ( VersionString->strptr = (char *)IfcAllocateMemory( sizeof(PARSE_VERSION_STRING) ) ) == NULL )
         goto fastexit;
      VersionString->strlength = sizeof(PARSE_VERSION_STRING);
   }

   if ((len = VersionString->strlength) > sizeof(PARSE_VERSION_STRING))
      len = sizeof(PARSE_VERSION_STRING);
   memcpy(VersionString->strptr,PARSE_VERSION_STRING,len);

   /* sizeof includes the terminating 0. Subtract it if we should. */
   if (len > sizeof(PARSE_VERSION_STRING) - 1)
      len = sizeof(PARSE_VERSION_STRING) - 1;
   VersionString->strlength = len;

fastexit:
   low[0] = REGINA_VERSION_MINOR[0];
   if (low[0] == '0') /* atoi may have problems with leading zeros (octal) */
   {
      low[0] = REGINA_VERSION_MINOR[1];
      low[1] = '\0';
   }
   else
      low[1] = REGINA_VERSION_MINOR[1];
   low[2] = '\0';
   return( (atoi(REGINA_VERSION_MAJOR) << 8) | atoi(low) ) ;
}

/*****************************************************************************

    NAME
        APIRET ReginaCleanup()

    SYNOPSIS
        );

    FUNCTION
        Cleans up Regina state associated with the current execution context.

    RESULT
        Zero when no state was released; nonzero when cleanup performed work.

    NOTES
        Deregister external handlers before cleanup. Do not call while the
        interpreter is actively using the state.

    SEE ALSO
        RexxStart(), ReginaVersion()

*****************************************************************************/

EXPORT_C APIRET APIENTRY ReginaCleanup( VOID )
{
   return( IfcReginaCleanup() );
}
