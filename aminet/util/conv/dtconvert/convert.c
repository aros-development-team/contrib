
/*
**
**  $VER: convert.c 1.8 (20.3.98)
**  datatypes.library/Examples/DTConvert
**
**  Converts file into another format using datatypes
**
**  converters for the various datatype classes
**
**  Written 1996-1998 by Roland 'Gizzy' Mainz
**
*/

/* project includes */
#include "DTConvert.h"
#include "convert.h"

/* local prototypes */
static                 BOOL           ConvertDataType( Object *, struct DataType *, STRPTR );
static                 BOOL           ConvertPicture( Object *, struct DataType *, STRPTR );
static                 BOOL           ConvertAnimation( Object *, struct DataType *, STRPTR );
static                 BOOL           ConvertSound( Object *, struct DataType *, STRPTR );
static DISPATCHERFLAGS ULONG          WriteAnimAndSoundDispatch( REGA0 struct IClass *, REGA2 Object *, REGA1 Msg );
static                 void           No_Local_Encoder_Message( struct DataType * );


void Convert( STRPTR srcname, STRPTR datatypename, STRPTR destname )
{
      D(bug("[DTConvert] %s()\n", __func__);)
    if( srcname && datatypename && destname )
    {
      struct DataType *destdtn;

      if( destdtn = ObtainDataTypeA( DTST_RAM, (APTR)datatypename, NULL ) )
      {
        Object *srcdto;
        ULONG   groupid = destdtn -> dtn_Header -> dth_GroupID;
        BOOL    retry   = TRUE;

        Message( "loading source data \"%s\"\n", srcname );

retry: /* see downstairs */

        if( srcdto = NewDTObject( (APTR)srcname, DTA_GroupID, groupid, TAG_DONE ) )
        {
          if( ConvertDataType( srcdto, destdtn, destname ) )
          {
            BPTR result_lock;

            Message( "file successfully converted\n" );

            /* Did we select a root class descriptor (e.g. one of document, text, music, sound, instrument, picture,
             * animation, movie ? - In this case the base class was used to save the data...
             * (or more simply: did we use DTWM_RAW ?)
             */
            if( ((destdtn -> dtn_Header -> dth_Flags) & DTF_TYPE_MASK) != DTF_MISC )
            {
              /* Check which data format we _really_ got. In the case that a datatypes class does not
               * implement an encoder for it's format, throw out an error...
               */
              if( result_lock = Lock( destname, SHARED_LOCK ) )
              {
                struct DataType *resulting_dtn;

                if( resulting_dtn = ObtainDataType( DTST_FILE,   (APTR)result_lock,
                                                    DTA_GroupID, (destdtn -> dtn_Header -> dth_GroupID),
                                                    TAG_DONE ) )
                {
                  /* Not the same datatype ? */
                  if( Stricmp( (destdtn -> dtn_Header -> dth_Name), (resulting_dtn -> dtn_Header -> dth_Name) ) )
                  {
                    No_Local_Encoder_Message( destdtn );
                  }

                  ReleaseDataType( resulting_dtn );
                }

                UnLock( result_lock );
              }
            }
          }
          else
          {
            Message( "conversion failed\n" );
          }

          DisposeDTObject( srcdto );
        }
        else
        {
          if( retry )
          {
            retry = FALSE;

            /* Kluge that conversion between GID_ANIMATION and GID_MOVIE datatypes gets possible */
            switch( groupid )
            {
              case GID_ANIMATION: groupid = GID_MOVIE;     goto retry;
              case GID_MOVIE:     groupid = GID_ANIMATION; goto retry;
            }
          }

          Message( "can't create source object\n" );
        }

        ReleaseDataType( destdtn );
      }
      else
      {
        Message( "can' open destination datatype\n" );
      }
    }
    else
    {
      SetIoErr( ERROR_REQUIRED_ARG_MISSING );
    }
}


static
BOOL ConvertDataType( Object *srcdto, struct DataType *destdtn, STRPTR destfile )
{
    BOOL success = FALSE;
    LONG error   = 0L;

      D(bug("[DTConvert] %s()\n", __func__);)
    if( srcdto && destfile )
    {
      struct DataType *srcdtn;

      (void)GetDTAttrs( srcdto, DTA_DataType, (&srcdtn), TAG_DONE );

      Message( "converting \"%s\" %s into \"%s\" %s\n",
              (srcdtn  -> dtn_Header -> dth_Name),
              GetDTString( (srcdtn -> dtn_Header -> dth_GroupID) ),
              (destdtn -> dtn_Header -> dth_Name),
              GetDTString( (destdtn -> dtn_Header -> dth_GroupID) ) );

      switch( srcdtn -> dtn_Header -> dth_GroupID )
      {
        case GID_SYSTEM:
        {
            /* There is currently no system.datatype base class defined */
            error = ERROR_ACTION_NOT_KNOWN;
        }
            break;

        case GID_TEXT:
        {
            /* text.datatype -> text.datatype conversion possible, but not implemented...  */
            error = ERROR_NOT_IMPLEMENTED;
        }
            break;

        case GID_DOCUMENT:
        {
            /* There is currently no document.datatype base class defined */
            error = ERROR_ACTION_NOT_KNOWN;
        }
            break;

        case GID_SOUND:
        {
            success = ConvertSound( srcdto, destdtn, destfile );
            error = IoErr();
        }
            break;

        case GID_INSTRUMENT:
        {
            /* There is currently no instrument.datatype base class defined */
            error = ERROR_ACTION_NOT_KNOWN;
        }
            break;

        case GID_MUSIC:
        {
            /* There is currently no music.datatype base class defined */
            error = ERROR_ACTION_NOT_KNOWN;
        }
            break;

        case GID_PICTURE:
        {
            success = ConvertPicture( srcdto, destdtn, destfile );
            error = IoErr();
        }
            break;

        case GID_ANIMATION:
        case GID_MOVIE:
        {
            success = ConvertAnimation( srcdto, destdtn, destfile );
            error = IoErr();
        }
            break;

        default:
        {
            error = ERROR_ACTION_NOT_KNOWN;
        }
            break;
      }
    }

    SetIoErr( error );

    return( success );
}


static
BOOL ConvertPicture( Object *srcdto, struct DataType *destdtn, STRPTR destfile )
{
    BOOL success = FALSE;
    LONG error   = 0L;

      D(bug("[DTConvert] %s()\n", __func__);)
    Message( "picture converter\n" );

    D(
      bug("[DTConvert] %s: srcdto @ 0x%p\n", __func__, srcdto);
      bug("[DTConvert] %s: destdtn @ 0x%p\n", __func__, destdtn);
      bug("[DTConvert] %s: file = '%s'\n", __func__, destfile);
    )

    if( srcdto && destfile )
    {
      Object          *destdto;
      struct DataType *srcdtn;
      struct BitMap   *bm;

      /* Some (ugly) picture.datatype needs a DTM_PROCLAYOUT before
       * allowing to access their bitmaps
       */
      (void)GetDTAttrs( srcdto, PDTA_BitMap, (&bm), TAG_DONE );

      if( bm == NULL )
      {
        struct gpLayout  gpl;

      D(bug("[DTConvert] %s: srcdto needs layouting\n", __func__);)
        Message( "source requires layouting...\n" );

        /* "Layout" picture (for those picture classes which need this,
         * __NOT__ recommened)
         */
        SetDTAttrs( srcdto, NULL, NULL, PDTA_Remap, FALSE, TAG_DONE );

        gpl . MethodID    = DTM_PROCLAYOUT;
        gpl . gpl_GInfo   = NULL;
        gpl . gpl_Initial = 1L;

        DoDTMethodA( srcdto, NULL, NULL, (Msg)(&gpl) );
      }

      /* Get source datatype, see below */
      (void)GetDTAttrs( srcdto, DTA_DataType, (&srcdtn), TAG_DONE );

      D(bug("[DTConvert] %s: srcdto datatype @ 0x%p\n", __func__, srcdtn);)

      /* Create empty picture.datatype subclass object */
      if( destdto = NewDTObject( NULL, DTA_SourceType, DTST_RAM,
                                       DTA_DataType,   destdtn,
                                       TAG_DONE ) )
      {
        STRPTR                objname,
                              objauthor,
                              objannotation,
                              objcopyright,
                              objversion;
        ULONG                 modeid;
        ULONG                 numcolors;
        struct BitMapHeader  *sbmh,
                             *dbmh;
        ULONG                *scregs;
        struct ColorRegister *scm;

      D(bug("[DTConvert] %s: destdto @ 0x%p\n", __func__, destdto);)

        if( GetDTAttrs( srcdto, DTA_ObjName,         (&objname),
                                DTA_ObjAuthor,       (&objauthor),
                                DTA_ObjAnnotation,   (&objannotation),
                                DTA_ObjCopyright,    (&objcopyright),
                                DTA_ObjVersion,      (&objversion),
                                PDTA_ModeID,         (&modeid),
                                PDTA_BitMapHeader,   (&sbmh),
                                PDTA_NumColors,      (&numcolors),
                                PDTA_CRegs,          (&scregs),
                                PDTA_ColorRegisters, (&scm),
                                PDTA_BitMap,         (&bm),
                                TAG_DONE ) == 11UL )
        {

    D(
      bug("[DTConvert] %s: srcdto attribs obtained\n", __func__);
      bug("[DTConvert] %s: DTA_ObjName = 0x%p\n", __func__, objname);
      bug("[DTConvert] %s: DTA_ObjAuthor = 0x%p\n", __func__, objauthor);
      bug("[DTConvert] %s: DTA_ObjAnnotation = 0x%p\n", __func__, objannotation);
      bug("[DTConvert] %s: DTA_ObjCopyright = 0x%p\n", __func__, objcopyright);
      bug("[DTConvert] %s: DTA_ObjVersion = 0x%p\n", __func__, objversion);
      bug("[DTConvert] %s: PDTA_ModeID = 0x%p\n", __func__, modeid);
      bug("[DTConvert] %s: PDTA_BitMapHeader = 0x%p\n", __func__, sbmh);
      bug("[DTConvert] %s: PDTA_BitMap = 0x%p\n", __func__, bm);
      bug("[DTConvert] %s: PDTA_NumColors = 0x%p\n", __func__, numcolors);
      bug("[DTConvert] %s: PDTA_CRegs = 0x%p\n", __func__, scregs);
      bug("[DTConvert] %s: PDTA_ColorRegisters = 0x%p\n", __func__, scm);
    )

          if( GetDTAttrs( destdto, PDTA_BitMapHeader, (&dbmh), TAG_DONE ) == 1UL )
          {
      D(bug("[DTConvert] %s: destdto bmheader @ 0x%p\n", __func__, dbmh);)
            if( sbmh && bm && dbmh )
            {
              ULONG                *dcregs;
              struct ColorRegister *dcm;

              /* copy bitmapheader (RAW) from source bmh... */
              *dbmh = *sbmh;

              /* ...and fill in attributes which may be different */
              dbmh -> bmh_Compression = cmpNone;

              /* we're writing a full palette, set this flag (currently NOP, but...) */
              dbmh -> bmh_Pad    = BMHDF_CMAPOK; /* also known as bmh_Flags */

              /* Set datas "name", "author", "annotation", "copyright", "version" etc.,
               * screen mode id,
               * our bitmap and
               * the requested number of colors...
               */
              SetDTAttrs( destdto, NULL, NULL,
                          DTA_ObjName,       objname,
                          DTA_ObjAuthor,     objauthor,
                          DTA_ObjAnnotation, objannotation,
                          DTA_ObjCopyright,  objcopyright,
                          DTA_ObjVersion,    objversion,
                          PDTA_ModeID,       modeid,
                          PDTA_NumColors,    numcolors,
                          PDTA_BitMap,       bm,
                          TAG_DONE );

              /* Some (ugly) picture.datatype needs a DTM_PROCLAYOUT before
               * allowing to access their bitmaps
               */
              (void)GetDTAttrs( destdto, PDTA_BitMap, (&bm), TAG_DONE );

              if( bm == NULL )
              {
                struct gpLayout  gpl;

                Message( "destination requires layouting...\n" );

                /* "Layout" picture (for those picture classes which need this,
                 * __NOT__ recommened)
                 */
                SetDTAttrs( destdto, NULL, NULL, PDTA_Remap, FALSE, TAG_DONE );

                gpl . MethodID    = DTM_PROCLAYOUT;
                gpl . gpl_GInfo   = NULL;
                gpl . gpl_Initial = 1L;

                DoDTMethodA( destdto, NULL, NULL, (Msg)(&gpl) );
              }

              /* Get color tables (dest cregs, dest cm)  */
              if( GetDTAttrs( destdto, PDTA_CRegs,          (&dcregs),
                                       PDTA_ColorRegisters, (&dcm),
                                       TAG_DONE ) == 2UL )
              {
                /* Success ? */
                if( dcregs && dcm )
                {
                  ULONG i;

                  /* Copy color tables... */
                  for( i = 0UL ; i < numcolors ; i++ )
                  {
                    dcregs[ ((i * 3) + 0) ] = scregs[ ((i * 3) + 0) ]; /* copy r, */
                    dcregs[ ((i * 3) + 1) ] = scregs[ ((i * 3) + 1) ]; /*      g, */
                    dcregs[ ((i * 3) + 2) ] = scregs[ ((i * 3) + 2) ]; /*      b  */

                    *dcm++ = *scm++; /* copy colorregisters */
                  }

                  Message( "saving picture: \"%s\"\n", destfile );

                  if( !(success = SaveDTObjectA( destdto, NULL, NULL, destfile, DTWM_RAW, TRUE, NULL )) )
                  {
                    error = IoErr();
                    Message( "saving failed\n" );
                  }
                }
                else
                {
                  Message( "picture object mem err (no color tables)\n" );
                  error = ERROR_NO_FREE_STORE;
                }
              }
              else
              {
                /* can't get required args from object */
      D(bug("[DTConvert] %s: missing color regs\n", __func__);)
                error = ERROR_OBJECT_WRONG_TYPE;
              }

              /* src datatype will free it's bitmap  */
              SetDTAttrs( destdto, NULL, NULL, PDTA_BitMap, NULL, TAG_DONE );
            }
            else
            {
      D(bug("[DTConvert] %s: missing bmheader/bm\n", __func__);)
              Message( "can't get bitmap\n" );
              error = ERROR_NO_FREE_STORE;
            }
          }
          else
          {
            /* can't get required args from destination object */
      D(bug("[DTConvert] %s: dest object missing bmheader\n", __func__);)
            Message( "can't get PDTA_BitMapHeader from destination object \n" );
            error = ERROR_OBJECT_WRONG_TYPE;
          }
        }
        else
        {
          /* can't get required args from source object */
      D(bug("[DTConvert] %s: source object missing attribs\n", __func__);)
          Message( "can't get required args from source object\n" );
          error = ERROR_OBJECT_WRONG_TYPE;
        }

        /* Get rid of the picture object */
        DisposeDTObject( destdto );
      }
      else
      {
        /* NewDTObject failed */
        error = IoErr();

        Message( "can't create empty object\n" );

        No_Local_Encoder_Message( destdtn );
      }
    }
    else
    {
      /* missing function args */
      error = ERROR_REQUIRED_ARG_MISSING;
    }

    SetIoErr( error );

    return( success );
}


static
BOOL ConvertAnimation( Object *srcdto, struct DataType *destdtn, STRPTR destfile )
{
    BOOL success = FALSE;
    LONG error   = 0L;

      D(bug("[DTConvert] %s()\n", __func__);)
    Message( "animation converter\n" );

    if( srcdto && destfile && destdtn )
    {
      TEXT             classname[ 256 ];
      struct Library  *DTClassBase;
      Object          *destdto;
      struct DataType *srcdtn;

      /* Get source datatype, see below */
      (void)GetDTAttrs( srcdto, DTA_DataType, (&srcdtn), TAG_DONE );

      /* Create class library name... */
      mysprintf( classname, "DataTypes/%s.datatype", (destdtn -> dtn_Header -> dth_BaseName) );

      /* ...then open it */
      if( DTClassBase = OpenLibrary( classname, 0UL ) )
      {
        struct IClass *AnimClass;

#if !defined(__AROS__)
        /* Obtain destination class */
        if( AnimClass = ObtainEngine() )
#else
        if( (AnimClass = AROS_LVO_CALL0(Class *, struct Library *, DTClassBase, 5,)) )
#endif
        {
          struct IClass *WriteAnimClass;

          /* Create our new class... */
          if( WriteAnimClass = MakeClass( NULL, NULL, AnimClass, 0UL, 0UL ) )
          {
            /* Prepare class for usage... */
            WriteAnimClass -> cl_Dispatcher . h_Entry = (HOOKFUNC)WriteAnimAndSoundDispatch;
            WriteAnimClass -> cl_UserData             = 0UL; /* datatypes.library expects here a (struct Library *) or NULL,
                                                              * therefore we cannot use this for our context data
                                                              */

            /* Create empty subclass object (from our dest write class) */
            if( destdto = (Object *)NewDTObject( NULL, DTA_SourceType,          DTST_RAM,       /* Create empty object...    */
                                                       DTA_DataType,            destdtn,        /* ...using this datatype... */
                                                       DTA_Class,               WriteAnimClass, /* ...from this class...     */
                                                       WRITEANIMA_SourceObject, srcdto,         /* source of data            */
                                                       TAG_DONE ) )
            {
              /* datatypesclass attributes */
              STRPTR                objname,
                                    objauthor,
                                    objannotation,
                                    objcopyright,
                                    objversion;

              /* picture (e.g. global) attributes  */
              ULONG                 modeid;
              ULONG                 numcolors;
              struct BitMap        *bm;
              struct BitMapHeader  *sbmh,
                                   *dbmh;
              ULONG                *scregs;
              struct ColorRegister *scm;
              ULONG                 width,
                                    height,
                                    depth;
                                    
              /* animation attributes */
              ULONG                 frames,
                                    fps,
                                    finc;
                                    
              /* sound attributes */
              BYTE                 *sample;
              ULONG                 samplelength,
                                    period,
                                    volume,
                                    cycles;

              if( GetDTAttrs( srcdto, DTA_ObjName,          (&objname),
                                      DTA_ObjAuthor,        (&objauthor),
                                      DTA_ObjAnnotation,    (&objannotation),
                                      DTA_ObjCopyright,     (&objcopyright),
                                      DTA_ObjVersion,       (&objversion),
                                      PDTA_BitMapHeader,    (&sbmh),
                                      ADTA_ModeID,          (&modeid),
                                      ADTA_NumColors,       (&numcolors),
                                      ADTA_CRegs,           (&scregs),
                                      ADTA_ColorRegisters,  (&scm),
                                      ADTA_KeyFrame,        (&bm),
                                      ADTA_Width,           (&width),
                                      ADTA_Height,          (&height),
                                      ADTA_Depth,           (&depth),
                                      ADTA_Frames,          (&frames),
                                      ADTA_FramesPerSecond, (&fps),
                                      ADTA_FrameIncrement,  (&finc),
                                      ADTA_Sample,          (&sample),
                                      ADTA_SampleLength,    (&samplelength),
                                      ADTA_Period,          (&period),
                                      ADTA_Volume,          (&volume),
                                      ADTA_Cycles,          (&cycles),
                                      TAG_DONE ) == 22UL )
              {
                if( GetDTAttrs( destdto, PDTA_BitMapHeader, (&dbmh), TAG_DONE ) == 1UL )
                {
                  if( sbmh && bm && dbmh )
                  {
                    ULONG                *dcregs;
                    struct ColorRegister *dcm;

                    /* copy bitmapheader (RAW) from animation bmh... */
                    *dbmh = *sbmh;

                    /* ...and fill in attributes which may be different */
                    dbmh -> bmh_Compression = cmpNone;

                    /* we're writing a full palette, set this flag (currently NOP, but...) */
                    dbmh -> bmh_Pad  = BMHDF_CMAPOK; /* also known as bmh_Flags */

                    /* Set datas "name", "author", "annotation", "copyright", "version" etc.,
                     * screen mode id,
                     * our bitmap and
                     * the requested number of colors...
                     */
                    SetDTAttrs( destdto, NULL, NULL,
                                DTA_ObjName,          objname,
                                DTA_ObjAuthor,        objauthor,
                                DTA_ObjAnnotation,    objannotation,
                                DTA_ObjCopyright,     objcopyright,
                                DTA_ObjVersion,       objversion,
                                PDTA_BitMapHeader,    sbmh,
                                ADTA_ModeID,          modeid,
                                ADTA_NumColors,       numcolors,
                                ADTA_CRegs,           scregs,
                                ADTA_ColorRegisters,  scm,
                                ADTA_KeyFrame,        bm,
                                ADTA_Width,           width,
                                ADTA_Height,          height,
                                ADTA_Depth,           depth,
                                ADTA_Frames,          frames,
                                ADTA_FramesPerSecond, fps,
                                ADTA_FrameIncrement,  finc,
                                ADTA_Sample,          sample,
                                ADTA_SampleLength,    samplelength,
                                ADTA_Period,          period,
                                ADTA_Volume,          volume,
                                ADTA_Cycles,          cycles,
                                TAG_DONE );

                    /* Get color tables (dest cregs, dest cm)  */
                    if( GetDTAttrs( destdto, ADTA_CRegs,          (&dcregs),
                                             ADTA_ColorRegisters, (&dcm),
                                             TAG_DONE ) == 2UL )
                    {
                      /* Success ? */
                      if( (dcregs && dcm) || (numcolors == 0UL) )
                      {
                        ULONG i;

                        /* Copy color tables... */
                        for( i = 0UL ; i < numcolors ; i++ )
                        {
                          dcregs[ ((i * 3) + 0) ] = scregs[ ((i * 3) + 0) ]; /* copy r, */
                          dcregs[ ((i * 3) + 1) ] = scregs[ ((i * 3) + 1) ]; /*      g, */
                          dcregs[ ((i * 3) + 2) ] = scregs[ ((i * 3) + 2) ]; /*      b  */

                          *dcm++ = *scm++; /* copy colorregisters */
                        }

                        Message( "saving animation: \"%s\"\n", destfile );

                        if( !(success = SaveDTObjectA( destdto, NULL, NULL, destfile, DTWM_RAW, TRUE, NULL )) )
                        {
                          error = IoErr();
                          Message( "saving failed\n" );
                        }
                      }
                      else
                      {
                        Message( "animation object mem err (no color tables)\n" );
                        error = ERROR_NO_FREE_STORE;
                      }
                    }
                    else
                    {
                      /* can't get required args from object */
                      error = ERROR_OBJECT_WRONG_TYPE;
                    }

                    /* src datatype will free it's key frame (bitmap) */
                    SetDTAttrs( destdto, NULL, NULL, ADTA_KeyFrame, NULL, TAG_DONE );
                  }
                  else
                  {
                    Message( "can't get key frame (bitmap)\n" );
                    error = ERROR_NO_FREE_STORE;
                  }
                }
                else
                {
                  /* can't get required args from destination object */
                  Message( "can't get PDTA_BitMapHeader from destination\n" );
                  error = ERROR_OBJECT_WRONG_TYPE;
                }
              }
              else
              {
                /* can't get required args from source object */
                Message( "can't get required args from source object\n" );
                error = ERROR_OBJECT_WRONG_TYPE;
              }

              /* Get rid of the animation object */
              DisposeDTObject( destdto );
            }
            else
            {
              /* NewDTObject failed */
              error = IoErr();

              Message( "can't create empty object\n" );

              No_Local_Encoder_Message( destdtn );
            }

            /* Free class... */
            while( !FreeClass( WriteAnimClass ) )
            {
              /* Let other tasks partake... */
              Delay( (TICKS_PER_SECOND / 5UL) );
            }
          }
        }
        else
        {
          Message( "can't obtain output anim class\n" );
        }

        CloseLibrary( DTClassBase );
      }
      else
      {
        /* Can't open class library */
        if( !(error = IoErr()) )
        {
          error = DTERROR_UNKNOWN_DATATYPE;
        }
      }
    }
    else
    {
      /* missing function args */
      error = ERROR_REQUIRED_ARG_MISSING;
    }

    SetIoErr( error );

    return( success );
}


static
BOOL ConvertSound( Object *srcdto, struct DataType *destdtn, STRPTR destfile )
{
    BOOL success = FALSE;
    LONG error   = 0L;

      D(bug("[DTConvert] %s()\n", __func__);)
    Message( "sound converter\n" );

    if( srcdto && destfile )
    {
      Object          *destdto;
      struct DataType *srcdtn;

      /* Get source datatype, see below */
      (void)GetDTAttrs( srcdto, DTA_DataType, (&srcdtn), TAG_DONE );

      /* Create empty sound.datatype subclass object */
      if( destdto = NewDTObject( NULL, DTA_SourceType,  DTST_RAM,
                                       DTA_DataType,    destdtn,
                                       TAG_DONE ) )
      {
        STRPTR                objname,
                              objauthor,
                              objannotation,
                              objcopyright,
                              objversion;
        struct VoiceHeader   *svh,
                             *dvh;
        SAMPLE8              *sample;
        ULONG                 samplelength;
        ULONG                 period,
                              volume,
                              cycles;

        if( GetDTAttrs( srcdto, DTA_ObjName,         (&objname),
                                DTA_ObjAuthor,       (&objauthor),
                                DTA_ObjAnnotation,   (&objannotation),
                                DTA_ObjCopyright,    (&objcopyright),
                                DTA_ObjVersion,      (&objversion),
                                SDTA_VoiceHeader,    (&svh),
                                SDTA_Sample,         (&sample),
                                SDTA_SampleLength,   (&samplelength),
                                SDTA_Period,         (&period),
                                SDTA_Volume,         (&volume),
                                SDTA_Cycles,         (&cycles),
                                TAG_DONE ) == 11UL )
        {
          if( GetDTAttrs( destdto, SDTA_VoiceHeader, (&dvh), TAG_DONE ) == 1UL )
          {
            if( svh && sample && samplelength && dvh )
            {
              /* copy voiceheader (RAW) from source vh... */
              *dvh = *svh;

              /* ...and fill in attributes which may be different */
              dvh -> vh_Compression = CMP_NONE;

              /* Set datas "name", "author", "annotation", "copyright", "version" etc.,
               * screen mode id,
               * our sample data etc.
               */
              SetDTAttrs( destdto, NULL, NULL,
                          DTA_ObjName,       objname,
                          DTA_ObjAuthor,     objauthor,
                          DTA_ObjAnnotation, objannotation,
                          DTA_ObjCopyright,  objcopyright,
                          DTA_ObjVersion,    objversion,
                          SDTA_Sample,       sample,
                          SDTA_SampleLength, samplelength,
                          SDTA_Period,       period,
                          SDTA_Volume,       volume,
                          SDTA_Cycles,       cycles,
                          TAG_DONE );

              Message( "saving sound: \"%s\"\n", destfile );

              if( !(success = SaveDTObjectA( destdto, NULL, NULL, destfile, DTWM_RAW, TRUE, NULL )) )
              {
                error = IoErr();
                Message( "saving failed\n" );
              }

              /* src datatype will free it's sample data  */
              SetDTAttrs( destdto, NULL, NULL, SDTA_Sample, NULL, TAG_DONE );
            }
            else
            {
              Message( "can't get sample (maybe unsupported sound.datatype V41 mode)\n" );
              error = ERROR_NO_FREE_STORE;
            }
          }
          else
          {
            /* can't get required args from destination object */
            Message( "can't get required args from destination object\n" );
            error = ERROR_OBJECT_WRONG_TYPE;
          }
        }
        else
        {
          /* can't get required args from source object */
          Message( "can't get required args from source object\n" );
          error = ERROR_OBJECT_WRONG_TYPE;
        }

        /* Get rid of the sound object */
        DisposeDTObject( destdto );
      }
      else
      {
        /* NewDTObject failed */
        error = IoErr();

        Message( "can't create empty object\n" );

        No_Local_Encoder_Message( destdtn );
      }
    }
    else
    {
      /* missing function args */
      error = ERROR_REQUIRED_ARG_MISSING;
    }

    SetIoErr( error );

    return( success );
}


/* WriteAnimClass/WriteSoundClass dispatcher */
static
DISPATCHERFLAGS
ULONG WriteAnimAndSoundDispatch( REGA0 struct IClass *cl, REGA2 Object *o, REGA1 Msg msg )
{
    ULONG retval = 0UL;

      D(bug("[DTConvert] %s()\n", __func__);)
    switch( msg -> MethodID )
    {
#ifdef CURRENTLY_USELESS_CODE
        case OM_NEW:
        {
            /* Assumes that WRITEANIMA_SourceObject contains a valid object
             * (WRITEANIMA_SourceObject s mapped to DTA_UserData; datatypesclass handles
             * storage of this attribute :-)
             */
            if( retval = DoSuperMethodA( cl, o, msg ) )
            {
              /* NOP */
            }
        }
            break;

        case OM_DISPOSE:
        {
            DoSuperMethodA( cl, o, msg );
        }
            break;
#endif /* CURRENTLY_USELESS_CODE */

        case OM_UPDATE:
        {
            /* Avoid OM_NOTIFY loops */
            if( DoMethod( o, ICM_CHECKLOOP ) )
            {
              break;
            }
        }
        case OM_SET:
        {
            /* Pass the attributes to the text class and force a refresh
             * if we need it
             */
            if( retval = DoSuperMethodA( cl, o, msg ) )
            {
/* writeanimclass is alwasy the top instance... */
#ifdef COMMENTED_OUT
              /* Top instance ? */
              if( OCLASS( o ) == cl )
#endif /* COMMENTED_OUT */
              {
                struct RastPort *rp;

                /* Get a pointer to the rastport */
                if( rp = ObtainGIRPort( (((struct opSet *)msg) -> ops_GInfo) ) )
                {
                  struct gpRender gpr;

                  /* Force a redraw */
                  gpr . MethodID   = GM_RENDER;
                  gpr . gpr_GInfo  = ((struct opSet *)msg) -> ops_GInfo;
                  gpr . gpr_RPort  = rp;
                  gpr . gpr_Redraw = GREDRAW_UPDATE;

                  DoMethodA( o, (Msg)(&gpr) );

                  /* Release the temporary rastport */
                  ReleaseGIRPort( rp );
                }

                retval = 0UL;
              }
            }
        }
            break;

        /* This two methods are superset by writeanimclass (e.g. us) */
        case ADTM_LOADFRAME:
        case ADTM_UNLOADFRAME:
        {
            Object *source;

            if( GetDTAttrs( o, WRITEANIMA_SourceObject, (&source), TAG_DONE ) == 1UL )
            {
              if( source )
              {
                /* Route msg to source object... */
                retval = DoMethodA( source, msg );
              }
              else
              {
                /* This error code aborts animation.datatype's playback and should also abort any encoder
                 * (only ERROR_OBJECT_NOT_FOUND should be ignored by encoders...)
                 */
                SetIoErr( ERROR_REQUIRED_ARG_MISSING );
              }
            }
        }
            break;

#ifdef SOUNDDATATYPE_V41_SUPPORT
        /* This two methods are superset by writesoundclass (e.g. us) */
        case SDTM_LOADSAMPLE:
        case SDTM_UNLOADSAMPLE:
        {
            Object *source;

            if( GetDTAttrs( o, WRITESOUNDA_SourceObject, (&source), TAG_DONE ) )
            {
              if( source )
              {
                /* Route msg to source object... */
                retval = DoMethodA( source, msg );
              }
              else
              {
                /* This error code aborts animation.datatype's playback and should also abort any encoder
                 * (only ERROR_OBJECT_NOT_FOUND should be ignored by encoders...)
                 */
                SetIoErr( ERROR_REQUIRED_ARG_MISSING );
              }
            }
        }
            break;
#endif /* SOUNDDATATYPE_V41_SUPPORT */

        /* Let the superclass handle everything else */
        default:
        {
            retval = DoSuperMethodA( cl, o, msg );
        }
            break;
    }

    return( retval );
}


static
void No_Local_Encoder_Message( struct DataType *dtn )
{
    struct DataTypeHeader *dth = dtn -> dtn_Header;

  D(bug("[DTConvert] %s()\n", __func__);)

    Message( "##########################################################################\n"
             "# Error: the selected class \"%s\"\n"
             "# does not implement an encoder (which is reposible to save the data in\n"
             "# the requested format \"%s\").\n"
             "# Please contact the author of the SUBCLASS to fix this problem...\n"
             "##########################################################################\n",
             (dth -> dth_BaseName), (dth -> dth_Name) );
}




