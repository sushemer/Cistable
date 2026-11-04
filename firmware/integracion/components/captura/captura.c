#include "captura.h"
#include <string.h>
#include <float.h>
_Static_assert(sizeof(double)==8&&DBL_MANT_DIG==53,"IEEE754 binary64 required");
void cap_u64(uint8_t *p,uint64_t v,unsigned n) {for(unsigned k=0;k<n;k++) p[k]=(uint8_t)(v>>(8*k));}
void cap_f64(uint8_t *p,double v) {uint64_t bits;memcpy(&bits,&v,8);cap_u64(p,bits,8);}
uint32_t cap_crc32(const uint8_t *p,size_t n) {
    uint32_t c=0xffffffffu;
    for(size_t k=0;k<n;k++) {c^=p[k];for(unsigned b=0;b<8;b++) c=(c>>1)^((c&1)?0xedb88320u:0);}
    return c^0xffffffffu;
}
size_t cap_encode(uint8_t *out,uint8_t kind,uint8_t flags,uint32_t id,uint64_t time,const uint8_t *p,size_t n) {
    if(!out||n>CAP_PAYLOAD_MAX||(!p&&n)) return 0;
    memcpy(out,"CSTCAP01",8);out[8]=kind;out[9]=flags;
    cap_u64(out+10,n,2);cap_u64(out+12,id,4);cap_u64(out+16,time,8);
    if(n) memcpy(out+24,p,n);
    cap_u64(out+24+n,cap_crc32(out,24+n),4);return 28+n;
}
