// Exercise the complete vi->SDK SystemEvent->kernel handle->WaitSynchronization
// chain. Only the SDK's event object layout is substituted, not kernel events.
#include "../hle.h"
#include "../hle_thread.h"

module_t g_mod[MAX_MOD];
int g_nmod;
void unimpl_stubs(void) {}
void guest_thread_init(void) {}
void dm_thread_enter(void *entry, void *arg, void *sp) { (void)entry; (void)arg; (void)sp; abort(); }
int svc_dispatch(unsigned n, uint64_t *r) { return svc_thread((int)n, r) ? 0 : -1; }
typedef struct { uint32_t write, read; } sdk_event;
static int creates;
static uint32_t create_event(sdk_event *event, int mode, int interprocess) {
    if (mode || !interprocess) abort();
    uint64_t r[8] = {0};
    svc_thread(0x45, r);
    event->write = (uint32_t)r[1]; event->read = (uint32_t)r[2];
    creates++;
    return (uint32_t)r[0];
}
static uint32_t get_write(const sdk_event *event) { return event->write; }
void *guest_sym(const char *name) {
    if (!strcmp(name, "_ZN2nn2os17CreateSystemEventEPNS0_15SystemEventTypeENS0_14EventClearModeEb")) return (void *)create_event;
    if (!strcmp(name, "_ZN2nn2os30GetWritableHandleOfSystemEventEPKNS0_15SystemEventTypeE")) return (void *)get_write;
    return NULL;
}
static int fails;
#define CHK(c, msg) do { int ok = !!(c); printf("%s %s\n", ok ? "PASS" : "FAIL", msg); if (!ok) fails++; } while (0)
static void *lookup(const char *name) {
    uint64_t fn = dm_hle_force_lookup(name);
    for (unsigned i = 0; i < g_hle_force_n; i++)
        if (!strcmp(g_hle_force[i].name, name) && fn == (uint64_t)(uintptr_t)g_hle_force[i].fn) return NULL;
    return (void *)(uintptr_t)fn;
}
int main(void) {
    svc_thread_main_init();
    uint32_t (*open_display)(void **) = lookup("_ZN2nn2vi18OpenDefaultDisplayEPPNS0_7DisplayE");
    uint32_t (*create_layer)(void **, void *) = lookup("_ZN2nn2vi11CreateLayerEPPNS0_5LayerEPNS0_7DisplayE");
    uint32_t (*native)(void **, void *) = lookup("_ZN2nn2vi15GetNativeWindowEPPvPNS0_5LayerE");
    uint32_t (*scaling)(void *, int) = lookup("_ZN2nn2vi19SetLayerScalingModeEPNS0_5LayerENS0_11ScalingModeE");
    uint32_t (*vsync)(void *, void *) = lookup("_ZN2nn2vi20GetDisplayVsyncEventEPNS_2os15SystemEventTypeEPNS0_7DisplayE");
    CHK(open_display && create_layer && native && scaling && vsync, "vi output APIs are implementations, not success stubs");
    if (!open_display || !create_layer || !native || !scaling || !vsync) return 1;
    void *display = NULL, *layer = NULL, *window = NULL;
    CHK(open_display(&display) == 0 && display, "display output initialized");
    CHK(create_layer(&layer, display) == 0 && layer, "layer output initialized");
    CHK(native(&window, layer) == 0 && window, "native window output initialized");
    CHK(scaling(layer, 2) == 0, "game scaling mode accepted");
    void *bad = (void *)1;
    CHK(create_layer(&bad, NULL) != 0 && !bad, "invalid display rejected and output cleared");
    CHK(native(NULL, layer) != 0 && vsync(NULL, display) != 0, "null outputs rejected");
    static sdk_event events[2];
    for (unsigned i = 0; i < 2; i++) {
        CHK(vsync(&events[i], display) == 0 && events[i].read && events[i].write, "vsync creates real readable/writable event handles");
        CHK(vsync(&events[i], display) == 0 && creates == (int)i + 1, "repeat binding reuses event");
    }
    for (unsigned tick = 0; tick < 3; tick++) {
        for (unsigned i = 0; i < 2; i++) {
            uint64_t r[8] = {events[i].read}; svc_thread(0x12, r);
            r[0] = 0; r[1] = (uint64_t)(uintptr_t)&events[i].read; r[2] = 1; r[3] = 1000000000;
            CHK(svc_thread(0x18, r) && !r[0], "guest wait receives a subsequent vsync tick");
        }
    }
    return fails != 0;
}
