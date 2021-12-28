

/* XXX Draw a test pattern, getting colors right, for real now...
 */
 
static void fill(uint8_t x,uint8_t y,uint8_t w,uint8_t h,uint8_t pixel) {
  uint8_t *dst=(uint8_t*)fb.v+y*96+x;
  for (;h-->0;dst+=96) {
    memset(dst,pixel,w);
  }
}
 
#define RED_RAMP 1
#define GREEN_RAMP 2
#define BLUE_RAMP 3
#define YELLOW_RAMP 4
#define PURPLE_RAMP 5
#define CYAN_RAMP 6
#define GRAY_RAMP 7
#define PURE_BLUE 10
#define PURE_GREEN 11
#define MUTT_SPREAD_1 12
#define MUTT_SPREAD_2 13
#define MUTT_SPREAD_3 14
#define MUTT_SPREAD_4 15
#define MUTT_SPREAD_5 16
#define MUTT_SPREAD_6 17
#define MUTT_SPREAD_7 18
#define MUTT_SPREAD_8 19
#define MUTT_SPREAD_9 20
#define MUTT_SPREAD_10 21
#define MUTT_SPREAD_11 22
#define MUTT_SPREAD_12 23
 
static void draw_test_pattern() {
  memset(fb.v,0,96*64);
  switch (MUTT_SPREAD_12) {
    
    case RED_RAMP: { // ok
        fill( 0, 0, 9,64,0x00);
        fill( 9, 0, 9,64,0x01);
        fill(18, 0, 9,64,0x02);
        fill(27, 0, 9,64,0x03);
        fill(36, 0, 9,64,0x27);
        fill(45, 0, 9,64,0x4b);
        fill(54, 0, 9,64,0x6f);
        fill(63, 0, 9,64,0x93);
        fill(72, 0, 9,64,0xb7);
        fill(81, 0, 9,64,0xdb);
        fill(90, 0, 6,64,0xff);
      } break;
      
    case GREEN_RAMP: { // ok
        fill( 0, 0, 6,64,0x00);
        fill( 6, 0, 6,64,0x04);
        fill(12, 0, 6,64,0x08);
        fill(18, 0, 6,64,0x0c);
        fill(24, 0, 6,64,0x10);
        fill(30, 0, 6,64,0x14);
        fill(36, 0, 6,64,0x18);
        fill(42, 0, 6,64,0x1c);
        fill(48, 0, 6,64,0x1c);
        fill(54, 0, 6,64,0x3c);
        fill(60, 0, 6,64,0x5d);
        fill(66, 0, 6,64,0x7d);
        fill(72, 0, 6,64,0x9e);
        fill(78, 0, 6,64,0xbe);
        fill(84, 0, 6,64,0xdf);
        fill(90, 0, 6,64,0xff);
      } break;
      
    case BLUE_RAMP: { // ok
        fill( 0, 0, 6,64,0x00);
        fill( 6, 0, 6,64,0x20);
        fill(12, 0, 6,64,0x40);
        fill(18, 0, 6,64,0x60);
        fill(24, 0, 6,64,0x80);
        fill(30, 0, 6,64,0xa0);
        fill(36, 0, 6,64,0xc0);
        fill(42, 0, 6,64,0xe0);
        fill(48, 0, 6,60,0xe0);
        fill(54, 0, 6,64,0xe4);
        fill(60, 0, 6,62,0xe9);
        fill(66, 0, 6,64,0xed);
        fill(72, 0, 6,62,0xf2);
        fill(78, 0, 6,64,0xf6);
        fill(84, 0, 6,62,0xfb);
        fill(90, 0, 6,64,0xff);
      } break;
      
    case YELLOW_RAMP: { // ok
        fill( 0, 0, 6,64,0x00);
        fill( 6, 0, 6,62,0x04);
        fill(12, 0, 6,64,0x09);
        fill(18, 0, 6,62,0x0d);
        fill(24, 0, 6,64,0x12);
        fill(30, 0, 6,62,0x16);
        fill(36, 0, 6,64,0x1b);
        fill(42, 0, 6,64,0x1f);
        fill(48, 0, 6,60,0x1f);
        fill(54, 0, 6,64,0x3f);
        fill(60, 0, 6,62,0x5f);
        fill(66, 0, 6,64,0x7f);
        fill(72, 0, 6,62,0x9f);
        fill(78, 0, 6,64,0xbf);
        fill(84, 0, 6,62,0xdf);
        fill(90, 0, 6,64,0xff);
      } break;
      
    case CYAN_RAMP: { // ok
        fill( 0, 0, 6,64,0x00);
        fill( 6, 0, 6,62,0x24);
        fill(12, 0, 6,64,0x48);
        fill(18, 0, 6,62,0x6c);
        fill(24, 0, 6,64,0x90);
        fill(30, 0, 6,62,0xb4);
        fill(36, 0, 6,64,0xd8);
        fill(42, 0, 6,64,0xfc);
        fill(48, 0,12,60,0xfc);
        fill(60, 0,12,64,0xfd);
        fill(72, 0,12,62,0xfe);
        fill(84, 0,12,64,0xff);
      } break;
      
    case PURPLE_RAMP: { // ok
        fill( 0, 0, 6,64,0x00);
        fill( 6, 0, 6,62,0x20);
        fill(12, 0, 6,64,0x41);
        fill(18, 0, 6,62,0x61);
        fill(24, 0, 6,64,0x82);
        fill(30, 0, 6,62,0xa2);
        fill(36, 0, 6,64,0xc3);
        fill(42, 0, 6,64,0xe3);
        fill(48, 0, 6,60,0xe3);
        fill(54, 0, 6,64,0xe7);
        fill(60, 0, 6,62,0xeb);
        fill(66, 0, 6,64,0xef);
        fill(72, 0, 6,62,0xf3);
        fill(78, 0, 6,64,0xf7);
        fill(84, 0, 6,62,0xfb);
        fill(90, 0, 6,64,0xff);
      } break;
      
    case GRAY_RAMP: { // ok (mostly not "gray" but it matches)
        fill( 0, 0,12,64,0x00);
        fill(12, 0,12,64,0x24);
        fill(24, 0,12,62,0x49);
        fill(36, 0,12,64,0x6d);
        fill(48, 0,12,64,0x92);
        fill(60, 0,12,62,0xb6);
        fill(72, 0,12,64,0xdb);
        fill(84, 0,12,64,0xff);
      } break;
    
    case PURE_BLUE: {
        memset(fb.v,0xe0,96*64);
      } break;
      
    case PURE_GREEN: {
        memset(fb.v,0x20,96*32);
        memset(fb.v+96*32,0x1c,96*32);
      } break;
      
    // Then machine-generated screens of 16 colors each, for all the ones I missed...
      
    case MUTT_SPREAD_1: { // ok
        fill( 0, 0,24,16,0x05);
        fill(24, 0,24,16,0x06);
        fill(48, 0,24,16,0x07);
        fill(72, 0,24,16,0x0a);
        fill( 0,16,24,16,0x0b);
        fill(24,16,24,16,0x0e);
        fill(48,16,24,16,0x0f);
        fill(72,16,24,16,0x11);
        fill( 0,32,24,16,0x13);
        fill(24,32,24,16,0x15);
        fill(48,32,24,16,0x17);
        fill(72,32,24,16,0x19);
        fill( 0,48,24,16,0x1a);
        fill(24,48,24,16,0x1d);
        fill(48,48,24,16,0x1e);
        fill(72,48,24,16,0x21);
      } break;
      
    case MUTT_SPREAD_2: { // ok
        fill( 0, 0,24,16,0x22);
        fill(24, 0,24,16,0x23);
        fill(48, 0,24,16,0x25);
        fill(72, 0,24,16,0x26);
        fill( 0,16,24,16,0x28);
        fill(24,16,24,16,0x29);
        fill(48,16,24,16,0x2a);
        fill(72,16,24,16,0x2b);
        fill( 0,32,24,16,0x2c);
        fill(24,32,24,16,0x2d);
        fill(48,32,24,16,0x2e);
        fill(72,32,24,16,0x2f);
        fill( 0,48,24,16,0x30);
        fill(24,48,24,16,0x31);
        fill(48,48,24,16,0x32);
        fill(72,48,24,16,0x33);
      } break;
      
    case MUTT_SPREAD_3: { // ok
        fill( 0, 0,24,16,0x34);
        fill(24, 0,24,16,0x35);
        fill(48, 0,24,16,0x36);
        fill(72, 0,24,16,0x37);
        fill( 0,16,24,16,0x38);
        fill(24,16,24,16,0x39);
        fill(48,16,24,16,0x3a);
        fill(72,16,24,16,0x3b);
        fill( 0,32,24,16,0x3d);
        fill(24,32,24,16,0x3e);
        fill(48,32,24,16,0x42);
        fill(72,32,24,16,0x43);
        fill( 0,48,24,16,0x44);
        fill(24,48,24,16,0x45);
        fill(48,48,24,16,0x46);
        fill(72,48,24,16,0x47);
      } break;
      
    case MUTT_SPREAD_4: { // ok
        fill( 0, 0,24,16,0x4a);
        fill(24, 0,24,16,0x4c);
        fill(48, 0,24,16,0x4d);
        fill(72, 0,24,16,0x4e);
        fill( 0,16,24,16,0x4f);
        fill(24,16,24,16,0x50);
        fill(48,16,24,16,0x51);
        fill(72,16,24,16,0x52);
        fill( 0,32,24,16,0x53);
        fill(24,32,24,16,0x54);
        fill(48,32,24,16,0x55);
        fill(72,32,24,16,0x56);
        fill( 0,48,24,16,0x57);
        fill(24,48,24,16,0x58);
        fill(48,48,24,16,0x59);
        fill(72,48,24,16,0x5a);
      } break;
      
    case MUTT_SPREAD_5: { // ok
        fill( 0, 0,24,16,0x5b);
        fill(24, 0,24,16,0x5c);
        fill(48, 0,24,16,0x5e);
        fill(72, 0,24,16,0x62);
        fill( 0,16,24,16,0x63);
        fill(24,16,24,16,0x64);
        fill(48,16,24,16,0x65);
        fill(72,16,24,16,0x66);
        fill( 0,32,24,16,0x67);
        fill(24,32,24,16,0x68);
        fill(48,32,24,16,0x69);
        fill(72,32,24,16,0x6a);
        fill( 0,48,24,16,0x6b);
        fill(24,48,24,16,0x6e);
        fill(48,48,24,16,0x70);
        fill(72,48,24,16,0x71);
      } break;
      
    case MUTT_SPREAD_6: { // ok
        fill( 0, 0,24,16,0x72);
        fill(24, 0,24,16,0x73);
        fill(48, 0,24,16,0x74);
        fill(72, 0,24,16,0x75);
        fill( 0,16,24,16,0x76);
        fill(24,16,24,16,0x77);
        fill(48,16,24,16,0x78);
        fill(72,16,24,16,0x79);
        fill( 0,32,24,16,0x7a);
        fill(24,32,24,16,0x7b);
        fill(48,32,24,16,0x7c);
        fill(72,32,24,16,0x7e);
        fill( 0,48,24,16,0x81);
        fill(24,48,24,16,0x83);
        fill(48,48,24,16,0x84);
        fill(72,48,24,16,0x85);
      } break;
      
    case MUTT_SPREAD_7: { // ok
        fill( 0, 0,24,16,0x86);
        fill(24, 0,24,16,0x87);
        fill(48, 0,24,16,0x88);
        fill(72, 0,24,16,0x89);
        fill( 0,16,24,16,0x8a);
        fill(24,16,24,16,0x8b);
        fill(48,16,24,16,0x8c);
        fill(72,16,24,16,0x8d);
        fill( 0,32,24,16,0x8e);
        fill(24,32,24,16,0x8f);
        fill(48,32,24,16,0x91);
        fill(72,32,24,16,0x94);
        fill( 0,48,24,16,0x95);
        fill(24,48,24,16,0x96);
        fill(48,48,24,16,0x97);
        fill(72,48,24,16,0x98);
      } break;
      
    case MUTT_SPREAD_8: { // ok
        fill( 0, 0,24,16,0x99);
        fill(24, 0,24,16,0x9a);
        fill(48, 0,24,16,0x9b);
        fill(72, 0,24,16,0x9c);
        fill( 0,16,24,16,0x9d);
        fill(24,16,24,16,0xa1);
        fill(48,16,24,16,0xa3);
        fill(72,16,24,16,0xa4);
        fill( 0,32,24,16,0xa5);
        fill(24,32,24,16,0xa6);
        fill(48,32,24,16,0xa7);
        fill(72,32,24,16,0xa8);
        fill( 0,48,24,16,0xa9);
        fill(24,48,24,16,0xaa);
        fill(48,48,24,16,0xab);
        fill(72,48,24,16,0xac);
      } break;
      
    case MUTT_SPREAD_9: { // ok
        fill( 0, 0,24,16,0xad);
        fill(24, 0,24,16,0xae);
        fill(48, 0,24,16,0xaf);
        fill(72, 0,24,16,0xb0);
        fill( 0,16,24,16,0xb1);
        fill(24,16,24,16,0xb2);
        fill(48,16,24,16,0xb3);
        fill(72,16,24,16,0xb5);
        fill( 0,32,24,16,0xb8);
        fill(24,32,24,16,0xb9);
        fill(48,32,24,16,0xba);
        fill(72,32,24,16,0xbb);
        fill( 0,48,24,16,0xbc);
        fill(24,48,24,16,0xbd);
        fill(48,48,24,16,0xc1);
        fill(72,48,24,16,0xc2);
      } break;
      
    case MUTT_SPREAD_10: { // ok
        fill( 0, 0,24,16,0xc4);
        fill(24, 0,24,16,0xc5);
        fill(48, 0,24,16,0xc6);
        fill(72, 0,24,16,0xc7);
        fill( 0,16,24,16,0xc8);
        fill(24,16,24,16,0xc9);
        fill(48,16,24,16,0xca);
        fill(72,16,24,16,0xcb);
        fill( 0,32,24,16,0xcc);
        fill(24,32,24,16,0xcd);
        fill(48,32,24,16,0xce);
        fill(72,32,24,16,0xcf);
        fill( 0,48,24,16,0xd0);
        fill(24,48,24,16,0xd1);
        fill(48,48,24,16,0xd2);
        fill(72,48,24,16,0xd3);
      } break;
      
    case MUTT_SPREAD_11: { // ok
        fill( 0, 0,24,16,0xd4);
        fill(24, 0,24,16,0xd5);
        fill(48, 0,24,16,0xd6);
        fill(72, 0,24,16,0xd7);
        fill( 0,16,24,16,0xd9);
        fill(24,16,24,16,0xda);
        fill(48,16,24,16,0xdc);
        fill(72,16,24,16,0xdd);
        fill( 0,32,24,16,0xde);
        fill(24,32,24,16,0xe1);
        fill(48,32,24,16,0xe2);
        fill(72,32,24,16,0xe5);
        fill( 0,48,24,16,0xe6);
        fill(24,48,24,16,0xe8);
        fill(48,48,24,16,0xea);
        fill(72,48,24,16,0xec);
      } break;
      
    case MUTT_SPREAD_12: {
        fill( 0, 0,24,16,0xee);
        fill(24, 0,24,16,0xf0);
        fill(48, 0,24,16,0xf1);
        fill(72, 0,24,16,0xf4);
        fill( 0,16,24,16,0xf5);
        fill(24,16,24,16,0xf8);
        fill(48,16,24,16,0xf9);
        fill(72,16,24,16,0xfa);
    } break;
      
  }
}
