#include "ps_macwm_internal.h"
#include <Carbon/Carbon.h>

/* Decode UTF-8.
 */

int ps_macwm_decode_utf8(int *dst,const void *src,int srcc) {
  if (!dst||!src||!srcc) return -1;
  if (srcc<0) srcc=4;
  const unsigned char *SRC=src;
  if (!(SRC[0]&0x80)) {
    *dst=SRC[0];
    return 1;
  }
  if (!(SRC[0]&0x40)) {
    return -1;
  }
  if (!(SRC[0]&0x20)) {
    if (srcc<2) return -1;
    if ((SRC[1]&0xc0)!=0x80) return -1;
    *dst=((SRC[0]&0x1f)<<6)|(SRC[1]&0x3f);
    return 2;
  }
  if (!(SRC[0]&0x10)) {
    if (srcc<3) return -1;
    if ((SRC[1]&0xc0)!=0x80) return -1;
    if ((SRC[2]&0xc0)!=0x80) return -1;
    *dst=((SRC[0]&0x0f)<<12)|((SRC[1]&0x3f)<<6)|(SRC[2]&0x3f);
    return 3;
  }
  if (!(SRC[0]&0x08)) {
    if (srcc<4) return -1;
    if ((SRC[1]&0xc0)!=0x80) return -1;
    if ((SRC[2]&0xc0)!=0x80) return -1;
    if ((SRC[3]&0xc0)!=0x80) return -1;
    *dst=((SRC[0]&0x1f)<<18)|((SRC[1]&0x3f)<<12)|((SRC[2]&0x3f)<<6)|(SRC[3]&0x3f);
    return 4;
  }
  return -1;
}

/* Unicode-to-unicode, smoothing out some Mac quirks.
 */

int ps_macwm_translate_codepoint(int src) {
  switch (src) {

    // We prefer LF over CR.
    case 0x0d: return 0x0a;

    // Macs report Shift+Tab as U+19 (End of Medium), which is cool but not if they're the only ones doing it.
    case 0x19: return 0x09;

    // Macs report BACKSPACE as DELETE for some reason...
    case 0x7f: return 0x08;

    // ...and they make something else up entirely for DELETE. Nice one, Mac.
    case 0xf728: return 0x7f;

    default: {
        if ((src>=0xe000)&&(src<0xf900)) {
          // Unicode "Private Use Area", used extensively by Apple for things like F-keys and arrows.
          // Our scope here is text only, so we ignore them.
          //akg_log(MACOS,TRACE,"Discarding Private Use keyDown U+%04x",src);
          return 0;
        }
      }
  }
  return src;
}

/* MacOS to USB-HID keysyms.
 */
 
int ps_macwm_translate_keysym(int key) {
  switch (key) {
    #define _(macname,usage) case kVK_##macname: return 0x0007##usage;
    _(ANSI_A,0004)
    _(ANSI_B,0005)
    _(ANSI_C,0006)
    _(ANSI_D,0007)
    _(ANSI_E,0008)
    _(ANSI_F,0009)
    _(ANSI_G,000a)
    _(ANSI_H,000b)
    _(ANSI_I,000c)
    _(ANSI_J,000d)
    _(ANSI_K,000e)
    _(ANSI_L,000f)
    _(ANSI_M,0010)
    _(ANSI_N,0011)
    _(ANSI_O,0012)
    _(ANSI_P,0013)
    _(ANSI_Q,0014)
    _(ANSI_R,0015)
    _(ANSI_S,0016)
    _(ANSI_T,0017)
    _(ANSI_U,0018)
    _(ANSI_V,0019)
    _(ANSI_W,001a)
    _(ANSI_X,001b)
    _(ANSI_Y,001c)
    _(ANSI_Z,001d)
    _(ANSI_0,0027)
    _(ANSI_1,001e)
    _(ANSI_2,001f)
    _(ANSI_3,0020)
    _(ANSI_4,0021)
    _(ANSI_5,0022)
    _(ANSI_6,0023)
    _(ANSI_7,0024)
    _(ANSI_8,0025)
    _(ANSI_9,0026)
    _(Return,0028)
    _(Escape,0029)
    _(Delete,002a)
    _(Tab,002b)
    _(Space,002c)
    _(ANSI_Minus,002d)
    _(ANSI_Equal,002e)
    _(ANSI_LeftBracket,002f)
    _(ANSI_RightBracket,0030)
    _(ANSI_Backslash,0031)
    _(ANSI_Semicolon,0033)
    _(ANSI_Quote,0034)
    _(ANSI_Grave,0035)
    _(ANSI_Comma,0036)
    _(ANSI_Period,0037)
    _(ANSI_Slash,0038)
    _(CapsLock,0039)
    _(F1,003a)
    _(F2,003b)
    _(F3,003c)
    _(F4,003d)
    _(F5,003e)
    _(F6,003f)
    _(F7,0040)
    _(F8,0041)
    _(F9,0042)
    _(F10,0043)
    _(F11,0044)
    _(F12,0045)
    _(Home,004a)
    _(PageUp,004b)
    _(ForwardDelete,004c)
    _(End,004d)
    _(PageDown,004e)
    _(RightArrow,004f)
    _(LeftArrow,0050)
    _(DownArrow,0051)
    _(UpArrow,0052)
    _(ANSI_KeypadDivide,0054)
    _(ANSI_KeypadMultiply,0055)
    _(ANSI_KeypadMinus,0056)
    _(ANSI_KeypadPlus,0057)
    _(ANSI_KeypadEnter,0058)
    _(ANSI_Keypad1,0059)
    _(ANSI_Keypad2,005a)
    _(ANSI_Keypad3,005b)
    _(ANSI_Keypad4,005c)
    _(ANSI_Keypad5,005d)
    _(ANSI_Keypad6,005e)
    _(ANSI_Keypad7,005f)
    _(ANSI_Keypad8,0060)
    _(ANSI_Keypad9,0061)
    _(ANSI_Keypad0,0062)
    _(ANSI_KeypadDecimal,0063)
    _(ANSI_KeypadEquals,0067)
    _(F13,0068)
    _(F14,0069)
    _(F15,006a)
    _(Control,00e0)
    _(Shift,00e1)
    _(Option,00e2)
    _(Command,00e3)
    _(RightControl,00e4)
    _(RightShift,00e5)
    _(RightOption,00e6)
    //_(RightCommand,00e7) // No such thing as RightCommand?
    #undef _
  }
  if (key&0xffff0000) return 0;
  return 0xffff0000|key;
}

/* Modifier keys.
 */

int ps_macwm_translate_modifier(int src) {
  switch (src) {
    case 0x00000002: return 0x000700e1; // l shift
    case 0x00800000: return 0x00070076; // fn ("Keyboard Menu" in HID)
    case 0x00000001: return 0x000700e0; // l control
    case 0x00000020: return 0x000700e2; // l option
    case 0x00000008: return 0x000700e3; // l command
    case 0x00000010: return 0x000700e7; // r command
    case 0x00000040: return 0x000700e6; // r option
    case 0x00000004: return 0x000700e5; // r shift
  }
  return 0;
}
