#ifndef CI_CAPTURA_H
#define CI_CAPTURA_H
#include <stdint.h>
#include <stddef.h>
#define CAP_PAYLOAD_MAX 272
#define CAP_FRAME_MAX (28+CAP_PAYLOAD_MAX)
enum {CAP_RX=1,CAP_PROCESS=2,CAP_TX=3,CAP_TX_DONE=4,CAP_STATS=5,CAP_BOOT=6,CAP_COMMAND=7,CAP_CONFIG=8};
/* Explicit little endian, no packed structs: magic[8], kind/u8, flags/u8,
 * length/u16, capture_id/u32, local_us/u64, payload[length], crc32/u32.
 * CRC covers header+payload; acquisition times remain in CST2 and RX header. */
#ifdef __cplusplus
extern "C" {
#endif
uint32_t cap_crc32(const uint8_t *data,size_t n);
size_t cap_encode(uint8_t *out,uint8_t kind,uint8_t flags,uint32_t id,uint64_t local_us,const uint8_t *payload,size_t n);
void cap_u64(uint8_t *out,uint64_t value,unsigned bytes);
void cap_f64(uint8_t *out,double value);
#ifdef __cplusplus
}
#endif
#endif
