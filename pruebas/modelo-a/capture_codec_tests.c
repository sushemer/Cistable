#include "captura.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    assert(cap_crc32((const uint8_t *)"123456789",9)==0xcbf43926u); /* independent IEEE CRC check */
    uint8_t out[CAP_FRAME_MAX],p[CAP_PAYLOAD_MAX];memset(p,0x0a,sizeof(p));
    size_t n=cap_encode(out,1,2,0x12345678,0x0102030405060708ULL,p,sizeof(p));
    assert(n==CAP_FRAME_MAX&&memcmp(out,"CSTCAP01",8)==0&&out[10]==0x10&&out[11]==1);
    assert(out[12]==0x78&&out[16]==8&&out[23]==1&&out[24]==0x0a);
    assert(!cap_encode(out,1,0,1,0,p,CAP_PAYLOAD_MAX+1));
    assert(!cap_encode(out,1,0,1,0,NULL,1));
    cap_f64(p,1.0);const uint8_t one[]={0,0,0,0,0,0,0xf0,0x3f};assert(!memcmp(p,one,8));
    FILE *file=fopen("pruebas/modelo-a/build/capture-codec.raw","wb");assert(file);
    n=cap_encode(out,6,0,1,100000,(const uint8_t *)"schema=1 role=fixture rate_target_hz=100",38);
    assert(fwrite(out,1,n,file)==n);fclose(file);
    puts("PASS capture: IEEE CRC reference, explicit endian/binary64, bounds, binary fixture");return 0;
}
