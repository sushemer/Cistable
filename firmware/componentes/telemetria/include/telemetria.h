#ifndef CISTABLE_TELEMETRIA_H
#define CISTABLE_TELEMETRIA_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define CT_WIRE_SIZE 124
#define CT_ACCEL 1
#define CT_GYRO 2
#define CT_ORIENTATION 4
#define CT_WATER 8
/* Wire water unit enum; RAW is always raw counts, never percent. */
enum { CT_RAW_REGISTER, CT_DEPTH_M, CT_GAUGE_KPA };
typedef struct {
    uint32_t session, sequence;
    uint64_t sent_us, accel_us, gyro_us, orientation_us, water_us;
    uint32_t accel_sequence, gyro_sequence, orientation_sequence, water_sequence;
    float acceleration[3], gyro[3], quaternion[4], water;
    uint16_t water_register;
    uint8_t water_quantity, flags, accel_quality, gyro_quality, orientation_quality;
} ct_frame;
bool ct_encode(const ct_frame *frame,uint8_t out[CT_WIRE_SIZE]);
bool ct_decode(const uint8_t *data,size_t size,ct_frame *out);
typedef struct { bool initialized; uint32_t session, sequence, retired[4]; unsigned retired_count;
    uint64_t sent_us; } ct_stream;
typedef enum { CT_ACCEPT, CT_NEW_SESSION, CT_DUPLICATE, CT_REVERSED, CT_BAD } ct_acceptance;
ct_acceptance ct_accept(ct_stream *state,const ct_frame *frame);
/* Same-source age + cabin reception residence, no subtraction of two clocks.
 * Does NOT include radio latency: caller must add a measured latency bound. */
bool ct_age_us(uint64_t sent,uint64_t acquired,uint64_t rx_local,uint64_t now_local,uint64_t *age);
#define CT_ACK_SIZE 20
void ct_ack_encode(uint32_t session,uint32_t sequence,uint64_t original_sent_us,uint8_t out[CT_ACK_SIZE]);
bool ct_ack_decode(const uint8_t *data,size_t size,uint32_t *session,uint32_t *sequence,uint64_t *original_sent_us);
uint16_t ct_crc16(const uint8_t *data,size_t size);
typedef enum { CT_MB_PARTIAL, CT_MB_OK, CT_MB_EXCEPTION, CT_MB_BAD } ct_modbus_result;
ct_modbus_result ct_modbus_decode(const uint8_t *data,size_t size,uint8_t address,uint16_t *raw,uint8_t *exception);
#ifdef __cplusplus
}
#endif
#endif
