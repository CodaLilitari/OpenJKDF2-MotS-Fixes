#include "edStorage.h"
#ifndef ED_NET_H
#define ED_NET_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
/* Bounded, lossless zero-run codec. No pointers or renderer state go on wire.
 * The protocol rejects differing editor model layouts before decoding a model. */
#define ED_NET_LIMIT (512u*1024u*1024u)
static uint32_t edNetHash(const void *data,size_t length){const unsigned char *p=(const unsigned char*)data;uint32_t h=2166136261u;while(length--)h=(h^*p++)*16777619u;return h;}
static unsigned char *edNetPack(const void *data,uint32_t length,uint32_t *size){if(!length||length>ED_NET_LIMIT/2)return NULL;const unsigned char *p=(const unsigned char*)data;unsigned char *out=edAlloc(length*2u+16),*q=out;if(!out)return NULL;
    for(uint32_t i=0;i<length;){uint32_t start=i,n=0;if(!p[i]){while(i<length&&!p[i]&&n<128){i++;n++;}*q++=128+(n-1);}else{while(i<length&&p[i]&&n<128){i++;n++;}*q++=n-1;edCopy(q,p+start,n);q+=n;}}
    *size=(uint32_t)(q-out);return out;
}
static unsigned char *edNetUnpack(const void *data,uint32_t size,uint32_t length){if(!size||!length||size>ED_NET_LIMIT||length>ED_NET_LIMIT/2)return NULL;const unsigned char *p=(const unsigned char*)data,*end=p+size;unsigned char *out=edCalloc(1,length);if(!out)return NULL;uint32_t at=0;
    while(p<end){unsigned int tag=*p++,n=(tag&127)+1;if(n>length-at){edFree(out);return NULL;}if(!(tag&128)){if((size_t)(end-p)<n){edFree(out);return NULL;}edCopy(out+at,p,n);p+=n;}at+=n;}
    if(at!=length){edFree(out);return NULL;}return out;
}
static uint32_t edNetLoad32(const void *value){const unsigned char *p=(const unsigned char*)value;return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void edNetStore32(void *value,uint32_t number){unsigned char *p=(unsigned char*)value;for(int i=0;i<4;i++)p[i]=(unsigned char)(number>>(i*8));}
static unsigned char *edNetDelta(const unsigned char *base,uint32_t baseSize,const unsigned char *next,uint32_t size,uint32_t *packedSize){if(!size||size>ED_NET_LIMIT/2)return NULL;unsigned char *delta=edAlloc(size);if(!delta)return NULL;for(uint32_t i=0;i<size;i++)delta[i]=next[i]^(i<baseSize?base[i]:0);unsigned char *packed=edNetPack(delta,size,packedSize);edFree(delta);return packed;}
#endif
