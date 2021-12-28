/* multiarcade.h
 */
 
#ifndef MULTIARCADE_H
#define MULTIARCADE_H

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
  extern "C" {
#endif

/* Pixel size for all images must be established at compile time,
 * in the interest of reducing code size.
 * May be 8, 16, or 32. TinyArcade only supports 8 or 16.
 */
#define MA_PIXELSIZE 8
#if MA_PIXELSIZE==8
  typedef uint8_t ma_pixel_t;
#elif MA_PIXELSIZE==16
  typedef uint16_t ma_pixel_t;
#elif MA_PIXELSIZE==32
  typedef uint32_t ma_pixel_t;
#else
  #error "Illegal value for MA_PIXELSIZE"
#endif

#if MA_NON_TINY
  #include <stdio.h>
  #define ma_log(fmt,...) fprintf(stderr,fmt,##__VA_ARGS__)
#else
  #define ma_log(fmt,...)
#endif

/* You must implement these.
 * Declared here as commentary.
 ***************************************************************/
 
void setup();
void loop();
int16_t audio_next();

/* Call from setup() to bring hardware online.
 ***************************************************************/
 
struct ma_init_params {

  int16_t videow,videoh; // pixels; 0=>96,64
  uint8_t rate; // Hz, 0=>max, unregulated

  uint32_t audio_rate; // Hz, 0=>disable
};

/* Bring everything online.
 * Null or zeroed (params) uses sensible defaults.
 * If present, we fill (params) with the actual config.
 * Nonzero on success.
 */
uint8_t ma_init(struct ma_init_params *params);

/* Routine maintenance, things you should do during loop().
 **************************************************************/
 
#define MA_BUTTON_LEFT       0x0001
#define MA_BUTTON_RIGHT      0x0002
#define MA_BUTTON_UP         0x0004
#define MA_BUTTON_DOWN       0x0008
#define MA_BUTTON_A          0x0010
#define MA_BUTTON_B          0x0020

/* Gather input, poll systems that need it, sleep if we're configured to regulate timing.
 * Returns input state (bitfields MA_BUTTON_*).
 */
uint16_t ma_update();

/* Dimensions of (fb) must match init params with no padding.
 */
void ma_send_framebuffer(const void *fb);

/* Incidental ops.
 *************************************************************/

/* Read or write a regular file in one shot.
 * When reading, you provide a preallocated buffer.
 * We return the length filled. If the file is longer, you won't know.
 * (path) is from the root of TinyArcade's SD card, or 
 * from some externally-configured sandbox for native builds.
 */
int32_t ma_file_read(void *dst,int32_t dsta,const char *path,int32_t seek);
int32_t ma_file_write(const char *path,const void *src,int32_t srcc,int32_t seek);

/* Arduino functions that we imitate if needed.
 **************************************************************/
 
uint32_t millis();
uint32_t micros();
void delay(uint32_t ms);

/* Software rendering.
 * There are basically two kinds of image:
 *  - framebuffer: 8, 16, or 32 bits. Typically 2 of them: background and work space.
 *  - texture: Always 2-bit pixels with a per-image color table.
 * Texture width must be a multiple of 4 (to force byte alignment of rows).
 * Texture pixels are stored big-endianly: 0xc0,0x30,0x0c,0x03,etc
 * Only framebuffer can be blitted to; only texture can be blitted from.
 ***************************************************************/
 
#define MA_XFORM_NONE    0x00
#define MA_XFORM_XREV    0x01
#define MA_XFORM_YREV    0x02
#define MA_XFORM_SWAP    0x04
 
#define MA_TEXTURE_TRANSPARENCY    0x80 /* Natural zeroes are transparent. */
 
struct ma_framebuffer {
  int16_t w,h;
  int16_t stride; // pixels
  ma_pixel_t *v;
};

struct ma_texture {
  uint8_t w,h;
  uint8_t stride; // bytes; typically (w/4)
  uint8_t flags;
  ma_pixel_t ctab[4];
  uint8_t *v;
};

void ma_framebuffer_fill_rect(
  struct ma_framebuffer *dst,
  int16_t x,int16_t y,int16_t w,int16_t h,
  ma_pixel_t pixel
);

void ma_blit(
  struct ma_framebuffer *dst,
  int16_t dstx,int16_t dsty,
  const struct ma_texture *src,
  uint8_t xform
);

// Helper to apply incremental transforms.
// eg ADJUST_X does not just toggle XREV; it flips whatever currently appears to be the X axis.
#define MA_XFORM_ADJUST_X   1
#define MA_XFORM_ADJUST_Y   2
#define MA_XFORM_ADJUST_90  3
#define MA_XFORM_ADJUST_180 4
#define MA_XFORM_ADJUST_270 5
uint8_t ma_xform_adjust(uint8_t prev,uint8_t adjustment);

// RGB estimate of the TinyArcade's 8-bit color table.
// The docs say it's BGR332, and it's close, but there is some obvious nonlinearity.
//TODO Can we get something more official? I'm generating this list by sight.
extern const uint8_t ma_ctab8[768];

#if MA_PIXELSIZE==8
  #define MA_PIXEL(r,g,b) ((r>>6)|((g&0xe0)>>3)|(b&0xe0))
#else
  #define MA_PIXEL(r,g,b) (((r&0xf8)<<5)|((g&0x1c)<<11)|(g>>5)|(b&0xf8))
#endif

static inline ma_pixel_t ma_pixel_from_rgb(const uint8_t *rgb) {
  return MA_PIXEL(rgb[0],rgb[1],rgb[2]);
  #if MA_PIXELSIZE==8
    return (rgb[0]>>6)|((rgb[1]&0xe0)>>3)|(rgb[2]&0xe0);
  #elif MA_PIXELSIZE==16
    // 16-bit BGR565 but always big-endian: It's backward on little-endian machines (like the Tiny).
    #if 0 //TODO big-endian host
      return (rgb[0]>>3)|((rgb[1]&0xfc)>>1)|((rgb[2]&0xf8)<<8);
    #else
      return ((rgb[0]&0xf8)<<5)|((rgb[1]&0x1c)<<11)|(rgb[1]>>5)|(rgb[2]&0xf8);
    #endif
  #endif
}

static inline void ma_rgb_from_pixel(uint8_t *rgb,ma_pixel_t pixel) {
  #if MA_PIXELSIZE==8
    //rgb[0]=pixel<<6; if (rgb[0]&0x40) rgb[0]|=0x20; rgb[0]|=rgb[0]>>3; rgb[0]|=rgb[0]>>6;
    //rgb[1]=pixel&0x1c; rgb[1]|=rgb[1]<<3; rgb[1]|=rgb[1]>>6;
    //rgb[2]=pixel&0xe0; rgb[2]|=rgb[2]>>3; rgb[2]|=rgb[2]>>6;
    memcpy(rgb,ma_ctab8+pixel*3,3);
  #elif MA_PIXELSIZE==16
    rgb[0]=(pixel&0x1f00)>>5; rgb[0]|=rgb[0]>>5;
    rgb[1]=((pixel&0xe000)>>11)|((pixel&0x0007)<<5); rgb[1]|=rgb[1]>>6;
    rgb[2]=pixel&0x00f8; rgb[2]|=rgb[2]>>5;
  #endif
}

/* Structured tile rendering.
 * You must maintain one 2-bit image whose dimensions are both (tilesize*16), and minimum stride.
 * Also maintain a 256-entry tile list for translating grid values.
 * For each tile, you supply (xform) and (ctab) fresh.
 *************************************************************/
 
struct ma_tile {
  uint8_t tileid;
  uint8_t xform; // MA_XFORM_*|MA_TEXTURE_TRANSPARENCY
  ma_pixel_t ctab[4];
};

#define MA_GRID_BG_NONE  0x0000
#define MA_GRID_BG_CELL  0x0100 /* |cell */
#define MA_GRID_BG_COLOR 0x0200 /* |pixel */
 
void ma_render_grid(
  struct ma_framebuffer *dst,
  const uint8_t *src,
  uint8_t colc,uint8_t rowc,
  uint16_t stride,
  const struct ma_tile *tilemap,
  const void *pixels,
  uint8_t tilesize,
  uint16_t bg
);

void ma_render_sprite(
  struct ma_framebuffer *dst,
  int16_t dstx,int16_t dsty,
  const struct ma_tile *tile,
  const void *pixels,
  uint8_t tilesize
);

#define MA_DECLARE_TILESHEET(name,tilesize) \
  uint8_t name[(tilesize>>2)*16*tilesize*16];

/* Font.
 * We only support ASCII G0 (0x20..0x7f), glyphs up to 7x7 pixels.
 * (actually, the total pixel count in a glyph must be <=32, so 5x6 is about it).
 **********************************************************/
 
struct ma_font {
  uint8_t metrics[0x60]; // (w<<5)|(h<<2)|y
  uint32_t bits[0x60]; // LRTB big-endianly
};

/* Render text in one color.
 * No line breaking; do that yourself before calling.
 * (dstx,dsty) is the top-left corner of the first glyph.
 * Returns the total advancement, including 1 blank column after the last glyph.
 */
int16_t ma_font_render(
  struct ma_framebuffer *dst,
  int16_t dstx,int16_t dsty,
  const struct ma_font *font,
  const char *src,int16_t srcc,
  ma_pixel_t pixel
);

int16_t ma_font_measure(
  const struct ma_font *font,
  const char *src,int16_t srcc
);

#ifdef __cplusplus
  }
#endif
#endif
