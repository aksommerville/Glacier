#include "encoder.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <limits.h>
#include <string.h>

void encoder_cleanup(struct encoder *encoder) {
  if (encoder->v) free(encoder->v);
}

int encoder_require(struct encoder *encoder,int addc) {
  if (addc<1) return 0;
  if (encoder->c<=encoder->a-addc) return 0;
  int na=encoder->c+addc;
  if (na<INT_MAX-1024) na=(na+1024)&~1023;
  void *nv=realloc(encoder->v,na);
  if (!nv) return -1;
  encoder->v=nv;
  encoder->a=na;
  return 0;
}

int encoder_append(struct encoder *encoder,const void *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (((char*)src)[srcc]) srcc++; }
  if (encoder_require(encoder,srcc)<0) return -1;
  memcpy(encoder->v+encoder->c,src,srcc);
  encoder->c+=srcc;
  return 0;
}

int encoder_appendf(struct encoder *encoder,const char *fmt,...) {
  while (1) {
    va_list vargs;
    va_start(vargs,fmt);
    int err=vsnprintf(encoder->v+encoder->c,encoder->a-encoder->c,fmt,vargs);
    if ((err<0)||(err>=INT_MAX)) return -1;
    if (encoder->c<encoder->a-err) { // sic < not <=
      encoder->c+=err;
      return 0;
    }
    if (encoder_require(encoder,err+1)<0) return -1;
  }
}

/* Binary integers.
 */
 
int encode_u8(struct encoder *encoder,int src) {
  uint8_t tmp[]={src};
  return encoder_append(encoder,tmp,sizeof(tmp));
}

int encode_16be(struct encoder *encoder,int src) {
  uint8_t tmp[]={src>>8,src};
  return encoder_append(encoder,tmp,sizeof(tmp));
}

int encode_32be(struct encoder *encoder,int src) {
  uint8_t tmp[]={src>>24,src>>16,src>>8,src};
  return encoder_append(encoder,tmp,sizeof(tmp));
}

int decode_32be(int *dst,struct decoder *decoder) {
  if (decoder_remaining(decoder)<4) return -1;
  const unsigned char *src=decoder->v+decoder->p;
  *dst=(src[0]<<24)|(src[1]<<16)|(src[2]<<8)|src[3];
  decoder->p+=4;
  return 0;
}

/* VLQ
 */
 
int vlq_decode(int *dst,const void *src,int srcc) {
  const uint8_t *SRC=src;
  if (srcc<1) return -1;
  if (!(SRC[0]&0x80)) {
    *dst=SRC[0];
    return 1;
  }
  if (srcc<2) return -1;
  if (!(SRC[1]&0x80)) {
    *dst=((SRC[0]&0x7f)<<7)|SRC[1];
    return 2;
  }
  if (srcc<3) return -1;
  if (!(SRC[2]&0x80)) {
    *dst=((SRC[0]&0x7f)<<14)|((SRC[1]&0x7f)<<7)|SRC[2];
    return 3;
  }
  if (srcc<4) return -1;
  if (!(SRC[3]&0x80)) {
    *dst=((SRC[0]&0x7f)<<21)|((SRC[1]&0x7f)<<14)|((SRC[2]&0x7f)<<7)|SRC[3];
    return 4;
  }
  return -1;
}
