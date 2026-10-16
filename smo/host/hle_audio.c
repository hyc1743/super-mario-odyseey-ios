// Audren service boundary. Keep the SDK's client configuration implementation.
// Its shipped server also exports the pure work-buffer calculation, allowing
// the host to answer this query without IPC or a guessed fixed allocation.
#include "hle.h"
#include "hle_thread.h"
#include <pthread.h>
#include <time.h>

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
    for (int i = 0; i < g_nmod; i++) {
        if (strcmp(g_mod[i].name, "sdk")) continue;
        void *fn = module_sym(&g_mod[i], name);
        if (fn) return fn;
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
static void server_event(renderer *r) {
    void (*signal)(void *) = sdk_function("_ZN2nn2os11SignalEventEPNS0_9EventTypeE");
    signal(r->server + 0x1d0);
}
static uint32_t render_frame(renderer *r) {
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
    limit(r->processor, UINT32_MAX);
    setup(r->processor, commands, bytes);
    process(r->processor);
    server_event(r);
    if (!r->frames++) LOG("AUDIO_RENDERER_CPU_FRAME bytes=%zu (host speaker output pending)", bytes);
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

// Only event delivery runs on this host thread. SDK work remains on the
// calling guest audio thread, with its initialized TLS and kernel identity.
static void *audio_tick(void *unused) {
    (void)unused;
    const struct timespec interval = {0, 5000000};
    for (;;) {
        nanosleep(&interval, NULL);
        pthread_mutex_lock(&audio_lock);
        for (unsigned i = 0; i < SMO_ARRAY_LEN(sessions); i++) {
            renderer *r = sessions[i];
            if (r && r->running && !r->manual && r->event) hle_signal_system_event(r->event);
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
    pthread_mutex_lock(&audio_lock);
    renderer *r = find_session(handle.object);
    uint32_t result = AUDIO_INVALID;
    if (r && config) {
        uint32_t (*client)(renderer_handle, const void *) = sdk_function(
            "_ZN2nn5audio26RequestUpdateAudioRendererENS0_19AudioRendererHandleEPKNS0_19AudioRendererConfigE");
        result = client(handle, config);
        if (!result && r->running && !r->manual) result = render_frame(r);
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
        if (!result) r->running = 1;
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
    if (r) stop_renderer(r);
    pthread_mutex_unlock(&audio_lock);
}
static void h_close(renderer_handle handle) {
    pthread_mutex_lock(&audio_lock);
    renderer *r = find_session(handle.object);
    if (r) {
        stop_renderer(r);
        void (*finalize)(void *) = SERVER_FN("8FinalizeEv");
        finalize(r->server);
        destroy_server(r);
        if (r->event) hle_forget_system_event(r->event);
        for (unsigned i = 0; i < SMO_ARRAY_LEN(sessions); i++) if (sessions[i] == r) sessions[i] = NULL;
        free(r);
    }
    pthread_mutex_unlock(&audio_lock);
}
static uint32_t h_execute(renderer_handle handle) {
    pthread_mutex_lock(&audio_lock);
    renderer *r = find_session(handle.object);
    uint32_t result = r && r->running && r->manual ? render_frame(r) : AUDIO_INVALID;
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

static const hle_impl_ent impl[] = {
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
