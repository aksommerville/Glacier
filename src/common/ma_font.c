#include "multiarcade.h"

/* Render glyph.
 */
 
static void ma_glyph_render(
  struct ma_framebuffer *dst,
  int16_t dstx,int16_t dsty,
  int8_t w,int8_t h,
  uint32_t bits,
  ma_pixel_t pixel
) {
  int8_t srcstride=w;
  if (dstx<0) {
    if ((w+=dstx)<1) return;
    bits<<=-dstx;
    dstx=0;
  }
  if (dstx>dst->w-w) {
    if ((w=dst->w-dstx)<1) return;
  }
  if (dsty<0) {
    if ((h+=dsty)<1) return;
    bits<<=-dsty*srcstride;
    dsty=0;
  }
  if (dsty>dst->h-h) {
    if ((h=dst->h-dsty)<1) return;
  }
  ma_pixel_t *dstrow=dst->v+dsty*dst->stride+dstx;
  for (;h-->0;dstrow+=dst->stride,bits<<=srcstride) {
    ma_pixel_t *dstp=dstrow;
    uint32_t bitsp=bits;
    int8_t xi=w;
    for (;xi-->0;dstp++,bitsp<<=1) {
      if (bitsp&0x80000000) *dstp=pixel;
    }
  }
}

/* Render line of text.
 */
 
int16_t ma_font_render(
  struct ma_framebuffer *dst,
  int16_t dstx,int16_t dsty,
  const struct ma_font *font,
  const char *src,int16_t srcc,
  ma_pixel_t pixel
) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int16_t rtn=0;
  for (;srcc-->0;src++) {
  
    // Skip anything outside G0*, and shuffle from codepoint to index.
    // [*] We do allow 0x7f DEL, and 0x20 SPC which I don't know if it's strictly "G0" or "C0", whatever.
    uint8_t p=(*src)-0x20;
    if (p>=0x60) continue;
    
    uint8_t metrics=font->metrics[p];
    uint32_t bits=font->bits[p];
    uint8_t w=metrics>>5;
    if (!w) continue; // Important: Zero-width glyphs do not cause implicit advancement either.
    uint8_t h=(metrics>>2)&7;
    uint8_t y=metrics&3;
    
    ma_glyph_render(dst,dstx,dsty+y,w,h,bits,pixel);
    
    w+=1;
    dstx+=w;
    rtn+=w;
  }
  return rtn;
}

/* Measure line of text.
 */

int16_t ma_font_measure(
  const struct ma_font *font,
  const char *src,int16_t srcc
) {
  int16_t rtn=0;
  for (;srcc-->0;src++) {
    uint8_t p=(*src)-0x20;
    if (p>=0x60) continue;
    uint8_t metrics=font->metrics[p];
    uint8_t w=metrics>>5;
    if (!w) continue; // Important: Zero-width glyphs do not cause implicit advancement either.
    rtn+=w+1;
  }
  return rtn;
}
