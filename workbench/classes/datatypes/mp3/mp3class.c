/*
    Copyright (C) 2025, The AROS Development Team. All rights reserved.

    mp3.datatype - MPEG Layer III audio datatype for AROS.

    Modelled on the 8svx and wav datatypes; decodes the entire MP3 stream
    to 16-bit signed native-endian PCM in OM_NEW using libmad, then hands
    the resulting buffer to the parent sound.datatype via the standard
    SDTA_Sample attributes.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dostags.h>
#include <intuition/imageclass.h>
#include <intuition/icclass.h>
#include <intuition/gadgetclass.h>
#include <intuition/cghooks.h>
#include <datatypes/datatypesclass.h>
#include <datatypes/soundclass.h>
#include <datatypes/soundclassext.h>

#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/datatypes.h>

#include <aros/symbolsets.h>

#include <mad.h>

#include "debug.h"

/* Open superclass */
ADD2LIBS("datatypes/sound.datatype", 0, struct Library *, SoundBase);

/**************************************************************************/

#define MP3_INPUT_BUFFER_SIZE   16384
#define MP3_INITIAL_PCM_BYTES   (1 * 1024 * 1024)

struct MP3DecodeCtx
{
    BPTR     mp3_File;
    UBYTE   *mp3_InBuf;
    LONG     mp3_InBufFill;     /* number of valid bytes in mp3_InBuf */
    BOOL     mp3_EOF;

    /* Growing destination buffer of native-endian signed 16-bit PCM. */
    BYTE    *mp3_PCM;
    ULONG    mp3_PCMCapacity;   /* allocated bytes */
    ULONG    mp3_PCMUsed;       /* used bytes */

    ULONG    mp3_SampleRate;    /* captured from the first decoded frame */
    UWORD    mp3_Channels;      /* captured from the first decoded frame */
    BOOL     mp3_Error;
};

/**************************************************************************/

/* Scale a 28-bit mad_fixed_t sample down to signed 16-bit with rounding
 * and clipping. (From libmad's minimad.c / madahi.c reference.)
 */
static inline signed int mp3_scale(mad_fixed_t sample)
{
    sample += (1L << (MAD_F_FRACBITS - 16));

    if (sample >= MAD_F_ONE)
        sample = MAD_F_ONE - 1;
    else if (sample < -MAD_F_ONE)
        sample = -MAD_F_ONE;

    return sample >> (MAD_F_FRACBITS + 1 - 16);
}

/* Grow the PCM destination buffer to hold at least 'needed' bytes. */
static BOOL mp3_EnsureCapacity(struct MP3DecodeCtx *ctx, ULONG needed)
{
    ULONG newcap;
    BYTE *newbuf;

    if (needed <= ctx->mp3_PCMCapacity)
        return TRUE;

    newcap = ctx->mp3_PCMCapacity ? ctx->mp3_PCMCapacity : MP3_INITIAL_PCM_BYTES;
    while (newcap < needed)
    {
        ULONG doubled = newcap << 1;
        if (doubled <= newcap)
            return FALSE; /* overflow */
        newcap = doubled;
    }

    newbuf = AllocVec(newcap, MEMF_PUBLIC);
    if (!newbuf)
        return FALSE;

    if (ctx->mp3_PCM)
    {
        CopyMem(ctx->mp3_PCM, newbuf, ctx->mp3_PCMUsed);
        FreeVec(ctx->mp3_PCM);
    }
    ctx->mp3_PCM = newbuf;
    ctx->mp3_PCMCapacity = newcap;
    return TRUE;
}

/**************************************************************************/

/* libmad input callback: top up the input buffer by reading more bytes
 * from the source file, preserving any unconsumed tail of the previous
 * buffer (which the decoder asks us to keep).
 */
static enum mad_flow mp3_Input(void *data, struct mad_stream *stream)
{
    struct MP3DecodeCtx *ctx = (struct MP3DecodeCtx *)data;
    LONG     remaining = 0;
    LONG     readlen;

    if (ctx->mp3_EOF)
        return MAD_FLOW_STOP;

    if (stream->next_frame)
    {
        remaining = stream->bufend - stream->next_frame;
        memmove(ctx->mp3_InBuf, stream->next_frame, remaining);
    }

    readlen = Read(ctx->mp3_File,
                   ctx->mp3_InBuf + remaining,
                   MP3_INPUT_BUFFER_SIZE - remaining);
    if (readlen < 0)
    {
        D(bug("[mp3.dt] Input: read error\n"));
        ctx->mp3_Error = TRUE;
        return MAD_FLOW_STOP;
    }
    if (readlen == 0)
    {
        ctx->mp3_EOF = TRUE;
        if (remaining == 0)
            return MAD_FLOW_STOP;
        /* Pad final flush with MAD_BUFFER_GUARD bytes of zeroes so libmad
         * can decode the last frame. */
        if (remaining + MAD_BUFFER_GUARD <= MP3_INPUT_BUFFER_SIZE)
        {
            memset(ctx->mp3_InBuf + remaining, 0, MAD_BUFFER_GUARD);
            readlen = MAD_BUFFER_GUARD;
        }
    }

    ctx->mp3_InBufFill = remaining + readlen;
    mad_stream_buffer(stream, ctx->mp3_InBuf, ctx->mp3_InBufFill);

    return MAD_FLOW_CONTINUE;
}

/* libmad output callback: convert one decoded frame to signed 16-bit
 * native-endian PCM (interleaved for stereo) and append to our growing
 * destination buffer.
 */
static enum mad_flow mp3_Output(void *data, struct mad_header const *header,
                                struct mad_pcm *pcm)
{
    struct MP3DecodeCtx *ctx = (struct MP3DecodeCtx *)data;
    unsigned int nchannels = pcm->channels;
    unsigned int nsamples  = pcm->length;
    mad_fixed_t const *left_ch  = pcm->samples[0];
    mad_fixed_t const *right_ch = pcm->samples[1];
    ULONG bytes_needed;
    WORD *out;

    if (ctx->mp3_SampleRate == 0)
    {
        ctx->mp3_SampleRate = pcm->samplerate;
        ctx->mp3_Channels   = nchannels;
        D(bug("[mp3.dt] Output: rate %lu, channels %u\n",
              ctx->mp3_SampleRate, ctx->mp3_Channels));
    }

    /* If the stream switches channel count mid-file we keep emitting in
     * the initial channel layout to keep the output buffer regular.
     * mp3_Channels stays fixed; mix or duplicate as needed.
     */
    bytes_needed = ctx->mp3_PCMUsed
                 + (ULONG)nsamples * ctx->mp3_Channels * sizeof(WORD);

    if (!mp3_EnsureCapacity(ctx, bytes_needed))
    {
        D(bug("[mp3.dt] Output: out of memory\n"));
        ctx->mp3_Error = TRUE;
        return MAD_FLOW_STOP;
    }

    out = (WORD *)(ctx->mp3_PCM + ctx->mp3_PCMUsed);

    if (ctx->mp3_Channels == 2)
    {
        while (nsamples--)
        {
            signed int l = mp3_scale(*left_ch++);
            signed int r;
            if (nchannels == 2)
                r = mp3_scale(*right_ch++);
            else
                r = l; /* duplicate mono into right channel */
            *out++ = (WORD)l;
            *out++ = (WORD)r;
        }
    }
    else /* mono output */
    {
        while (nsamples--)
        {
            signed int l = mp3_scale(*left_ch++);
            if (nchannels == 2)
            {
                signed int r = mp3_scale(*right_ch++);
                l = (l + r) >> 1; /* downmix */
            }
            *out++ = (WORD)l;
        }
    }

    ctx->mp3_PCMUsed = bytes_needed;
    return MAD_FLOW_CONTINUE;
}

/* libmad error callback: skip recoverable errors, abort on fatal ones. */
static enum mad_flow mp3_Error(void *data, struct mad_stream *stream,
                               struct mad_frame *frame)
{
    struct MP3DecodeCtx *ctx = (struct MP3DecodeCtx *)data;

    D(bug("[mp3.dt] decode error 0x%04x (%s)\n",
          stream->error, mad_stream_errorstr(stream)));

    if (MAD_RECOVERABLE(stream->error))
        return MAD_FLOW_CONTINUE;

    ctx->mp3_Error = TRUE;
    return MAD_FLOW_BREAK;
}

/**************************************************************************/

static BOOL ReadMP3(Class *cl, Object *o)
{
    struct MP3DecodeCtx ctx;
    struct mad_decoder  decoder;
    IPTR                sourcetype = 0;
    BPTR                handle = BNULL;
    LONG                err;
    UBYTE               sampletype;
    ULONG               framecount;

    D(bug("mp3.datatype/ReadMP3()\n"));

    memset(&ctx, 0, sizeof(ctx));

    if (GetDTAttrs(o, DTA_SourceType, (IPTR)&sourcetype,
                      DTA_Handle,     (IPTR)&handle,
                      TAG_DONE) != 2)
    {
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        return FALSE;
    }

    if ((sourcetype != DTST_FILE) && (sourcetype != DTST_CLIPBOARD))
    {
        SetIoErr(ERROR_NOT_IMPLEMENTED);
        return FALSE;
    }

    if (!handle)
    {
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        return FALSE;
    }

    ctx.mp3_File  = handle;
    ctx.mp3_InBuf = AllocVec(MP3_INPUT_BUFFER_SIZE, MEMF_PUBLIC);
    if (!ctx.mp3_InBuf)
    {
        SetIoErr(ERROR_NO_FREE_STORE);
        return FALSE;
    }

    /* Rewind in case the datatypes framework left the handle past the
     * file's magic / id3 tag header. */
    Seek(handle, 0, OFFSET_BEGINNING);

    /* Skip ID3v2 tag if present.  Header is "ID3" + ver(2) + flags(1) +
     * syncsafe size(4), payload is 'size' bytes that follow.  libmad will
     * eventually resync past one via LOSTSYNC errors, but a large embedded
     * cover-art tag can waste a lot of time, so skip it explicitly.
     */
    {
        UBYTE id3hdr[10];
        if (Read(handle, id3hdr, 10) == 10
            && id3hdr[0] == 'I' && id3hdr[1] == 'D' && id3hdr[2] == '3'
            && id3hdr[3] != 0xFF && id3hdr[4] != 0xFF
            && (id3hdr[6] & 0x80) == 0
            && (id3hdr[7] & 0x80) == 0
            && (id3hdr[8] & 0x80) == 0
            && (id3hdr[9] & 0x80) == 0)
        {
            ULONG id3size = ((ULONG)id3hdr[6] << 21)
                          | ((ULONG)id3hdr[7] << 14)
                          | ((ULONG)id3hdr[8] <<  7)
                          | ((ULONG)id3hdr[9]);
            D(bug("[mp3.dt] skipping ID3v2 tag of %lu bytes\n", id3size));
            Seek(handle, id3size, OFFSET_CURRENT);
        }
        else
        {
            Seek(handle, 0, OFFSET_BEGINNING);
        }
    }

    mad_decoder_init(&decoder, &ctx,
                     mp3_Input, NULL /* header */, NULL /* filter */,
                     mp3_Output, mp3_Error, NULL /* message */);

    err = mad_decoder_run(&decoder, MAD_DECODER_MODE_SYNC);

    mad_decoder_finish(&decoder);

    FreeVec(ctx.mp3_InBuf);
    ctx.mp3_InBuf = NULL;

    if (ctx.mp3_Error || ctx.mp3_PCMUsed == 0 || ctx.mp3_SampleRate == 0)
    {
        D(bug("[mp3.dt] decode failed (err=%ld used=%lu rate=%lu)\n",
              err, ctx.mp3_PCMUsed, ctx.mp3_SampleRate));
        if (ctx.mp3_PCM) FreeVec(ctx.mp3_PCM);
        SetIoErr(ERROR_OBJECT_WRONG_TYPE);
        return FALSE;
    }

    sampletype = (ctx.mp3_Channels == 2) ? SDTST_S16S : SDTST_M16S;
    framecount = ctx.mp3_PCMUsed
               / (sizeof(WORD) * ctx.mp3_Channels);

    D(bug("[mp3.dt] decoded %lu frames, %lu Hz, %s 16-bit\n",
          framecount, ctx.mp3_SampleRate,
          (ctx.mp3_Channels == 2) ? "stereo" : "mono"));

    SetDTAttrs(o, NULL, NULL,
               DTA_ObjName,        (IPTR)"Unknown",
               SDTA_Sample,        (IPTR)ctx.mp3_PCM,
               SDTA_SampleLength,  framecount,
               SDTA_SampleType,    sampletype,
               SDTA_SamplesPerSec, ctx.mp3_SampleRate,
               SDTA_Period,        709379UL * 5 / ctx.mp3_SampleRate,
               SDTA_Volume,        64,
               SDTA_Cycles,        1,
               TAG_DONE);

    return TRUE;
}

/**************************************************************************/

IPTR MP3__OM_NEW(Class *cl, Object *o, struct opSet *msg)
{
    IPTR retval;

    retval = DoSuperMethodA(cl, o, (Msg)msg);
    if (retval)
    {
        if (!ReadMP3(cl, (Object *)retval))
        {
            CoerceMethod(cl, (Object *)retval, OM_DISPOSE);
            retval = 0;
        }
    }

    return retval;
}

/**************************************************************************/
