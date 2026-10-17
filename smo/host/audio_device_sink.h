#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
/* Independently decoded SDK command ABI. DeviceSink (15) is deliberately
 * skipped by the SDK CPU processor: it belongs to the host device backend. */
static inline uint32_t smo_audio_u32(const void *p) { uint32_t v; memcpy(&v,p,4); return v; }
static inline uint64_t smo_audio_u64(const void *p) { uint64_t v; memcpy(&v,p,8); return v; }
static inline uint16_t smo_audio_u16(const void *p) { uint16_t v; memcpy(&v,p,2); return v; }
static inline int16_t smo_audio_clip(double v) { return v>32767?32767:v< -32768?-32768:(int16_t)v; }
static inline int16_t smo_audio_fixed16_to_pcm16(double v) { double sample=v/65536.0;return smo_audio_clip(sample+(sample>=0?0.5:-0.5)); }
static inline int smo_audio_device_pcm(const uint8_t *header, const uint8_t *cmd,
                                       size_t bytes, int16_t *out, size_t capacity) {
    if(bytes!=0x130 || !cmd[0x10])return -1;
    uint32_t frames=smo_audio_u32(header+0x14), channels=smo_audio_u32(cmd+0x120);
    uint32_t buffers=smo_audio_u32(header+0x10), rate=smo_audio_u32(header+0x18);
    uint64_t address=smo_audio_u64(cmd+0x118);
    if(!frames || frames>capacity || rate!=48000 || !address ||
       (channels!=1 && channels!=2 && channels!=6))return -1;
    /* An upsampled sink owns a separate six-plane, 240-frame buffer.
     * Current renderer is 48 kHz / 240 frames, so both layouts have this stride. */
    if(frames!=240)return -1;
    const int32_t *mix=(const int32_t *)(uintptr_t)address;
    int indices[6];
    for(unsigned c=0;c<channels;c++){
        indices[c]=(int8_t)cmd[0x124+c];
        if(indices[c]<0 || (uint32_t)indices[c]>=buffers)return -1;
    }
    for(unsigned f=0;f<frames;f++){
        double l=mix[indices[0]*frames+f],r=channels==1?l:mix[indices[1]*frames+f];
        if(channels==6){
            double center=mix[indices[2]*frames+f]*0.70710678118;
            l=(l+center+mix[indices[4]*frames+f]*0.70710678118)*0.70710678118;
            r=(r+center+mix[indices[5]*frames+f]*0.70710678118)*0.70710678118;
        }
        out[2*f]=smo_audio_fixed16_to_pcm16(l);out[2*f+1]=smo_audio_fixed16_to_pcm16(r);
    }
    return (int)frames;
}
