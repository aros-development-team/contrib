
/*
**
**  $VER: misc.c 1.4 (23.6.97)
**  datatypes.library/Examples/DTConvert
**
**  Converts file into another format using datatypes
**
**  Misc functions
**
**  Written 1996/97 by Roland 'Gizzy' Mainz
**
*/

/* project includes */
#include "DTConvert.h"


struct Node *GetNumNode( struct List *list, ULONG num )
{
    struct Node *cur     = NULL;
    ULONG        currnum = 0UL;

    if( list )
    {
      for( cur = list -> lh_Head ; cur -> ln_Succ ; cur = cur -> ln_Succ )
      {
        if( currnum++ == num ) break;
      }

      if( !(cur -> ln_Succ) ) cur = NULL;
    }

    return( cur );
}

#if !defined(__AROS__)
void mysprintf( STRPTR buffer, STRPTR fmt, ... )
{
    APTR args;

    args = (APTR)((&fmt) + 1);

    RawDoFmt( fmt, args, (void (*))"\x16\xc0\x4e\x75", buffer );
}

APTR AllocVecPooled( APTR poolheader, ULONG memsize )
{
    ULONG *memory;

    memsize += sizeof( ULONG );

    if( memory = (ULONG *)AllocPooled( poolheader, memsize ) )
    {
      *memory = memsize;

      memory++;
    }

    return( (APTR)memory );
}


void FreeVecPooled( APTR poolheader, APTR mem )
{
    ULONG *memory;

    if( mem )
    {
      memory = (ULONG *)mem;

      memory--;

      FreePooled( poolheader, memory, (*memory) );
    }
}
#endif

STRPTR GetLockName( BPTR lock, STRPTR name )
{
    ULONG  buffsize  = 64UL;
    STRPTR buff      = NULL;
    LONG   ioerr     = 0L;
    ULONG  namelen   = ((name)?(strlen( name )):(0UL));
      bug("[DTConvert] %s()\n", __func__);

    for( ;; )
    {
      /* Allocate buffer for path, name and '/' */
      if( buff = (STRPTR)AllocVec( (buffsize + namelen + 16UL), (MEMF_PUBLIC | MEMF_CLEAR) ) )
      {
        if( NameFromLock( lock, buff, (buffsize - 1UL) ) )
        {
          break;
        }
        else
        {
          ioerr = IoErr();

          FreeVec( (APTR)buff );
          buff = NULL;

          if( ioerr == ERROR_LINE_TOO_LONG )
          {
            buffsize *= 2UL;
          }
          else
          {
            break;
          }
        }
      }
    }

    if( buff )
    {
      if( name )
      {
        if( !AddPart( buff, name, (buffsize + namelen + 14UL) ) )
        {
          FreeVec( buff );
          buff = NULL;
        }
      }
    }
    else
    {
      SetIoErr( ioerr );
    }

    return( buff );
}


void AttemptOpenLibrary( struct Library **library, STRPTR title, STRPTR libname, ULONG libversion )
{
    struct EasyStruct LibNotFoundES,
                      LibWrongVersionES;
      bug("[DTConvert] %s()\n", __func__);

    LibNotFoundES . es_StructSize   = sizeof( struct EasyStruct );
    LibNotFoundES . es_Flags        = 0UL;
    LibNotFoundES . es_Title        = title;
    LibNotFoundES . es_TextFormat   = "%s\nnot found.";
    LibNotFoundES . es_GadgetFormat = "Retry|Cancel";

    LibWrongVersionES . es_StructSize   = sizeof( struct EasyStruct );
    LibWrongVersionES . es_Flags        = 0UL;
    LibWrongVersionES . es_Title        = title;
    LibWrongVersionES . es_TextFormat   = "Requires at least\n%s version %lu";
    LibWrongVersionES . es_GadgetFormat = "Cancel";

    if( (*library) == NULL )
    {
      for( ;; )
      {
        /* attemp to open shared library */
        (*library) = OpenLibrary( libname, 0UL );

        if( (*library) )
        {
          /* check if we got at least "libversion" version of shared library */
          if( ((*library) -> lib_Version) < libversion )
          {
            (void)EasyRequest( NULL, (&LibWrongVersionES), 0UL, libname, libversion );

            CloseLibrary( (*library) );
            (*library) = NULL;
          }

          break;
        }

        /* prompt the user */
        if( EasyRequest( NULL, (&LibNotFoundES), 0UL, libname ) == 0L )
        {
          /* user canceled */
          break;
        }
      }
    }
}


void ClearMsgPort( struct MsgPort *port )
{
    struct Message *msg;
      bug("[DTConvert] %s()\n", __func__);

    while( msg = GetMsg( port ) )
    {
      ReplyMsg( msg );
    }
}





