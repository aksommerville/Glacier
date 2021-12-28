/* encoder.h
 * Helper for generating variable-length output.
 */

#ifndef ENCODER_H
#define ENCODER_H
 
struct encoder {
  char *v;
  int c,a;
};

void encoder_cleanup(struct encoder *encoder);
int encoder_require(struct encoder *encoder,int addc);
int encoder_append(struct encoder *encoder,const void *src,int srcc);
int encoder_appendf(struct encoder *encoder,const char *fmt,...);

int encode_u8(struct encoder *encoder,int src);
int encode_16be(struct encoder *encoder,int src);
int encode_32be(struct encoder *encoder,int src);

struct decoder {
  const unsigned char *v;
  int p,c;
};

static inline int decoder_remaining(const struct decoder *decoder) {
  return decoder->c-decoder->p;
}

int decode_32be(int *dst,struct decoder *decoder);

int vlq_decode(int *dst,const void *src,int srcc);

#endif
