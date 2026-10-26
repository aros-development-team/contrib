/*
 *
 * yuv2overlay.c
 *
 */

#if defined(__AROS__)
#include "aros-inc.h"
#endif

#include <string.h>

#include "refresh_internal.h"

void yuv2overlay(rect_type *rect, unsigned char *src[])
{
  unsigned long yrow, uvrow, yadd, uvadd;
  unsigned long *dst, *py, *pu, *pv, y, u, v, cuv;
  int i, j;

  yrow = rect->src_row_length;
  uvrow = rect->src_row_length >> 1;
  yadd = rect->src_offset;
  uvadd = rect->src_uv_offset;

  cuv = rect->cuv ^ 0x80808080; /* Overlay wants it the other way around */

  dst = rect->dst;

  for (j=0; j<rect->yloop; j++) {
    if (rect->use_osd[j]) {
      py = ((unsigned long *)rect->osd[0]) + yadd;
      pu = ((unsigned long *)rect->osd[1]) + uvadd;
      pv = ((unsigned long *)rect->osd[2]) + uvadd;
    } else {
      py = ((unsigned long *)src[0]) + yadd;
      pu = ((unsigned long *)src[1]) + uvadd;
      pv = ((unsigned long *)src[2]) + uvadd;
    }

    for (i=0; i<(rect->xloop >> 3); i++) {
      y = *py++;
      u = *pu++ ^ cuv;
      v = *pv++ ^ cuv;
      *dst++ = (y&0xff000000) | ((u>>8)&0x00ff0000) | ((y>>8)&0x0000ff00) | (v>>24);
      *dst++ = ((y<<16)&0xff000000) | (u&0x00ff0000) | ((y<<8)&0x0000ff00) | ((v>>16)&0x000000ff);
      y = *py++;
      *dst++ = (y&0xff000000) | ((u<<8)&0x00ff0000) | ((y>>8)&0x0000ff00) | ((v>>8)&0x000000ff);
      *dst++ = ((y<<16)&0xff000000) | ((u<<16)&0x00ff0000) | ((y<<8)&0x0000ff00) | (v&0x000000ff);
    }

    yadd += yrow;
    uvadd += (uvrow * (j & 1));
  }
}

void yuv2overlay_gray(rect_type *rect, unsigned char *src[])
{
  unsigned long yrow, yadd;
  unsigned long *sptr, *dptr, col;
  int x, y, xloop;

  yrow = rect->src_row_length;
  yadd = rect->src_offset;

  dptr = rect->dst;

  xloop = rect->xloop >> 2;
  for (y = 0; y < rect->yloop; y++) {
    if (rect->use_osd[y]) {
      sptr = ((unsigned long *)rect->osd[0]) + yadd;
    } else {
      sptr = ((unsigned long *)src[0]) + yadd;
    }

    for (x = 0; x < xloop; x++) {
      col = *sptr++;
      *dptr++ = (col & 0xff000000) | ((col & 0x00ff0000) >> 8) | 0x00800080;
      *dptr++ = ((col & 0x0000ff00) << 16) | ((col & 0x000000ff) << 8) | 0x00800080;
    }

    yadd += yrow;
  }
}

static void i420_copy_y(rect_type *rect, unsigned char *src[])
{
  unsigned char *dst = (unsigned char *)rect->dst;
  int j;

  for (j=0; j<rect->height; j++) {
    unsigned char *py = rect->buf_use_osd[j] ? rect->osd[0] : src[0];

    memcpy(dst + j * rect->overlay_pitch, py + j * rect->line_width, rect->width);
  }
}

/* Planar overlay, the planes are copied as they come from the decoder */
void yuv2overlay_i420(rect_type *rect, unsigned char *src[])
{
  unsigned char *dst = (unsigned char *)rect->dst;
  unsigned char cuv = (rect->cuv ^ 0x80808080) & 0xff; /* Overlay wants it the other way around */
  int uvpitch = rect->overlay_pitch / 2, uvrow = rect->line_width / 2;
  int p, i, j;

  i420_copy_y(rect, src);

  for (p=1; p<3; p++) {
    unsigned char *pdst = dst + ((p == 1) ? rect->overlay_uoff : rect->overlay_voff);

    for (j=0; j<rect->height / 2; j++) {
      unsigned char *s = (rect->buf_use_osd[j * 2] ? rect->osd[p] : src[p]) + j * uvrow;
      unsigned char *d = pdst + j * uvpitch;

      if (cuv == 0) {
        memcpy(d, s, rect->width / 2);
      } else {
        for (i=0; i<rect->width / 2; i++) {
          d[i] = s[i] ^ cuv;
        }
      }
    }
  }
}

/* Chroma stays at the neutral value it was cleared to */
void yuv2overlay_i420_gray(rect_type *rect, unsigned char *src[])
{
  i420_copy_y(rect, src);
}
