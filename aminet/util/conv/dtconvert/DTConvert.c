
/*
**
**  $VER: DTConvert.c 1.8 (20.3.98)
**  datatypes.library/Examples/DTConvert
**
**  Converts file into another format using datatypes
**
**  Main program and startup code
**
**  Written 1996-1998 by Roland 'Gizzy' Mainz
**
*/

/* project includes */
#include "DTConvert.h"
#include "gui.h"

/* version string */
#include "DTConvert_rev.h"

/*****************************************************************************/

/* list of datatypes, used in gadtools listview gadget */
struct DTList
{
    struct List dtl_List;
    UWORD       dtl_NumNodes;
    APTR        dtl_Pool;
};

/* single dtlist node */
struct DTNode
{
    struct Node dtn_Node;
    STRPTR      dtn_DTName;
    /* dynamic bufferspace follows here */
};


/*****************************************************************************/
/* shared libraries */

struct UtilityBase       *UtilityBase    = NULL;
struct GfxBase       *GfxBase        = NULL;
struct IntuitionBase *IntuitionBase  = NULL;
struct Library       *GadToolsBase   = NULL;
struct Library       *DataTypesBase  = NULL;
struct Library       *AslBase        = NULL;
struct Library       *IconBase       = NULL;
struct Library       *WorkbenchBase  = NULL;

/*****************************************************************************/

/* version_string */
STRPTR versionstring = VERSTAG;

/*****************************************************************************/

/* local prototypes */
static                 void                 DefaultSettings( void );
static                 void                 ClearRDA( void );
static                 void                 ScanRDA( void );
static                 void                 ScanToolTypes( TEXT ** );
static                 void                 FreeInitProjectResult( void );
static                 void                 ReadENVPrefs( void );

static                 BOOL                 CreateBasicResources( void );
static                 void                 DeleteBasicResources( void );

static                 BOOL                 OpenLibStuff( void );
static                 void                 CloseLibStuff( void );

static                 void                 RunTool( STRPTR );

static                 BOOL                 NewProject( STRPTR, STRPTR, STRPTR, BOOL, STRPTR );

static                 struct DTList       *CreateDTDescrList( BPTR, ULONG );
static                 void                 FreeDTDescrList( struct DTList * );
static                 struct DTNode       *FindDTNodeByName( struct DTList *, STRPTR );

static                 void                 LockGUI( void );
static                 void                 UnlockGUI( void );

static                 STRPTR               StringSave( STRPTR );
static                 STRPTR               AllocStringBuff( ULONG );
static                 STRPTR               UpdateString( STRPTR *, STRPTR );
static                 void                 FreeString( STRPTR );

static                 struct DiskObject   *GetToolDiskObject( STRPTR );

static                 ULONG                SumString( STRPTR );


/*****************************************************************************/

long main_retval,
     main_retval2;

static struct RDArgs  *startuprda;

/* mempool for strings */
static APTR StringPool;

/*****************************************************************************/

/* template for ReadArgs */
#define STARTUP_TEMPLATE "FROM=NAME=SRCNAME,"          \
                         "DESTDATATYPE=DATATYPE=DTN,"  \
                         "TO=DESTNAME,"                \
                         "GUI/S,"                      \
                         "PUBSCREEN/K"

#define ENV_TEMPLATE STARTUP_TEMPLATE /* equal because there are no /A switches */

static
struct
{
    STRPTR  srcname;
    STRPTR  destdatatype;
    STRPTR  destname;
    long   *gui;
    STRPTR  pubscreen;
} result;

static
struct
{
    STRPTR  srcname;
    STRPTR  destdatatype;
    STRPTR  destname;
    BOOL    gui;
    STRPTR  pubscreen;
} project;

/*****************************************************************************/

/****** DTConvert/DTConvert **************************************************
*
*    NAME
*       DTConvert -- DataTypes-based conversion tool
*
*    FORMAT
*       DTConvert [FROM|NAME|SRCNAME <file>]
*                 [DESTDATATYPE|DATATYPE|DTN <datatype>]
*                 [TO|DESTNAME <file>] [GUI] [PUBSCREEN <public screen>]
*
*    TEMPLATE
*       FROM=NAME=SRCNAME,DESTDATATYPE=DATATYPE=DTN,TO=DESTNAME,GUI/S,
*       PUBSCREEN/K
*
*    PURPOSE
*       Convert data from one format into another format using datatypes
*       classes.
*
*    DESCRIPTION
*       DTConvert converts data from one format into another format,
*       e.g. picture -> picture,
*            animation -> anmation,
*            movie -> movie,
*            sound -> sound,
*            text -> text (n/a)
*
*      You simply have to set the souce file name (SRCNAME), the
*      datatype to convert to (DATATYPE) and the destination file name (TO).
*
*      The datatype option accepts the format names as they are written in
*      DEVS:DataTypes, e.g. if you ant to save a mpeg video stream,
*      you've to set DATATYPE="MPEG Video".
*      If you want to use the base class (to save the incoming data to a 
*      matching IFF format, simply specify the base class name, e.g.
*      "picture" for picture.datatype, "sound" for sound.datatype, 
*      "animation" for animation.datatype and so on.
*
*      If you miss one of the options or if you set the GUI switch, a small
*      GUI will appear where you can set the options.
*      If ou're done with setting all neccesary things in the GUI, the
*      "Convert"-button gets enabled.
*      Selection of the "Convert"-button starts the encoding job...
*
*    BUGS
*      - picture.datatype V43 interface not implemented.
*        This causes that any encoder only runs in V42 mode any may
*        only encode 8 bit data.
*
*      - path names are limitted to 1024 chars, larger filenames may cause
*        mailfunctions. But: Who uses so long path names ?
*
*      - datatypes.library versions V45.3 and V45.4 have a bug in
*        NewDTObjectA which causes that a descriptor is unlocked too often,
*        which results in an Alert( 3500000 ). >= V45.5 fixes this.
*
*    NOTES
*      - picture.datatype V43 pixmap interface not supported.
*
*      - text conversion not supported yet.
*
*      - DTConvert prints a message if a subclass does not
*        implement a local encoder. This was only done to get rid
*        of mails like "your DTConvert has a bug: IFF ILBM -> PNG does
*        not work...". These mails should go to the authors of the
*        datatype classes, not to me. Sorry, but...
*
*    TODO
*      - picture.datatype V43 compatibility
*
*      - text conversion (GID_TEXT -> GID_TEXT), e.g. IFF FTXT <-> Ascii
*
*      - document conversion (GID_DOCUMENT -> GID_DOCUMENT), after
*        Stefan Ruppert and I finsished the document.datatype concepts.
*
*    HISTORY
*       V1.1:
*         - First release to dta@amigawolrd.com mailinglist.
*
*       V1.2:
*         - Added support for animation.datatype subclasses
*
*       V1.3:
*         - Minor fixes.
*
*       V1.4
*         - Added support for sound.datatype V40 subclasses, partial support
*           for suggested sound.datatype V41 interface.
*
*         - Added some usefull comments.
*
*         - WriteAnimClass/WriteSoundClass now sets
*           SetIoErr( ERROR_REQUIRED_ARGUMENT_MISSING ); if something goes
*           wrong (this should abort the encoder; only ERROR_OBJECT_NOT_FOUND
*           is accepted here).
*
*         - Fixed some holes in the error handling.
*
*         - GUI added.
*
*         - Added WB support
*
*         - Added ENV variable support; a local or global (ENV) variable
*           "DTConvert" takes the same arguments as the shell template.
*           Variable settings can be overridden by any argument.
*
*         - Project split in seperate sources (e.g. main, GUI, converters)
*
*       V1.5
*         - Added the feature that the datatypes selection requester shows
*           only entries which match the source group IDs
*           (if a source has been already selected).
*
*         - Added kluge which alows the conversion between GID_MOVIE
*           and GID_ANIMATION datatypes (both are based on
*           animation.datatype, here we have the same interface :-)
*
*         - Fixed the bug that ConvertAnimation function did not deal
*           with truecolor bitmaps (e.g. CyberGFX bitmaps for example,
*           truecolor bitmaps are indicated by ADTA_NumColors == 0.
*           Fixed.
*
*       V1.6
*         - Fixed the bug that ObtainDataTypeA was called for each
*           INTUITICK if the datatype selection requester was open.
*           Fixed.
*
*       V1.7
*         - Fixed the bug that a descriptor was unlocked too often
*           in some cases. See BUGS section above about a matching bug
*           in datatypes.library V45.3 and V45.4 (V45.5 fixes this).
*           Fixed (both "datatypes.library" and "DTConvert").
*
*         - The datatypes requester now accepts ESC as "Cancel" and
*           RETURN as OK.
*
*         - The tool can now be aborted by SIGBREAKF_CTRL_C.
*
*         - Fixed the bug that somtimes an error return code
*           was overwritting by the Message() function.
*
*         - Fixed the bug that the IFF comandline switch did not work
*           (the GUI prompted everytimes in this case). But IFF and
*           GUI switches does currently not co-operate :-(
*
*         - Fixed a bug in the WB startup code which caused that a project
*           without an icon wasn't processed. The code now uses
*           GetDiskObjectNew instead of GetDiskObject to fix this.
*           Fixed.
*
*         - If launched from WB, "DTConvert" now opens a console window
*           manually. Previously, the default WB output channel was used
*           (which was NIL:).
*           Same for project childs.
*
*         - Added output which prints a message if a subclass does not
*           implement a local encoder. This was only done to get rid
*           of mails like "your DTConvert has a bug: IFF ILBM -> PNG does
*           not work...". These mails should go to the authors of the
*           datatype classes, not to me. Sorry, but...
*
*         - If more than one project icon is given at WB startup, "DTConvert"
*           now launches the matching number of projects instead
*           of processing the given icons in sequence.
*
*         - Fixed problems with WB project startup and the current directory.
*
*         - The icon now contains the (possible) options and sets the
*           stack size up to 16384 bytes.
*
*       V1.8
*         - Shame on me. 
*           In the last minutes of the V1.7 release build I
*           decided to make "DTConvert" resident (using cres.o), which
*           bombs out any __saved marked code has used by the animation
*           converter section.
*           Fixed.
*
*         - Completly removed the IFF switch because it was really
*           unneccesary. If you want to save in the base class format,
*           simply specify the name of the base class, e.g.
*           "document" for documents,
*           "text" for texts,
*           "picture" for pictures,
*           "sound" for sample data,
*           "animation" for animation (and movies, due lack of a
*               movie.datatype subclass)
*           and so on...
*           And the same from the GUI.
*
*         - The "no encoder"-message is now avoided if a base class did the
*           encoder job.
*
*         - Added stack checking code. Now "DTConvert" forces the user
*           to use at least 16k stack to et rid of stack related problems...
*
*         - Minor code cleanup.
*
*
*    AUTHOR's REQUEST
*        By  releasing  this program I do  not  place any obligations on you,
*        feel free to share this program with your  friends (and enemies).
*
*        If you want to blame me, report any bugs, or wants a new version
*        send your letter to:
*                        Roland Mainz
*                        Hohenstaufenstraße 8
*                        52388 Nörvenich
*                        GERMANY
*
*        Phone: (+49)(0)2426/901568
*        Fax:   (+49)(0)2426/901569
*
*        EMAIL is also available:
*        GISBURN@w-specht.rhein-ruhr.de
*
*        If you want to send me attachments larger than 1MB (up to 5MB,
*        more with my permission):
*        Up to April 1998 I'm reachable using this email address, too:
*        Reinhold.A.Mainz@KBV.DE
*
*        | Please put your name and address in your mails !
*        | German mailers should add their phone numbers.
*        | See BUGS section above when submitting bug reports.
*
*        Sorry, but I can only look once a week for mails.
*        If you don't hear something from me within three weeks, please
*        send your mail again (but watch about new releases) (problems with
*        this email port are caused by reconfigurations, hackers, network
*        problems etc.).
*
*        The  entire  "DTConvert"  package may  be  noncommercially
*        redistributed, provided  that  the package  is always  distributed
*        in it's complete  form (including it's documentation).  A small copy
*        fee for media costs is okay but any kind of commercial distribution
*        is strictly forbidden! Comments  and  suggestions  how  to  improve
*        this program  are generally appreciated!
*
*        Thanks to Matt Dillon for his DICE, David Junod for this datatypes
*        environment and Olaf 'Olsen' Barthel for his help, ideas and some
*        text clips from his documentations.
*
*    SEE ALSO
*
******************************************************************************
*
*/


#ifdef _DCC
/* disable CTRL_C break support (DICE CTRL_C abort function) */
void chkabort( void )
{
}


/* DICE workbench entry */
long wbmain( struct WBStartup *wbstartup )
{
    /* Call main like SAS-C */
    return( (int)main( 0L, (STRPTR *)wbstartup ) );
}
#endif /* _DCC */


int main( int ac, STRPTR *av )
{
    LONG               numArgs,
                       x;
    struct WBStartup  *wbstartup;
    struct WBArg      *wbarg;
    struct DiskObject *tooldobj,
                      *projectdobj;
    BPTR               oldToolLock    = NULL,
                       oldProjectLock = NULL;
    BPTR               oldOutput      = NULL;

  D(bug("[DTConvert] %s()\n", __func__);)

    main_retval2 = 0L;
    main_retval  = RETURN_OK;

    if( CreateBasicResources() )
    {
      DefaultSettings();

      /* check if something goes wrong (ENV error etc.) */
      if( main_retval == RETURN_OK )
      {
/* Workbench */
        if( ac == 0L )
        {
          BPTR newOutput;

          /* Add "stdout" for Message() function...  */
          if( newOutput = Open( "CON:////" NAME "/auto/wait/close", MODE_READWRITE ) )
          {
            oldOutput = SelectOutput( newOutput );
          }

          wbstartup = (struct WBStartup *)av;

          numArgs = wbstartup -> sm_NumArgs;
          wbarg   = wbstartup -> sm_ArgList;

          if( *(wbarg[ 0 ] . wa_Name) )
          {
            if( wbarg[ 0 ] . wa_Lock )
            {
              oldToolLock = CurrentDir( (wbarg[ 0 ] . wa_Lock) );
            }

            if( tooldobj = GetToolDiskObject( (wbarg[ 0 ] . wa_Name) ) )
            {
              /* two possible cases when started from workbench ... */
              if( numArgs < 2L )
              {
                /* ... first case, only our tool icon is given, create one project here */

                ScanToolTypes( (STRPTR *)(tooldobj -> do_ToolTypes) );

                /* check if something goes wrong (parsing error, etc.) */
                if( main_retval == RETURN_OK )
                {
                  RunTool( NULL );
                }

                FreeInitProjectResult();
              }
              else
              {
                /* ... second case, a couple of project icons are given, multiple projects will start from here */
                for( x = 1L ; x < numArgs ; x++ )
                {
                  if( wbarg[ x ] . wa_Lock )
                  {
                    oldProjectLock = CurrentDir( (wbarg[ x ] . wa_Lock) );
                  }

                  if( *(wbarg[ x ] . wa_Name) )
                  {
                    if( projectdobj = GetDiskObjectNew( (wbarg[ x ] . wa_Name) ) )
                    {
                      ScanToolTypes( (STRPTR *)(tooldobj -> do_ToolTypes) );
                      ScanToolTypes( (STRPTR *)(projectdobj -> do_ToolTypes) );

                      /* check if something goes wrong (parsing error, etc.) */
                      if( main_retval == RETURN_OK )
                      {
                        NewProject( ((project . srcname)?(project . srcname):((STRPTR)(wbarg[ x ] . wa_Name))),
                                    (project . destdatatype),
                                    (project . destname),
                                    TRUE,
                                    (project . pubscreen) );
                      }

                      FreeInitProjectResult();
                      DefaultSettings();

                      FreeDiskObject( projectdobj );
                    }
                  }

                  if( wbarg[ x ] . wa_Lock )
                  {
                    CurrentDir( oldProjectLock );
                  }
                }
              }

              FreeDiskObject( tooldobj );
            }

            if( wbarg[ 0 ] . wa_Lock )
            {
              CurrentDir( oldToolLock );
            }
          }
        }
        else
        {
/* CLI/Shell */
          if( startuprda = ReadArgs( STARTUP_TEMPLATE, (SIPTR *)(&result), NULL ) )
          {
            /* did we get a CTRL_C signal ? */
            if( !CheckSignal( SIGBREAKF_CTRL_C ) )
            {
              ScanRDA();

              /* check if something goes wrong (parsing error, etc.) */
              if( main_retval == RETURN_OK )
              {
                RunTool( NULL );
              }

              FreeInitProjectResult();
            }
            else
            {
              main_retval2 = ERROR_BREAK;
              main_retval  = RETURN_WARN;
            }

            FreeArgs( startuprda );
          }
          else
          {
            main_retval2 = IoErr();
            main_retval  = RETURN_FAIL;
          }
        }
      }

      DeleteBasicResources();
    }

    PrintFault( main_retval2, NAME );

    /* Close WB stdout... */
    if( oldOutput )
    {
      BPTR newOutput;

      newOutput = SelectOutput( oldOutput );

      (void)Close( newOutput );
    }

    SetIoErr( main_retval2 );

    return( main_retval );
}


static
void DefaultSettings( void )
{
  D(bug("[DTConvert] %s()\n", __func__);)

    ClearRDA();

    memset( (void *)(&project), 0, sizeof( project ) );

    ReadENVPrefs();
}


static
void ReadENVPrefs( void )
{
    struct RDArgs envvarrda =
    {
      NULL,
      256L,
      0L,
      0L,
      NULL,
      0L,
      NULL,
      RDAF_NOPROMPT
    };

    TEXT varbuff[ 258 ];

  D(bug("[DTConvert] %s()\n", __func__);)

    envvarrda . RDA_Source . CS_Buffer = varbuff;

    if( GetVar( NAME, varbuff, 256L, 0UL ) != (-1L) )
    {
      strcat( varbuff, "\n" );

      if( ReadArgs( ENV_TEMPLATE, (SIPTR *)(&result), (&envvarrda) ) )
      {
        ScanRDA();

        FreeArgs( (&envvarrda) );
      }
      else
      {
        main_retval2 = IoErr();
        main_retval  = RETURN_FAIL;
      }
    }
}


static
void ClearRDA( void )
{
  D(bug("[DTConvert] %s()\n", __func__);)
    memset( (void *)(&result), 0, sizeof( result ) );
}


static
void ScanRDA( void )
{
  D(bug("[DTConvert] %s()\n", __func__);)
    if( result . srcname )
    {
      UpdateString( (&(project . srcname)), (result . srcname) );
    }

    if( result . destdatatype )
    {
      UpdateString( (&(project . destdatatype)), (result . destdatatype) );
    }

    if( result . destname )
    {
      UpdateString( (&(project . destname)), (result . destname) );
    }

    if( result . gui )
    {
      project . gui = TRUE;
    }

    if( result . pubscreen )
    {
      UpdateString( (&(project . pubscreen)), (result . pubscreen) );
    }

    ClearRDA();
}


static
void ScanToolTypes( TEXT **tt )
{
    STRPTR s;

  D(bug("[DTConvert] %s()\n", __func__);)

    if( s = FindToolType( tt, "FROM" ) )
      result . srcname = s;

    if( s = FindToolType( tt, "NAME" ) )
      result . srcname = s;

    if( s = FindToolType( tt, "SRCNAME" ) )
      result . srcname = s;

    if( s = FindToolType( tt, "DESTDATATYPE" ) )
      result . destdatatype = s;

    if( s = FindToolType( tt, "DATATYPE" ) )
      result . destdatatype = s;

    if( s = FindToolType( tt, "DTN" ) )
      result . destdatatype = s;

    if( s = FindToolType( tt, "TO" ) )
      result . destname = s;

    if( s = FindToolType( tt, "DESTNAME" ) )
      result . destname = s;

    if( s = FindToolType( tt, "GUI" ) )
      result . gui = (long *)s;

    if( s = FindToolType( tt, "PUBSCREEN" ) )
      result . pubscreen = s;

    ScanRDA();
}


static
void FreeInitProjectResult( void )
{
  D(bug("[DTConvert] %s()\n", __func__);)

    FreeString( (project . srcname) );
    FreeString( (project . destdatatype) );
    FreeString( (project . destname) );
    FreeString( (project . pubscreen) );
}


static
BOOL CreateBasicResources( void )
{
  D(bug("[DTConvert] %s()\n", __func__);)

    if( OpenLibStuff() )
    {
      if( StringPool = CreatePool( (MEMF_PUBLIC | MEMF_CLEAR), 128UL, 128UL ) )
      {
        return( TRUE );

#ifdef COMMENTED_OUT
        DeletePool( StringPool );
#endif /* COMMENTED_OUT */
      }

      CloseLibStuff();
    }

    return( FALSE );
}


static
void DeleteBasicResources( void )
{
  D(bug("[DTConvert] %s()\n", __func__);)

    DeletePool( StringPool );
    CloseLibStuff();
}


static
BOOL OpenLibStuff( void )
{
  D(bug("[DTConvert] %s()\n", __func__);)

    AttemptOpenLibrary( (struct Library **)(&UtilityBase),       NAME, "utility.library",     39UL );
    AttemptOpenLibrary( (struct Library **)(&(GfxBase)),         NAME, "graphics.library",    39UL );
    AttemptOpenLibrary( (struct Library **)(&(IntuitionBase)),   NAME, "intuition.library",   39UL );
    AttemptOpenLibrary( (&GadToolsBase),                         NAME, "gadtools.library",    39UL );
    AttemptOpenLibrary( (&DataTypesBase),                        NAME, "datatypes.library",   45UL );
    AttemptOpenLibrary( (&IconBase),                             NAME, "icon.library",        39UL );
    AttemptOpenLibrary( (&WorkbenchBase),                        NAME, "workbench.library",   39UL );
    AttemptOpenLibrary( (&AslBase),                              NAME, "asl.library",         38UL );

    if( UtilityBase && GfxBase && IntuitionBase && GadToolsBase && DataTypesBase && IconBase && WorkbenchBase && AslBase  )
    {
      return( TRUE );
    }

    CloseLibStuff();

    return( FALSE );
}


static
void CloseLibStuff( void )
{
  D(bug("[DTConvert] %s()\n", __func__);)

    CloseLibrary( AslBase );
    CloseLibrary( WorkbenchBase );
    CloseLibrary( IconBase );
    CloseLibrary( DataTypesBase );
    CloseLibrary( GadToolsBase );
    CloseLibrary( (&(IntuitionBase -> LibNode)) );
    CloseLibrary( (&(GfxBase -> LibNode)) );
    CloseLibrary( (struct Library *)UtilityBase );
}


static
void RunTool( STRPTR srcname )
{
    struct Task *ThisTask  = FindTask( NULL );
    ULONG        stacksize = (ULONG)(((UBYTE *)(ThisTask -> tc_SPReg)) - ((UBYTE *)(ThisTask -> tc_SPLower)));

  D(bug("[DTConvert] %s()\n", __func__);)

    /* Enougth stack ? */
    if( stacksize >= 8192UL )
    {
      STRPTR destname     = project . destname;
      STRPTR datatypename = project . destdatatype;
      BOOL   srcchanged   = FALSE;      /* srcname changed ? */
      ULONG  srcsum       = SumString( srcname );

      if( project . srcname )
      {
        srcname = project . srcname;
      }

      /* Check if datatype name is valid */
      if( datatypename )
      {
        struct DataType *dtn;

        if( dtn = ObtainDataTypeA( DTST_RAM, (APTR)datatypename, NULL ) )
        {
          ReleaseDataType( dtn );
        }
        else
        {
          datatypename = NULL;
        }
          D(bug("[DTConvert] %s: datatypename = '%s'\n", __func__, datatypename);)
      }

      /* GUI requested / required (some ares are missing and the user should fill them in) ? */
      if( (project . gui) || (srcname == NULL) || (destname == NULL) || (datatypename == NULL) )
      {
        struct MsgPort *appport;
        struct DTList  *dtl;
        struct DTNode  *selecteddt;

        if( appport = CreateMsgPort() )
        {
          BPTR srclock = NULL;

          if( Strlen( srcname ) )
          {
            srclock = Lock( srcname, SHARED_LOCK );
          }

          if( dtl = CreateDTDescrList( srclock, 0UL ) )
          {
            PubScreenName = project . pubscreen; /* set pubscreen name for GadToolsBox's windows */

            selecteddt = FindDTNodeByName( dtl, datatypename );

            if( !SetupScreen() )
            {
              struct FileRequester *SrcFileReq  = NULL,
                                   *DestFileReq = NULL;

              if( !OpenDTConvert_MAINWindow() )
              {
                BOOL                 done             = FALSE;
                struct IntuiMessage *imsg;
                ULONG                sigmask;
                BOOL                 get_source       = FALSE,
                                     get_datatype     = FALSE,
                                     get_destination  = FALSE;
                BOOL                 convert          = FALSE;
                BOOL                 convertable      = Strlen( srcname ) && selecteddt && Strlen( destname );
                struct AppWindow    *appwindow        = NULL;
                struct DiskObject   *appdobj          = NULL;
                struct AppIcon      *appicon          = NULL;

                /* Running on WB screen ? */
                if( ((DTConvert_MAINWnd -> WScreen -> Flags) & SCREENTYPE) == WBENCHSCREEN )
                {
                  appwindow = AddAppWindowA( 0UL, 0UL, DTConvert_MAINWnd, appport, NULL );
                }
                else
                {
                  if( appdobj = GetDefDiskObject( WBPROJECT ) )
                  {
                    /* Not running on workbench screen ? - Create AppIcon for wb app support ! */
                    appicon = AddAppIconA( 0UL, 0UL, NAME, appport, NULL, appdobj, NULL );
                  }
                }

                GT_SetGadgetAttrs( DTConvert_MAINGadgets[ GD_SOURCE ],      DTConvert_MAINWnd, NULL, GTST_String, srcname,                                                  TAG_DONE );
                GT_SetGadgetAttrs( DTConvert_MAINGadgets[ GD_DATATYPE ],    DTConvert_MAINWnd, NULL, GTTX_Text,   ((selecteddt)?(selecteddt -> dtn_Node . ln_Name):(NULL)), TAG_DONE );
                GT_SetGadgetAttrs( DTConvert_MAINGadgets[ GD_DESTINATION ], DTConvert_MAINWnd, NULL, GTST_String, destname,                                                 TAG_DONE );
                GT_SetGadgetAttrs( DTConvert_MAINGadgets[ GD_CONVERT ],     DTConvert_MAINWnd, NULL, GA_Disabled, (!convertable),                                           TAG_DONE );

                /*******************
                 * Main event loop *
                 *******************/
                while( !done )
                {
                  ULONG sigr;

                  sigmask = (1UL << (DTConvert_MAINWnd -> UserPort -> mp_SigBit)) |
                            (1UL << (appport -> mp_SigBit))                       |
                            SIGBREAKF_CTRL_C;

                  if( DTConvert_DATATYPEWnd )
                  {
                    sigmask |= (1UL << (DTConvert_DATATYPEWnd -> UserPort -> mp_SigBit));
                  }

                  sigr = Wait( sigmask );

/* Main window */
                  if( sigr & (1UL << (DTConvert_MAINWnd -> UserPort -> mp_SigBit)) )
                  {
                    while( imsg = GT_GetIMsg( (DTConvert_MAINWnd -> UserPort) ) )
                    {
                      switch( imsg -> Class )
                      {
                        case IDCMP_CLOSEWINDOW:
                        {
                            done = TRUE;
                        }
                            break;

                        case IDCMP_REFRESHWINDOW:
                        {
                            GT_BeginRefresh( (imsg -> IDCMPWindow) );
                            GT_EndRefresh( (imsg -> IDCMPWindow), TRUE );
                        }
                            break;

                        case IDCMP_GADGETUP:
                        {
                            switch( G( (imsg -> IAddress) ) -> GadgetID )
                            {
                              case GD_SOURCE: break;   /* NOP */

                              case GD_DATATYPE: break; /* NOP */

                              case GD_DESTINATION: break; /* NOP */

                              case GD_GET_SOURCE:
                              {
                                  get_source = TRUE;
                              }
                                  break;

                              case GD_GET_DATATYPE:
                              {
                                  get_datatype = TRUE;
                              }
                                  break;

                              case GD_GET_DESTINATION:
                              {
                                  get_destination = TRUE;
                              }
                                  break;

                              case GD_CONVERT:
                              {
                                  convert = TRUE;
                              }
                                  break;

                              case GD_CANCEL:
                              {
                                  done = TRUE;
                              }
                                  break;
                            }
                        }
                            break;

                        case IDCMP_VANILLAKEY:
                        {
                            switch( imsg -> Code )
                            {
                              case 's': /* Source */
                                  ActivateGadget( DTConvert_MAINGadgets[ GD_SOURCE ], DTConvert_MAINWnd, NULL );
                                  break;

                              case 'S': /* Get Source */
                                  get_source = TRUE;
                                  break;

                              case 't': /* datatype */
                              case 'T': /* datatype */
                                  get_datatype = TRUE;
                                  break;

                              case 'd': /* Destination */
                                  ActivateGadget( DTConvert_MAINGadgets[ GD_DESTINATION ], DTConvert_MAINWnd, NULL );
                                  break;

                              case 'D': /* Get Destination */
                                  get_destination = TRUE;
                                  break;

                              case 'c': /* Convert */
                              case 'C': /* Convert */
                              {
                                  convert = TRUE;
                              }
                                  break;

                              case 'a': /* Cancel */
                              case 'A': /* Cancel */
                              {
                                  done = TRUE;
                              }
                                  break;
                            }
                        }
                            break;
                      }

                      GT_ReplyIMsg( imsg );
                    }
                  }

/* DataTypes requester */
                  if( DTConvert_DATATYPEWnd )
                  {
                    if( sigr & (1UL << (DTConvert_DATATYPEWnd -> UserPort -> mp_SigBit)) )
                    {
                      BOOL  dtdone = FALSE;
                      BOOL  dtok   = FALSE;
                      LONG  oldselected,
                            selected;

                      GT_GetGadgetAttrs( DTConvert_DATATYPEGadgets[ GD_DATATYPEDT ], DTConvert_DATATYPEWnd, NULL,
                                         GTLV_Selected, (&oldselected),
                                         TAG_DONE );

                      selected = oldselected;

                      while( imsg = GT_GetIMsg( (DTConvert_DATATYPEWnd -> UserPort) ) )
                      {
                        switch( imsg -> Class )
                        {
                          case IDCMP_CLOSEWINDOW:
                          {
                              dtdone = TRUE;
                          }
                              break;

                          case IDCMP_REFRESHWINDOW:
                          {
                              GT_BeginRefresh( (imsg -> IDCMPWindow) );
                              GT_EndRefresh( (imsg -> IDCMPWindow), TRUE );
                          }
                              break;

                          case IDCMP_GADGETUP:
                          {
                              switch( G( (imsg -> IAddress) ) -> GadgetID )
                              {
                                case GD_DATATYPEDT: break; /* NOP */

                                case GD_OKDT:
                                {
                                    dtdone = dtok = TRUE;
                                }
                                    break;

                                case GD_CANCELDT:
                                {
                                    dtdone = TRUE;
                                }
                                    break;
                              }
                          }
                              break;

                          case IDCMP_VANILLAKEY:
                          {
                              switch( imsg -> Code )
                              {
                                case 'o':  /* Ok */
                                case 'O':  /* Ok */
                                case '\r': /* Ok */
                                    dtdone = dtok = TRUE;
                                    break;

                                case 'c': /* Cancel */
                                case 'C': /* Cancel */
                                case  27: /* ESC == Cancel */
                                    dtdone = TRUE;
                                    break;

                                case 'd': /* DT Selection down */
                                {
                                    selected++;
                                }
                                    break;

                                case 'D': /* DT Selection up */
                                {
                                    selected--;
                                }
                                    break;
                              }
                          }
                              break;

                          case IDCMP_RAWKEY:
                          {
                              switch( imsg -> Code )
                              {
                                case 76: /* up */
                                {
                                    if( (imsg -> Qualifier) & (IEQUALIFIER_LALT | IEQUALIFIER_RALT) )
                                    {
                                      selected = 0L; /* top */
                                    }
                                    else
                                    {
                                      if( (imsg -> Qualifier) & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT) )
                                      {
                                        selected -= 5L; /* page up */
                                      }
                                      else
                                      {
                                        selected--; /* previous entry */
                                      }
                                    }
                                }
                                    break;

                                case 77: /* down */
                                {
                                    if( (imsg -> Qualifier) & (IEQUALIFIER_LALT | IEQUALIFIER_RALT) )
                                    {
                                      selected = dtl -> dtl_NumNodes; /* bottom */
                                    }
                                    else
                                    {
                                      if( (imsg -> Qualifier) & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT) )
                                      {
                                        selected += 5L; /* page down */
                                      }
                                      else
                                      {
                                        selected++; /* next entry */
                                      }
                                    }
                                }
                                    break;
                              }
                          }
                              break;
                        }

                        /* Check bounds */
                        if( selected < 0L )
                        {
                          selected = 0L;
                        }
                        else
                        {
                          if( selected > (LONG)(dtl -> dtl_NumNodes) )
                          {
                            selected = (LONG)(dtl -> dtl_NumNodes);
                          }
                        }

                        GT_ReplyIMsg( imsg );
                      }

                      if( selected != oldselected )
                      {
                        GT_SetGadgetAttrs( DTConvert_DATATYPEGadgets[ GD_DATATYPEDT ], DTConvert_DATATYPEWnd, NULL,
                                           GTLV_Selected,    selected,
                                           GTLV_MakeVisible, selected,
                                           TAG_DONE );
                      }

                      if( dtok )
                      {
                        if( selecteddt = (struct DTNode *)GetNumNode( (&(dtl -> dtl_List)), selected ) )
                        {
                          GT_SetGadgetAttrs( DTConvert_MAINGadgets[ GD_DATATYPE ], DTConvert_MAINWnd, NULL,
                                             GTTX_Text, (selecteddt -> dtn_Node . ln_Name),
                                             TAG_DONE );
                        }
                      }

                      if( dtdone )
                      {
                        CloseDTConvert_DATATYPEWindow();
                      }
                    }
                  }

/* appwindow / appicon port */
                  if( sigr & (1UL << (appport -> mp_SigBit)) )
                  {
                    struct AppMessage *amsg;

                    while( amsg = (struct AppMessage *)GetMsg( appport ) )
                    {
                      switch( amsg -> am_Type )
                      {
                        case AMTYPE_APPWINDOW:
                        case AMTYPE_APPICON:
                        {
                            if( amsg -> am_NumArgs )
                            {
                              LockGUI();

                                if( (amsg -> am_NumArgs) == 1UL )
                                {
                                  STRPTR name;

                                  if( name = GetLockName( (amsg -> am_ArgList[ 0 ] . wa_Lock), (amsg -> am_ArgList[ 0 ] . wa_Name) ) )
                                  {
                                    GT_SetGadgetAttrs( DTConvert_MAINGadgets[ GD_SOURCE ], DTConvert_MAINWnd, NULL, GTST_String, name, TAG_DONE );

                                    FreeVec( name );
                                  }
                                }
                                else
                                {
                                  ULONG i;

                                  for( i = 0UL ; i < (amsg -> am_NumArgs) ; i++ )
                                  {
                                    BPTR oldProjectLock = NULL;

                                    if( amsg -> am_ArgList[ i ] . wa_Lock )
                                    {
                                      oldProjectLock = CurrentDir( (amsg -> am_ArgList[ i ] . wa_Lock) );
                                    }

                                    NewProject( (amsg -> am_ArgList[ 0 ] . wa_Name), ((selecteddt)?(selecteddt -> dtn_DTName):(NULL)), destname, TRUE, (project . pubscreen) );

                                    if( amsg -> am_ArgList[ i ] . wa_Lock )
                                    {
                                      (void)CurrentDir( oldProjectLock );
                                    }
                                  }
                                }

                              UnlockGUI();
                            }
                        }
                            break;
                      }

                      ReplyMsg( (&(amsg -> am_Message)) );
                    }
                  }

                  /* Quit signal ? */
                  if( sigr & SIGBREAKF_CTRL_C )
                  {
                    done = TRUE;
                  }

/* process collected events */
                  if( get_source || get_datatype || get_destination )
                  {
                    LockGUI();

                      if( get_source )
                      {
                        if( SrcFileReq == NULL )
                        {
                          SrcFileReq = (struct FileRequester *)AllocAslRequestTags( ASL_FileRequest,
                                                                                    ASLFR_Screen,          (DTConvert_MAINWnd -> WScreen),
                                                                                    ASLFR_TitleText,       "Open File...",
                                                                                    ASLFR_InitialLeftEdge, ((DTConvert_MAINWnd -> LeftEdge) + (DTConvert_MAINWnd -> BorderLeft) + 1UL),
                                                                                    ASLFR_InitialTopEdge,  ((DTConvert_MAINWnd -> TopEdge)  + (DTConvert_MAINWnd -> BorderTop) + 1UL),
                                                                                    ASLFR_DoPatterns,      TRUE,
                                                                                    TAG_DONE );
                        }

                        if( SrcFileReq )
                        {
                          if( AslRequest( SrcFileReq, NULL ) )
                          {
                            ULONG  buffsize;
                            STRPTR filebuffer;

                            buffsize = Strlen( (SrcFileReq -> fr_Drawer) ) + Strlen( (SrcFileReq -> fr_File) ) + 10UL;

                            if( filebuffer = (STRPTR)AllocVec( buffsize, MEMF_PUBLIC ) )
                            {
                              strcpy( filebuffer, (SrcFileReq -> fr_Drawer) );
                              AddPart( filebuffer, (SrcFileReq -> fr_File), buffsize );

                              GT_SetGadgetAttrs( DTConvert_MAINGadgets[ GD_SOURCE ], DTConvert_MAINWnd, NULL, GTST_String, filebuffer, TAG_DONE );

                              FreeVec( filebuffer );
                            }
                          }
                        }

                        get_source = FALSE;
                      }

                      if( get_datatype )
                      {
                        if( DTConvert_DATATYPEWnd == NULL )
                        {
                          if( !OpenDTConvert_DATATYPEWindow() )
                          {
                            GT_SetGadgetAttrs( DTConvert_DATATYPEGadgets[ GD_DATATYPEDT ], DTConvert_DATATYPEWnd, NULL, GTLV_Labels, dtl, TAG_DONE );
                          }
                        }

                        get_datatype = FALSE;
                      }

                      if( get_destination )
                      {
                        if( DestFileReq == NULL )
                        {
                          DestFileReq = (struct FileRequester *)AllocAslRequestTags( ASL_FileRequest,
                                                                                     ASLFR_Screen,          (DTConvert_MAINWnd -> WScreen),
                                                                                     ASLFR_TitleText,       "Save File...",
                                                                                     ASLFR_InitialLeftEdge, ((DTConvert_MAINWnd -> LeftEdge) + (DTConvert_MAINWnd -> BorderLeft) + 1UL),
                                                                                     ASLFR_InitialTopEdge,  ((DTConvert_MAINWnd -> TopEdge)  + (DTConvert_MAINWnd -> BorderTop) + 1UL),
                                                                                     ASLFR_DoPatterns,      TRUE,
                                                                                     ASLFR_DoSaveMode,      TRUE,
                                                                                     TAG_DONE );
                        }

                        if( DestFileReq )
                        {
                          if( AslRequest( DestFileReq, NULL ) )
                          {
                            ULONG  buffsize;
                            STRPTR filebuffer;

                            buffsize = Strlen( (DestFileReq -> fr_Drawer) ) + Strlen( (DestFileReq -> fr_File) ) + 10UL;

                            if( filebuffer = (STRPTR)AllocVec( buffsize, MEMF_PUBLIC ) )
                            {
                              strcpy( filebuffer, (DestFileReq -> fr_Drawer) );
                              AddPart( filebuffer, (DestFileReq -> fr_File), buffsize );

                              GT_SetGadgetAttrs( DTConvert_MAINGadgets[ GD_DESTINATION ], DTConvert_MAINWnd, NULL, GTST_String, filebuffer, TAG_DONE );

                              FreeVec( filebuffer );
                            }
                          }
                        }

                        get_destination = FALSE;
                      }

                    UnlockGUI();
                  }

/* update */
                  GT_GetGadgetAttrs( DTConvert_MAINGadgets[ GD_SOURCE ], DTConvert_MAINWnd, NULL,
                                     GTST_String, (&srcname),
                                     TAG_DONE );


                  /* My quick hack to check if the src filename has been changed... */
                  srcchanged = (SumString( srcname ) != srcsum);

                  /* following block is experimental...  */
                  if( srcchanged )
                  {
                    struct DTList *old_dtl = dtl;

                    /* Update checksum... */
                    srcsum = SumString( srcname );

                    if( srclock )
                    {
                      UnLock( srclock );
                      srclock = NULL;
                    }

                    if( Strlen( srcname ) )
                    {
                      srclock = Lock( srcname, SHARED_LOCK );
                    }

                    dtl = CreateDTDescrList( srclock, 0UL );

                    if( selecteddt )
                    {
                      selecteddt = FindDTNodeByName( dtl, (selecteddt -> dtn_DTName) );

                      GT_SetGadgetAttrs( DTConvert_MAINGadgets[ GD_DATATYPE ], DTConvert_MAINWnd, NULL,
                                         GTTX_Text, ((selecteddt)?(selecteddt -> dtn_Node . ln_Name):("")),
                                         TAG_DONE );
                    }

                    /* update DataTypes selection gadget */
                    if( DTConvert_DATATYPEWnd )
                    {
                      GT_SetGadgetAttrs( DTConvert_DATATYPEGadgets[ GD_DATATYPEDT ], DTConvert_DATATYPEWnd, NULL, GTLV_Labels, dtl, TAG_DONE );
                    }

                    FreeDTDescrList( old_dtl );
                  }

                  GT_GetGadgetAttrs( DTConvert_MAINGadgets[ GD_DESTINATION ], DTConvert_MAINWnd, NULL,
                                     GTST_String, (&destname),
                                     TAG_DONE );

                  convertable = Strlen( srcname ) && selecteddt && Strlen( destname );

                  GT_SetGadgetAttrs( DTConvert_MAINGadgets[ GD_CONVERT ], DTConvert_MAINWnd, NULL,
                                     GA_Disabled, (!convertable),
                                     TAG_DONE );

/* run converter ? */
                  if( convert )
                  {
                    if( convertable )
                    {
                      LockGUI();

                        Message( "converting '%s' '%s' '%s'\n", srcname, (selecteddt -> dtn_DTName), destname );

                        Convert( srcname, (selecteddt -> dtn_DTName), destname );

                      UnlockGUI();
                    }

                    convert = FALSE;
                  }
                }

                /**********
                 * Done ! *
                 **********/
                if( appwindow )
                {
                  (void)RemoveAppWindow( appwindow );
                }

                if( appdobj )
                {
                  FreeDiskObject( appdobj );
                }

                if( appicon )
                {
                  (void)RemoveAppIcon( appicon );
                }

                CloseDTConvert_DATATYPEWindow();
                CloseDTConvert_MAINWindow();
              }

              FreeAslRequest( SrcFileReq );
              FreeAslRequest( DestFileReq );

              CloseDownScreen();
            }

            FreeDTDescrList( dtl );
          }

          if( srclock )
          {
            UnLock( srclock );
          }

          ClearMsgPort( appport );
          DeleteMsgPort( appport );
        }
      }
      else
      {
        Convert( srcname, datatypename, destname );
      }
    }
    else
    {
      Message( NAME " requires at least a stack size of 16384 bytes\n" );
      SetIoErr( ERROR_NO_FREE_STORE );
    }

    if( IoErr() )
    {
      main_retval2 = IoErr();
      main_retval  = RETURN_ERROR;
    }
}


static
BOOL NewProject( STRPTR from, STRPTR datatype, STRPTR destname, BOOL gui, STRPTR pubscreen )
{
    struct Process              *ThisProc = (struct Process *)FindTask( NULL );
    BPTR                         olddir    = CurrentDir( (ThisProc -> pr_HomeDir) );
    BOOL                         success  = FALSE;
    STRPTR                       buffer;
    ULONG                        buffersize;
    struct CommandLineInterface *cli = Cli();

  D(bug("[DTConvert] %s()\n", __func__);)

    if( from = GetLockName( olddir, from ) )
    {
      buffersize = Strlen( from ) + Strlen( datatype ) + Strlen( destname ) + Strlen( pubscreen ) + 512UL;

      if( buffer = (STRPTR)AllocVec( buffersize, MEMF_PUBLIC ) )
      {
        STRPTR next = buffer;

        if( cli )
        {
          STRPTR s = (STRPTR)BADDR( (cli -> cli_CommandName) );

          stccpy( next, (&s[ 1 ]), (int)(s[ 0 ] + 1U) );
        }
        else
        {
          strcpy( next, (ThisProc -> pr_Task . tc_Node . ln_Name) );
        }

        next += strlen( next );

        if( Strlen( from ) )
        {
          mysprintf( next, " FROM=\"%s\"", from );
          next += strlen( next );
        }

        if( Strlen( datatype ) )
        {
          mysprintf( next, " DATATYPE=\"%s\"", datatype );
          next += strlen( next );
        }

        if( Strlen( destname ) )
        {
          mysprintf( next, " TO=\"%s\"", destname );
          next += strlen( next );
        }

        if( gui )
        {
          mysprintf( next, " GUI" );
          next += strlen( next );
        }

        if( Strlen( pubscreen ) )
        {
          mysprintf( next, " PUBSCREEN=\"%s\"", pubscreen );
        }

        if( SystemTags( buffer,
                        SYS_Input,     Open( "CON:////" NAME "/auto/wait/close", MODE_READWRITE ),
                        SYS_Asynch,    TRUE,
                        TAG_DONE ) != (-1L) )
        {
          success = TRUE;
        }

        FreeVec( buffer );
      }
      
      FreeVec( from );
    }

    (void)CurrentDir( olddir );

    return( success );
}


/* GUI locking stuff */
static ULONG            GUILock                = 0UL;
static struct Requester MAINBlockReq           = { 0 };
static BOOL             MAINBlockReqActive     = FALSE;
static struct Requester DATATYPEBlockReq       = { 0 };
static BOOL             DATATYPEBlockReqActive = FALSE;


static
void LockGUI( void )
{
  D(bug("[DTConvert] %s()\n", __func__);)

    if( GUILock++ == 0UL )
    {
      InitRequester( (&MAINBlockReq) );
      InitRequester( (&DATATYPEBlockReq) );

      if( DTConvert_MAINWnd )
      {
        SetWindowPointer( DTConvert_MAINWnd, WA_BusyPointer, TRUE, WA_PointerDelay, TRUE, TAG_DONE );

        if( MAINBlockReqActive == FALSE )
        {
          MAINBlockReqActive = Request( (&MAINBlockReq), DTConvert_MAINWnd );
        }
      }

      if( DTConvert_DATATYPEWnd )
      {
        SetWindowPointer( DTConvert_DATATYPEWnd, WA_BusyPointer, TRUE,  WA_PointerDelay, TRUE, TAG_DONE );

        if( DATATYPEBlockReqActive == FALSE )
        {
          DATATYPEBlockReqActive = Request( (&DATATYPEBlockReq), DTConvert_DATATYPEWnd );
        }
      }
    }
}


static
void UnlockGUI( void )
{
  D(bug("[DTConvert] %s()\n", __func__);)

    if( --GUILock == 0UL )
    {
      if( DTConvert_MAINWnd )
      {
        SetWindowPointer( DTConvert_MAINWnd, WA_BusyPointer, FALSE, TAG_DONE );

        if( MAINBlockReqActive )
        {
          EndRequest( (&MAINBlockReq), DTConvert_MAINWnd );
          MAINBlockReqActive = FALSE;
        }
      }

      if( DTConvert_DATATYPEWnd )
      {
        SetWindowPointer( DTConvert_DATATYPEWnd, WA_BusyPointer, FALSE, TAG_DONE );

        if( DATATYPEBlockReqActive )
        {
          EndRequest( (&DATATYPEBlockReq), DTConvert_DATATYPEWnd );
          DATATYPEBlockReqActive = FALSE;
        }
      }
    }
}


void Message( STRPTR fmt, ... )
{
#if (0)
    VPrintf( fmt, (APTR)((&fmt) + 1) );
#endif
}


static
struct DTList *CreateDTDescrList( BPTR lock, ULONG groupid )
{
    APTR  pool;
#define NUM_GID_TABLE (32) /* we have much less GID_#? types defined yet (datatypes.library V45), but... */
    ULONG groupid_table[ NUM_GID_TABLE ] = { 0 };

  D(bug("[DTConvert] %s(0x%p, #%08x)\n", __func__, lock, groupid);)

    /* If a lock is given, we build here a list of all DT group ids the source file may be of... */
    if( lock )
    {
      struct DataType *dtn,
                      *prevdtn = NULL;

      D(bug("[DTConvert] %s: querying locks dt's\n", __func__);)

      /* Print whole list or determine the DataType of the file */
      while( dtn = ObtainDataType( DTST_FILE, (APTR)lock, DTA_DataType,                  prevdtn,
                                                          XTAG( groupid, DTA_GroupID ),  groupid,
                                                          TAG_DONE ) )
      {
        ULONG i,
              groupid = dtn -> dtn_Header -> dth_GroupID;

        if (prevdtn == dtn)
        {
        D(bug("[DTConvert] %s: dtn == prevdtn\n", __func__);)
          break;
        }

      D(bug("[DTConvert] %s: dtn @ 0x%p\n", __func__, dtn);)

        /* Release previous DataType */
        ReleaseDataType( prevdtn );

        /* the follwoing statement assmes that GID_SYSTEM datatypes are not convertable
         * (and it filters some invalid datatypes which have a 0 group id)
         */
        if( (groupid != GID_SYSTEM) && (groupid != 0UL) )
        {
          for( i = 0UL ; i < NUM_GID_TABLE ; i++ )
          {
            if( !groupid_table[ i ] )
            {
              D(bug("[DTConvert] %s: %u = grpid #%08x\n", __func__, i, groupid);)
              groupid_table[ i ] = groupid;
              break;
            }
          }
        }

        prevdtn = dtn;
      }

      /* Release last datatype */
      ReleaseDataType( prevdtn );
    }

    if( pool = CreatePool( (MEMF_PUBLIC | MEMF_CLEAR), 1024UL, 1024UL ) )
    {
      struct DTList *dtl;

      if( dtl = (struct DTList *)AllocVecPooled( pool, sizeof( struct DTList ) ) )
      {
        struct DataType *dtn,
                        *prevdtn = NULL;
        BOOL             error   = FALSE;

        NewList( (&(dtl -> dtl_List)) );
        dtl -> dtl_Pool = pool;

      D(bug("[DTConvert] %s: querying grpid #%08x dt's\n", __func__, groupid);)

        /* Print whole list or determine the DataType of the file */
        while( dtn = ObtainDataType( DTST_RAM, NULL, DTA_DataType,                  prevdtn,
                                                     XTAG( groupid, DTA_GroupID ),  groupid,
                                                     TAG_DONE ) )
        {
          struct DataTypeHeader *dth;          /* datatypes header        */
          BOOL                   match = TRUE; /* include this datatype ? */

      D(bug("[DTConvert] %s: dtn @ 0x%p\n", __func__, dtn);)

          /* Release previous DataType */
          ReleaseDataType( prevdtn );

          dth = dtn -> dtn_Header; /* Shortcut... */

          /* If we have a lock given, we previously build a group id table... */
          if( lock )
          {
            ULONG i;

            match = FALSE;

            /* ... now check if this datatype node matches any source groupid */
            for( i = 0UL ; ((i < NUM_GID_TABLE) && groupid_table[ i ]) ; i++ )
            {
              /* Match ? */
              if( groupid_table[ i ] == (dth -> dth_GroupID) )
              {
                match = TRUE;
                break;
              }

              /* Special kluge that we can convert GID_ANIMATION to GID_MOVIE and backwards
               * (both are based on animation.datatype, here we have the same interface :-)
               */
              if( ((groupid_table[ i ]   == GID_MOVIE) || (groupid_table[ i ]   == GID_ANIMATION)) &&
                  (((dth -> dth_GroupID) == GID_MOVIE) || ((dth -> dth_GroupID) == GID_ANIMATION)) )
              {
                match = TRUE;
                break;
              }
            }
          }

          /* Include this datatype ? */
          if( match )
          {
            STRPTR         groupid_name; /* short-cut to GID_#? name string    */
            ULONG          size;         /* size of struct DTNode and contents */
            struct DTNode *namenode;

            groupid_name = (char *)GetDTString( (dth -> dth_GroupID) );

            size = sizeof( struct DTNode ) + (strlen( (dth -> dth_Name) ) * 2UL) + Strlen( groupid_name ) + 16UL;

            /* Alloc and build node... */
            if( namenode = (struct DTNode *)AllocVecPooled( pool, size ) )
            {
              STRPTR s = (STRPTR)&namenode[1];

              /* store listview names */
              mysprintf( s, "%s %s", (dth -> dth_Name), groupid_name );
              namenode -> dtn_Node . ln_Name = s;

              s = s + strlen( s ) + 2; /* next free space */

              /* store datatype name */
              strcpy( s, (dth -> dth_Name) );
              namenode -> dtn_DTName = s;

              /* add node */
              AddTail( (&(dtl -> dtl_List)), (&(namenode -> dtn_Node)) );
              dtl -> dtl_NumNodes++;
            }
            else
            {
              error = TRUE;
            }
          }

          prevdtn = dtn;
        }

        /* Release last datatype */
        ReleaseDataType( prevdtn );

        /* Success ? */
        if( !error )
        {
          return( dtl );
        }
      }

      DeletePool( pool );
    }

    return( NULL );
}


static
void FreeDTDescrList( struct DTList *list )
{
  D(bug("[DTConvert] %s()\n", __func__);)

   if( list )
   {
     DeletePool( (list -> dtl_Pool) );
   }
}


static
struct DTNode *FindDTNodeByName( struct DTList *list, STRPTR name )
{
    struct DTNode *node;
    struct DTNode *result = NULL;

  D(bug("[DTConvert] %s(0x%p, '%s')\n", __func__, list, name);)

    if( list && name )
    {
      for( node = (struct DTNode *)(list -> dtl_List . lh_Head) ; node -> dtn_Node . ln_Succ ; node = (struct DTNode *)(node -> dtn_Node . ln_Succ) )
      {
        if( !Stricmp( (node -> dtn_DTName), name ) )
        {
          result = node;
          break;
        }
      }
    }

    return( result );
}


static
STRPTR StringSave( STRPTR s )
{
    STRPTR saved;

  D(bug("[DTConvert] %s()\n", __func__);)

    if( s )
    {
      if( saved = AllocStringBuff( (ULONG)strlen( s ) ) )
      {
        strcpy( saved, s );

        return( saved );
      }
    }

    return( NULL );
}


static
STRPTR AllocStringBuff( ULONG size )
{
  D(bug("[DTConvert] %s()\n", __func__);)

    return( (STRPTR)AllocVecPooled( StringPool, (size + 2UL) ) );
}


static
STRPTR UpdateString( STRPTR *storage, STRPTR newstring )
{
    STRPTR s;

  D(bug("[DTConvert] %s()\n", __func__);)

    s = NULL;

    if( storage )
    {
      FreeString( (*storage) );

      s = (*storage) = StringSave( newstring );
    }

    return( s );
}


static
void FreeString( STRPTR s )
{
  D(bug("[DTConvert] %s()\n", __func__);)

    if( s )
    {
      FreeVecPooled( StringPool, s );
    }
}


static
struct DiskObject *GetToolDiskObject( STRPTR name )
{
    struct DiskObject *dobj;
    struct Process    *ThisProc = (struct Process *)FindTask( NULL );
    BPTR               olddir   = CurrentDir( (ThisProc -> pr_HomeDir) );

  D(bug("[DTConvert] %s()\n", __func__);)

    dobj = GetDiskObjectNew( name );

    (void)CurrentDir( olddir );

    return( dobj );
}


static
ULONG SumString( STRPTR s )
{
    ULONG sum = 0UL;

    if( s )
    {
      ULONG ch;

      while( ch = *s++ )
      {
        sum = (sum * 7UL) + ch;
      }
    }

    return( sum );
}


