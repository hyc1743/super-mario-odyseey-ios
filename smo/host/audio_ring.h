#pragma once
#include <stdatomic.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
/* One producer (serialized SDK renderer), one CoreAudio consumer. Bounded
 * storage; callback never allocates, locks, logs or calls guest code. */
#define SMO_AUDIO_RING_FRAMES 4096u
typedef struct {
    _Atomic uint64_t read,write,dropped,underrun;
    int16_t samples[SMO_AUDIO_RING_FRAMES*2];
} smo_audio_ring;
static inline size_t smo_audio_ring_push(smo_audio_ring *r,const int16_t *p,size_t n){
    uint64_t w=atomic_load_explicit(&r->write,memory_order_relaxed);
    uint64_t read=atomic_load_explicit(&r->read,memory_order_acquire);
    size_t space=SMO_AUDIO_RING_FRAMES-(size_t)(w-read),take=n<space?n:space;
    for(size_t i=0;i<take;i++){size_t at=((w+i)%SMO_AUDIO_RING_FRAMES)*2;r->samples[at]=p[2*i];r->samples[at+1]=p[2*i+1];}
    atomic_store_explicit(&r->write,w+take,memory_order_release);
    atomic_fetch_add_explicit(&r->dropped,n-take,memory_order_relaxed);return take;
}
static inline size_t smo_audio_ring_pop(smo_audio_ring *r,int16_t *p,size_t n){
    uint64_t read=atomic_load_explicit(&r->read,memory_order_relaxed);
    uint64_t w=atomic_load_explicit(&r->write,memory_order_acquire);
    size_t available=(size_t)(w-read),take=n<available?n:available;
    for(size_t i=0;i<take;i++){size_t at=((read+i)%SMO_AUDIO_RING_FRAMES)*2;p[2*i]=r->samples[at];p[2*i+1]=r->samples[at+1];}
    memset(p+2*take,0,(n-take)*2*sizeof *p);
    atomic_store_explicit(&r->read,read+take,memory_order_release);
    atomic_fetch_add_explicit(&r->underrun,n-take,memory_order_relaxed);return take;
}
