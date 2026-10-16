// nn::vi display/layer policy adapted from d2r-mobile dm/gfx/hle_vi.c.
// MIT, Copyright (c) 2026 jhaizhou; third_party/d2r-mobile.LICENSE.
// Handles are opaque host objects. Vsync uses an SDK-created SystemEvent and
// host ticks until presentation is connected to the platform display link.
#include "hle.h"
#include "hle_thread.h"
#include <pthread.h>
#include <time.h>

static struct { uint32_t magic; } g_display = {0x50534944};
static struct { uint32_t magic; int scaling; } g_layer = {0x5259414c, 0};
static struct { uint32_t magic; void *layer; } g_native = {0x4e49574e, &g_layer};
static void *g_events[8];
static unsigned g_nevents;
static int g_ticker_started;
static pthread_mutex_t g_vi_lock = PTHREAD_MUTEX_INITIALIZER;

static void h_vi_init(void) { LOG("vi：宿主显示服务已接管"); }
static uint32_t h_open(void **out) {
    if (!out) return 0xD401;
    *out = &g_display;
    return 0;
}
static uint32_t h_layer(void **out, void *display) {
    if (!out) return 0xD401;
    *out = NULL;
    if (display != &g_display) return 0xD401;
    *out = &g_layer;
    return 0;
}
static uint32_t h_native(void **out, void *layer) {
    if (!out) return 0xD401;
    *out = NULL;
    if (layer != &g_layer) return 0xD401;
    *out = &g_native;
    return 0;
}
static uint32_t h_scaling(void *layer, int mode) {
    if (layer != &g_layer || mode < 0 || mode > 2) return 0xD401;
    pthread_mutex_lock(&g_vi_lock);
    g_layer.scaling = mode;
    pthread_mutex_unlock(&g_vi_lock);
    return 0;
}
static void *vsync_tick(void *arg) {
    (void)arg;
    const struct timespec interval = {0, 16666667};
    for (;;) {
        nanosleep(&interval, NULL);
        pthread_mutex_lock(&g_vi_lock);
        for (unsigned i = 0; i < g_nevents; i++) hle_signal_system_event(g_events[i]);
        pthread_mutex_unlock(&g_vi_lock);
    }
    return NULL;
}
static uint32_t h_vsync(void *event, void *display) {
    if (!event || display != &g_display) return 0xD401;
    pthread_mutex_lock(&g_vi_lock);
    for (unsigned i = 0; i < g_nevents; i++) {
        if (g_events[i] == event) { pthread_mutex_unlock(&g_vi_lock); return 0; }
    }
    if (g_nevents == SMO_ARRAY_LEN(g_events)) { pthread_mutex_unlock(&g_vi_lock); return 0xCE01; }
    if (!g_ticker_started) {
        pthread_t thread;
        if (pthread_create(&thread, NULL, vsync_tick, NULL)) {
            pthread_mutex_unlock(&g_vi_lock);
            return 0xCE01;
        }
        pthread_detach(thread);
        g_ticker_started = 1;
    }
    hle_bind_system_event(event); // On the guest thread, before publishing to the ticker.
    g_events[g_nevents++] = event;
    pthread_mutex_unlock(&g_vi_lock);
    LOG("vi：Vsync 系统事件已绑定（宿主 60 Hz）");
    return 0;
}

static const hle_impl_ent g_vi_impl[] = {
    {"_ZN2nn2vi10InitializeEv", (void *)h_vi_init, "宿主显示服务"},
    {"_ZN2nn2vi18OpenDefaultDisplayEPPNS0_7DisplayE", (void *)h_open, "写出有效显示句柄"},
    {"_ZN2nn2vi11CreateLayerEPPNS0_5LayerEPNS0_7DisplayE", (void *)h_layer, "写出有效图层句柄"},
    {"_ZN2nn2vi15GetNativeWindowEPPvPNS0_5LayerE", (void *)h_native, "NVN 窗口的宿主标识"},
    {"_ZN2nn2vi19SetLayerScalingModeEPNS0_5LayerENS0_11ScalingModeE", (void *)h_scaling, "保存图层缩放方式"},
    {"_ZN2nn2vi20GetDisplayVsyncEventEPNS_2os15SystemEventTypeEPNS0_7DisplayE", (void *)h_vsync, "SDK 系统事件与宿主节拍"},
};
static void __attribute__((constructor)) vi_reg(void) { hle_impl_register(g_vi_impl, SMO_ARRAY_LEN(g_vi_impl)); }
