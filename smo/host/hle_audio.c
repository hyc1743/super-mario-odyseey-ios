// Audren service boundary. Keep the SDK's client configuration implementation.
// Its shipped server also exports the pure work-buffer calculation, allowing
// the host to answer this query without IPC or a guessed fixed allocation.
#include "hle.h"
#include "hle_thread.h"
#include <pthread.h>
#include <time.h>
#include <stdatomic.h>
#include "audio_output.h"
#include "audio_device_sink.h"
static _Atomic(smo_audio_output_fn) speaker_output;
static _Atomic int app_suspended;
static _Atomic uint64_t renderer_updates,audio_event_ticks,audio_mix_frames;
void smo_audio_set_output(smo_audio_output_fn output) { atomic_store(&speaker_output, output); }
void smo_audio_set_suspended(int suspended) { atomic_store(&app_suspended,!!suspended); }

static void convert_parameter(uint32_t internal_words[13], const void *parameter) {
    const uint8_t *external = parameter;
    uint8_t *internal = (uint8_t *)internal_words;
    memset(internal, 0, 0x34);
    memcpy(internal, external, 0x20);
    internal[0x20] = external[0x20];
    memcpy(internal + 0x24, external + 0x24, 8);
    internal[0x22] = external[0x2c];
    internal[0x23] = external[0x30];
    memcpy(internal + 0x30, external + 0x34, 4);
}

static void *sdk_function(const char *name) {
    /* Symbols are immutable after host_boot. Resolve once, rather than
     * running the loader's linear cross-check for every 5 ms audio frame. */
    static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
    static struct {const char *name;void *function;} cache[48];
    static unsigned used;
    pthread_mutex_lock(&lock);
    for(unsigned c=0;c<used;c++)if(!strcmp(cache[c].name,name)){
        void *function=cache[c].function;pthread_mutex_unlock(&lock);return function;
    }
    for (int i = 0; i < g_nmod; i++) {
        if (strcmp(g_mod[i].name, "sdk")) continue;
        void *fn = module_sym(&g_mod[i], name);
        if (fn){
            if(used==SMO_ARRAY_LEN(cache))die("Audio SDK symbol cache exhausted");
            cache[used].name=name;cache[used++].function=fn;pthread_mutex_unlock(&lock);return fn;
        }
    }
    die("Audio SDK function unavailable: %s", name);
}

static uint64_t h_renderer_work_size(const void *parameter) {
    if (!parameter) return 0;
    int (*valid)(const void *) = sdk_function(
        "_ZN2nn5audio29IsValidAudioRendererParameterERKNS0_22AudioRendererParameterE");
    if (!valid(parameter)) return 0;
    // SDK+0x17db28..0x17dba4: public parameter -> IPC internal layout.
    // In particular, execution/rendering modes move from words to bytes;
    // the revision moves from +0x34 to +0x30, and reserved bytes remain zero.
    uint32_t internal_words[13];
    convert_parameter(internal_words, parameter);
    uint64_t (*size)(const void *) = sdk_function(
        "_ZN2nn5audio6server17AudioRenderSystem17GetWorkBufferSizeERKNS0_6detail30AudioRendererParameterInternalE");
    uint64_t bytes = size(internal_words);
    if (!bytes || (bytes & 0xfff)) die("Audio SDK returned invalid work-buffer size: %#llx", (unsigned long long)bytes);
    LOG("AUDIO_RENDERER_WORK_SIZE bytes=%llu", (unsigned long long)bytes);
    return bytes;
}

// Layouts verified against the shipped SDK: AudioRenderSystem's last field is
// at +0x328; CommandListProcessor's last field ends at +0x50. No guest code or
// hardware-service implementation is copied here; exported functions own both.
typedef struct renderer renderer;
typedef struct { renderer *object; void *event; } renderer_handle;
typedef struct { void *data; size_t size; } audio_buffer;
struct renderer {
    const void *const *vtable;
    void *event;
    int running, manual;
    uint64_t frames;
    uint8_t *scratch;
    size_t scratch_size;
    int16_t pcm[480];
    uint64_t sink_frames, audible_frames;
    _Alignas(16) uint8_t server[0x330];
    _Alignas(16) uint8_t processor[0x50];
};
static renderer *sessions[8];
static pthread_mutex_t audio_lock = PTHREAD_MUTEX_INITIALIZER;
static int ticker_started;
#define AUDIO_INVALID 0x299u
#define AUDIO_NO_MEMORY 0x899u
#define SERVER_FN(method) sdk_function("_ZN2nn5audio6server17AudioRenderSystem" method)

static renderer *find_session(renderer *object) {
    for (unsigned i = 0; i < SMO_ARRAY_LEN(sessions); i++)
        if (sessions[i] && sessions[i] == object) return object;
    return NULL;
}
static uint64_t audio_now_ns(void){
    struct timespec now;clock_gettime(CLOCK_MONOTONIC,&now);
    return (uint64_t)now.tv_sec*1000000000u+(uint64_t)now.tv_nsec;
}
/* SDK EventType::SignalEvent enters Horizon arbitration and needs a registered
 * guest thread. Auto mixing runs on a host scheduler thread, so that path uses
 * the already-bound client SystemEvent instead. */
static void server_event(renderer *r) {
    void (*signal)(void *) = sdk_function("_ZN2nn2os11SignalEventEPNS0_9EventTypeE");
    signal(r->server + 0x1d0);
}
static uint32_t render_frame(renderer *r, int signal_server_event) {
    void *(*buffer)(const void *) = sdk_function(
        "_ZNK2nn5audio6server17AudioRenderSystem26GetCommandBufferWorkBufferEv");
    size_t (*capacity)(const void *) = sdk_function(
        "_ZNK2nn5audio6server17AudioRenderSystem30GetCommandBufferWorkBufferSizeEv");
    size_t (*generate)(void *, void *, size_t) = SERVER_FN("15GenerateCommandEPvm");
    void (*setup)(void *, const void *, size_t) = sdk_function(
        "_ZN2nn5audio3dsp20CommandListProcessor5SetupEPKvm");
    void (*process)(void *) = sdk_function("_ZN2nn5audio3dsp20CommandListProcessor7ProcessEv");
    void (*limit)(void *, uint32_t) = sdk_function(
        "_ZN2nn5audio3dsp20CommandListProcessor17SetProcessTimeMaxEj");
    void *commands = buffer(r->server);
    size_t available = capacity(r->server);
    if (!commands || !available) return AUDIO_INVALID;
    size_t bytes = generate(r->server, commands, available);
    if (bytes < 0x20 || bytes > available) return AUDIO_INVALID;
    const uint8_t *list=commands;
    uint32_t count=smo_audio_u32(list+4);
    size_t offset=0x20, begin=offset;
    uint32_t segment_count=0;
    /* Validate the entire list before handing any segment to the SDK. */
    for(uint32_t i=0;i<count;i++){
        if(offset>bytes || bytes-offset<0x14 || smo_audio_u32(list+offset)!=0xcafebabeu)return AUDIO_INVALID;
        size_t length=smo_audio_u16(list+offset+4);
        if(length<0x14 || length>bytes-offset)return AUDIO_INVALID;
        offset+=length;
    }
    if(offset!=bytes)return AUDIO_INVALID;
    if(r->scratch_size<bytes){
        void *memory=realloc(r->scratch,bytes);if(!memory)return AUDIO_NO_MEMORY;
        r->scratch=memory;r->scratch_size=bytes;
    }
    offset=0x20;
    for(uint32_t i=0;i<count;i++){
        const uint8_t *cmd=list+offset;size_t length=smo_audio_u16(cmd+4);
        int sink=smo_audio_u16(cmd+6)==15 && cmd[0x10];
        offset+=length;segment_count++;
        if(!sink && i+1<count)continue;
        memcpy(r->scratch,list,0x20);memcpy(r->scratch+4,&segment_count,4);
        memcpy(r->scratch+0x20,list+begin,offset-begin);
        limit(r->processor,UINT32_MAX);
        setup(r->processor,r->scratch,0x20+offset-begin);process(r->processor);
        if(sink){
            int samples=smo_audio_device_pcm(list,cmd,length,r->pcm,240);
            if(samples<0){LOG("AUDIO_DEVICE_SINK_UNSUPPORTED frames=%u rate=%u channels=%u",smo_audio_u32(list+0x14),smo_audio_u32(list+0x18),length>=0x124?smo_audio_u32(cmd+0x120):0);return AUDIO_INVALID;}
            unsigned peak=0;for(int s=0;s<samples*2;s++){unsigned value=r->pcm[s]<0?-(int)r->pcm[s]:r->pcm[s];if(value>peak)peak=value;}
            r->sink_frames++;if(peak)r->audible_frames++;
            if(r->sink_frames==1 || (peak && r->audible_frames==1) || r->sink_frames%200==0)
                LOG("AUDIO_DEVICE_PCM frames=%llu audible=%llu peak=%u rate=48000 channels=2",(unsigned long long)r->sink_frames,(unsigned long long)r->audible_frames,peak);
            smo_audio_output_fn output=atomic_load(&speaker_output);if(output)output(r->pcm,(size_t)samples);
        }
        begin=offset;segment_count=0;
    }
    if(!count){limit(r->processor,UINT32_MAX);setup(r->processor,commands,bytes);process(r->processor);}
    if(signal_server_event)server_event(r);
    if (!r->frames++) LOG("AUDIO_RENDERER_CPU_FRAME bytes=%zu", bytes);
    uint64_t count=atomic_fetch_add(&audio_mix_frames,1)+1;
    if(count==1 || count%100==0)LOG("AUDIO_RENDERER_MIX_FRAMES count=%llu",(unsigned long long)count);
    return 0;
}
// The SDK RequestUpdate serializes the real client configuration and invokes
// IAudioRenderer::RequestUpdateAuto at vtable +0x40. Bridge that one service call
// directly to the SDK server, preserving pool/voice/effect/sink status updates.
static uint32_t update_service(renderer *r, const audio_buffer *out,
                               const audio_buffer *performance, const audio_buffer *in) {
    uint32_t (*update)(void *, void *, size_t, void *, size_t, const void *, size_t) =
        SERVER_FN("6UpdateEPvmS3_mPKvm");
    return update(r->server, out->data, out->size, performance->data,
                  performance->size, in->data, in->size);
}
static const void *const renderer_vtable[16] = { [8] = (void *)update_service };

// Auto-mode AudioRenderSystem is normally advanced by audren's service worker.
// This host replaces that worker, so it must generate and process each 5 ms
// command list here before waking the guest's renderer event. Config updates
// are independent and must not be used as the audio clock.
static void *audio_tick(void *unused) {
    (void)unused;
    const uint64_t period_ns=5000000u;
    uint64_t deadline=audio_now_ns();
    for (;;) {
        deadline+=period_ns;
        uint64_t now=audio_now_ns();
        while(deadline>now){
            uint64_t delay=deadline-now;
            struct timespec wait={(time_t)(delay/1000000000u),(long)(delay%1000000000u)};
            nanosleep(&wait,NULL);now=audio_now_ns();
        }
        /* Never catch up with a burst after a slow render or long suspension. */
        if(now-deadline>=period_ns)deadline=now;
        if(atomic_load(&app_suspended))continue;
        pthread_mutex_lock(&audio_lock);
        for (unsigned i = 0; i < SMO_ARRAY_LEN(sessions); i++) {
            renderer *r = sessions[i];
            if (r && r->running && !r->manual) {
                uint32_t result=render_frame(r,0);
                if(result){
                    LOG("AUDIO_RENDERER_TICK_FAILED result=%#x frames=%llu",result,(unsigned long long)r->frames);
                    continue;
                }
                uint64_t count=atomic_fetch_add(&audio_event_ticks,1)+1;
                if(count==1 || count%100==0)LOG("AUDIO_RENDERER_EVENT_TICKS count=%llu",(unsigned long long)count);
                if(r->event)hle_signal_system_event(r->event);
            }
        }
        pthread_mutex_unlock(&audio_lock);
    }
    return NULL;
}
static void destroy_server(renderer *r) {
    void (*system_event)(void *) = sdk_function("_ZN2nn2os18DestroySystemEventEPNS0_15SystemEventTypeE");
    void (*event)(void *) = sdk_function("_ZN2nn2os13FinalizeEventEPNS0_9EventTypeE");
    void (*mutex)(void *) = sdk_function("_ZN2nn2os13FinalizeMutexEPNS0_9MutexTypeE");
    system_event(r->server + 0x1a0);
    event(r->server + 0x1d0);
    mutex(r->server + 0x1f8);
}
static uint32_t h_open(renderer_handle *out, void *event, const void *parameter,
                       void *work, size_t size) {
    if (!out) return AUDIO_INVALID;
    memset(out, 0, sizeof *out);
    uint64_t required = h_renderer_work_size(parameter);
    if (!required || !work || ((uintptr_t)work & 0xfff) || size < required) return AUDIO_INVALID;
    pthread_mutex_lock(&audio_lock);
    unsigned slot = 0;
    while (slot < SMO_ARRAY_LEN(sessions) && sessions[slot]) slot++;
    if (slot == SMO_ARRAY_LEN(sessions)) { pthread_mutex_unlock(&audio_lock); return AUDIO_NO_MEMORY; }
    renderer *r = calloc(1, sizeof *r);
    if (!r) { pthread_mutex_unlock(&audio_lock); return AUDIO_NO_MEMORY; }
    r->vtable = renderer_vtable;
    uint32_t internal[13];
    convert_parameter(internal, parameter);
    r->manual = ((uint8_t *)internal)[0x22] != 0;
    void (*construct)(void *) = SERVER_FN("C1Ev");
    uint32_t (*initialize)(void *, const void *, uint32_t, void *, size_t, const uint64_t *, int) =
        SERVER_FN("10InitializeERKNS0_6detail30AudioRendererParameterInternalEjPvmRKNS_6applet20AppletResourceUserIdEi");
    construct(r->server);
    uint64_t applet = 0;
    uint32_t result = initialize(r->server, internal, 0xffff8001u, work, size, &applet, (int)slot);
    if (result) {
        destroy_server(r); free(r); pthread_mutex_unlock(&audio_lock);
        LOG("AUDIO_RENDERER_OPEN_FAILED result=%#x", result);
        return result;
    }
    if (!ticker_started) {
        pthread_t ticker;
        if (pthread_create(&ticker, NULL, audio_tick, NULL)) die("Audio event ticker creation failed");
        pthread_detach(ticker);
        ticker_started = 1;
    }
    if (event) hle_bind_system_event(event);
    r->event = event;
    sessions[slot] = r;
    out->object = r; out->event = event;
    int manual = r->manual;
    pthread_mutex_unlock(&audio_lock);
    LOG("AUDIO_RENDERER_OPEN slot=%u bytes=%zu manual=%d", slot, size, manual);
    return 0;
}
static uint32_t h_update(renderer_handle handle, const void *config) {
    uint64_t call=atomic_fetch_add(&renderer_updates,1)+1;
    if(call==1 || call%100==0)LOG("AUDIO_RENDERER_UPDATE_CALLS count=%llu",(unsigned long long)call);
    pthread_mutex_lock(&audio_lock);
    renderer *r = find_session(handle.object);
    uint32_t result = AUDIO_INVALID;
    if (r && config) {
        uint32_t (*client)(renderer_handle, const void *) = sdk_function(
            "_ZN2nn5audio26RequestUpdateAudioRendererENS0_19AudioRendererHandleEPKNS0_19AudioRendererConfigE");
        result = client(handle, config);
        /* RequestUpdateAuto applies client state; the service's 5 ms audio
         * worker, not this comparatively rare configuration call, mixes PCM. */
    }
    pthread_mutex_unlock(&audio_lock);
    return result;
}
static uint32_t h_start(renderer_handle handle) {
    pthread_mutex_lock(&audio_lock);
    renderer *r = find_session(handle.object);
    uint32_t result = AUDIO_INVALID;
    if (r) {
        uint32_t (*start)(void *) = SERVER_FN("5StartEv");
        result = start(r->server);
        if (!result){r->running=1;LOG("AUDIO_RENDERER_START event=%p manual=%d",r->event,r->manual);}
    }
    pthread_mutex_unlock(&audio_lock);
    return result;
}
static void stop_renderer(renderer *r) {
    server_event(r);
    void (*stop)(void *) = SERVER_FN("4StopEv");
    stop(r->server);
    r->running = 0;
}
static void h_stop(renderer_handle handle) {
    pthread_mutex_lock(&audio_lock);
    renderer *r = find_session(handle.object);
    if (r){LOG("AUDIO_RENDERER_STOP frames=%llu",(unsigned long long)r->frames);stop_renderer(r);}
    pthread_mutex_unlock(&audio_lock);
}
static void h_close(renderer_handle handle) {
    pthread_mutex_lock(&audio_lock);
    renderer *r = find_session(handle.object);
    if (r) {
        LOG("AUDIO_RENDERER_CLOSE frames=%llu",(unsigned long long)r->frames);
        stop_renderer(r);
        void (*finalize)(void *) = SERVER_FN("8FinalizeEv");
        finalize(r->server);
        destroy_server(r);
        if (r->event) hle_forget_system_event(r->event);
        for (unsigned i = 0; i < SMO_ARRAY_LEN(sessions); i++) if (sessions[i] == r) sessions[i] = NULL;
        free(r->scratch);free(r);
    }
    pthread_mutex_unlock(&audio_lock);
}
static uint32_t h_execute(renderer_handle handle) {
    pthread_mutex_lock(&audio_lock);
    renderer *r = find_session(handle.object);
    uint32_t result = r && r->running && r->manual ? render_frame(r,1) : AUDIO_INVALID;
    if (!result && r->event) hle_signal_system_event(r->event);
    pthread_mutex_unlock(&audio_lock);
    return result;
}
static uint32_t h_set_limit(renderer_handle handle, int percent) {
    pthread_mutex_lock(&audio_lock);
    renderer *r = find_session(handle.object);
    if (r && percent >= 0 && percent <= 100) {
        void (*set)(void *, int) = SERVER_FN("21SetRenderingTimeLimitEi");
        set(r->server, percent);
    } else r = NULL;
    pthread_mutex_unlock(&audio_lock);
    return r ? 0 : AUDIO_INVALID;
}
static int h_get_limit(renderer_handle handle) {
    pthread_mutex_lock(&audio_lock);
    renderer *r = find_session(handle.object);
    int value = 0;
    if (r) {
        int (*get)(const void *) = sdk_function("_ZNK2nn5audio6server17AudioRenderSystem21GetRenderingTimeLimitEv");
        value = get(r->server);
    }
    pthread_mutex_unlock(&audio_lock);
    return value;
}

// SDK+0x184668/0x184918 use 0x100 bytes per AudioDeviceName.
// Expose the CPU renderer's single stereo endpoint. Speaker delivery is still
// pending; these describe the renderer route, not physical device discovery.
typedef struct { char name[0x100]; } audio_device_name;
static const audio_device_name output_device = { "AudioTvOutput" };
static int h_list_devices(audio_device_name *out, int capacity) {
    if (!out || capacity <= 0) return 0;
    *out = output_device;
    return 1;
}
static void h_active_device(audio_device_name *out) {
    if (out) *out = output_device;
}
static int h_active_channels(void) { return 2; }
static void h_device_notification(void *event) {
    if (!event) die("Audio device notification requires a SystemEvent");
    // The caller polls this event every frame, including with a zero timeout.
    // It must be an initialized SDK event even when this fixed route never
    // switches. Do not periodically signal a device-change event.
    hle_bind_system_event(event);
    LOG("AUDIO_DEVICE_NOTIFICATION_BOUND");
}

static const hle_impl_ent impl[] = {
    {"_ZN2nn5audio19ListAudioDeviceNameEPNS0_15AudioDeviceNameEi", (void *)h_list_devices, "CPU renderer stereo endpoint"},
    {"_ZN2nn5audio24GetActiveAudioDeviceNameEPNS0_15AudioDeviceNameE", (void *)h_active_device, "CPU renderer active endpoint"},
    {"_ZN2nn5audio21GetActiveChannelCountEv", (void *)h_active_channels, "Stereo renderer route"},
    {"_ZN2nn5audio36AcquireAudioDeviceSwitchNotificationEPNS_2os11SystemEventE", (void *)h_device_notification, "Initialize SDK notification event for fixed renderer route"},
    {"_ZN2nn5audio30GetAudioRendererWorkBufferSizeERKNS0_22AudioRendererParameterE",
     (void *)h_renderer_work_size, "SDK server's pure size calculation; no service IPC"},
    {"_ZN2nn5audio17OpenAudioRendererEPNS0_19AudioRendererHandleEPNS_2os11SystemEventERKNS0_22AudioRendererParameterEPvm", (void *)h_open, "In-process SDK audio server"},
    {"_ZN2nn5audio26RequestUpdateAudioRendererENS0_19AudioRendererHandleEPKNS0_19AudioRendererConfigE", (void *)h_update, "SDK serialization, server status updates and CPU command processing"},
    {"_ZN2nn5audio18StartAudioRendererENS0_19AudioRendererHandleE", (void *)h_start, "Start renderer and paced update notifications"},
    {"_ZN2nn5audio17StopAudioRendererENS0_19AudioRendererHandleE", (void *)h_stop, "Stop renderer"},
    {"_ZN2nn5audio18CloseAudioRendererENS0_19AudioRendererHandleE", (void *)h_close, "Finalize renderer resources"},
    {"_ZN2nn5audio29ExecuteAudioRendererRenderingENS0_19AudioRendererHandleE", (void *)h_execute, "Manual CPU rendering"},
    {"_ZN2nn5audio34SetAudioRendererRenderingTimeLimitENS0_19AudioRendererHandleEi", (void *)h_set_limit, "SDK renderer budget"},
    {"_ZN2nn5audio34GetAudioRendererRenderingTimeLimitENS0_19AudioRendererHandleE", (void *)h_get_limit, "SDK renderer budget"},
};
static void __attribute__((constructor)) audio_reg(void) {
    hle_impl_register(impl, SMO_ARRAY_LEN(impl));
}
