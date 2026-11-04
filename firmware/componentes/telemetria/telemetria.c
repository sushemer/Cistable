#include "telemetria.h"
#include <string.h>
#include <math.h>
#include <float.h>
_Static_assert(sizeof(float)==4 && FLT_RADIX==2 && FLT_MANT_DIG==24,"IEEE754 binary32 required");
static void put(uint8_t *p,uint64_t v,unsigned n) { for(unsigned k=0;k<n;k++) p[k]=(uint8_t)(v>>(8*k)); }
static uint64_t get(const uint8_t *p,unsigned n) { uint64_t v=0;for(unsigned k=0;k<n;k++) v|=(uint64_t)p[k]<<(8*k);return v; }
static void pf(uint8_t *p,float f) { uint32_t v;memcpy(&v,&f,4);put(p,v,4); }
static float gf(const uint8_t *p) { uint32_t v=(uint32_t)get(p,4);float f;memcpy(&f,&v,4);return f; }
static bool valid(const ct_frame *f) {
    if(!f||!f->session||f->flags&0xf0||f->water_quantity>CT_GAUGE_KPA) return false;
    const uint64_t times[]={f->accel_us,f->gyro_us,f->orientation_us,f->water_us};
    for(unsigned k=0;k<4;k++) if((f->flags&(1u<<k)) && (!times[k]||times[k]>f->sent_us)) return false;
    for(unsigned k=0;k<3;k++) {
        if((f->flags&CT_ACCEL)&&!isfinite(f->acceleration[k])) return false;
        if((f->flags&CT_GYRO)&&!isfinite(f->gyro[k])) return false;
    }
    if(f->flags&CT_ORIENTATION) {
        double n=0;for(unsigned k=0;k<4;k++) n+=(double)f->quaternion[k]*f->quaternion[k];
        if(!isfinite(n)||fabs(n-1)>1e-4) return false;
    }
    if((f->flags&CT_WATER)&&(!isfinite(f->water)||f->water<0)) return false;
    return true;
}
bool ct_encode(const ct_frame *f,uint8_t p[CT_WIRE_SIZE]) {
    if(!p||!valid(f)) return false;
    memset(p,0,CT_WIRE_SIZE);memcpy(p,"CST2",4);p[4]=2;put(p+6,CT_WIRE_SIZE,2);
    put(p+8,f->session,4);put(p+12,f->sequence,4);
    const uint64_t t[]={f->sent_us,f->accel_us,f->gyro_us,f->orientation_us,f->water_us};
    for(unsigned k=0;k<5;k++) put(p+16+8*k,t[k],8);
    put(p+56,f->accel_sequence,4);put(p+60,f->gyro_sequence,4);put(p+64,f->orientation_sequence,4);
    for(unsigned k=0;k<3;k++) {pf(p+68+4*k,f->acceleration[k]);pf(p+80+4*k,f->gyro[k]);}
    for(unsigned k=0;k<4;k++) pf(p+92+4*k,f->quaternion[k]);
    pf(p+108,f->water);put(p+112,f->water_register,2);p[114]=f->water_quantity;p[115]=f->flags;
    p[116]=f->accel_quality;p[117]=f->gyro_quality;p[118]=f->orientation_quality;
    put(p+120,f->water_sequence,4);return true;
}
bool ct_decode(const uint8_t *p,size_t size,ct_frame *f) {
    if(!p||!f||size!=CT_WIRE_SIZE||memcmp(p,"CST2",4)||p[4]!=2||p[5]||get(p+6,2)!=CT_WIRE_SIZE||p[119]) return false;
    ct_frame v={0};v.session=(uint32_t)get(p+8,4);v.sequence=(uint32_t)get(p+12,4);
    v.sent_us=get(p+16,8);v.accel_us=get(p+24,8);v.gyro_us=get(p+32,8);v.orientation_us=get(p+40,8);v.water_us=get(p+48,8);
    v.accel_sequence=(uint32_t)get(p+56,4);v.gyro_sequence=(uint32_t)get(p+60,4);v.orientation_sequence=(uint32_t)get(p+64,4);
    for(unsigned k=0;k<3;k++) {v.acceleration[k]=gf(p+68+4*k);v.gyro[k]=gf(p+80+4*k);}
    for(unsigned k=0;k<4;k++) v.quaternion[k]=gf(p+92+4*k);
    v.water=gf(p+108);v.water_register=(uint16_t)get(p+112,2);v.water_quantity=p[114];v.flags=p[115];
    v.accel_quality=p[116];v.gyro_quality=p[117];v.orientation_quality=p[118];v.water_sequence=(uint32_t)get(p+120,4);
    if(!valid(&v)) return false;
    *f=v;return true;
}
ct_acceptance ct_accept(ct_stream *s,const ct_frame *f) {
    if(!s||!valid(f)) return CT_BAD;
    bool reset=!s->initialized||s->session!=f->session;
    if(reset) {
        for(unsigned k=0;k<s->retired_count;k++) if(s->retired[k]==f->session) return CT_REVERSED;
        if(s->initialized) {
            if(s->retired_count==4) return CT_BAD; /* require explicit rearm instead of forgetting sessions */
            s->retired[s->retired_count++]=s->session;
        }
    } else {
        uint32_t delta=f->sequence-s->sequence;
        if(!delta) return CT_DUPLICATE;
        if(delta>=0x80000000u||f->sent_us<=s->sent_us) return CT_REVERSED;
    }
    s->initialized=true;s->session=f->session;s->sequence=f->sequence;s->sent_us=f->sent_us;
    return reset?CT_NEW_SESSION:CT_ACCEPT;
}
bool ct_age_us(uint64_t sent,uint64_t acquired,uint64_t rx,uint64_t now,uint64_t *age) {
    if(!age||!acquired||acquired>sent||now<rx||UINT64_MAX-(sent-acquired)<now-rx) return false;
    *age=sent-acquired+now-rx;return true;
}
uint16_t ct_crc16(const uint8_t *p,size_t n) {
    uint16_t c=0xffff;
    for(size_t k=0;k<n;k++) {c^=p[k];for(unsigned b=0;b<8;b++) c=(c&1)?(c>>1)^0xa001:c>>1;}
    return c;
}
void ct_ack_encode(uint32_t session,uint32_t sequence,uint64_t sent,uint8_t p[CT_ACK_SIZE]) {
    memcpy(p,"ACK2",4);put(p+4,session,4);put(p+8,sequence,4);put(p+12,sent,8);
}
bool ct_ack_decode(const uint8_t *p,size_t size,uint32_t *session,uint32_t *sequence,uint64_t *sent) {
    if(!p||!session||!sequence||!sent||size!=CT_ACK_SIZE||memcmp(p,"ACK2",4)) return false;
    *session=(uint32_t)get(p+4,4);*sequence=(uint32_t)get(p+8,4);*sent=get(p+12,8);return true;
}
ct_modbus_result ct_modbus_decode(const uint8_t *p,size_t n,uint8_t address,uint16_t *raw,uint8_t *exception) {
    if(!p||!raw||!exception) return CT_MB_BAD;
    if(n<2) return CT_MB_PARTIAL;
    if(p[0]!=address||(p[1]!=3&&p[1]!=0x83)) return CT_MB_BAD;
    size_t expected=p[1]==0x83?5:7;
    if(n<expected) return CT_MB_PARTIAL;
    if(n!=expected||ct_crc16(p,n-2)!=(uint16_t)(p[n-2]|p[n-1]<<8)) return CT_MB_BAD;
    if(p[1]==0x83) {*exception=p[2];return CT_MB_EXCEPTION;}
    if(p[2]!=2) return CT_MB_BAD;
    *raw=(uint16_t)(p[3]<<8|p[4]);*exception=0;return CT_MB_OK;
}
