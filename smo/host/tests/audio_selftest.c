#include "../hle.h"

module_t g_mod[MAX_MOD];
int g_nmod = 2;
void unimpl_stubs(void) {}
int svc_dispatch(unsigned n, uint64_t *r) { (void)n; (void)r; return -1; }
static int fails, valid_calls, size_calls, accept;
static const void *expected_parameter;
static uint8_t expected_internal[0x34];
static int constructed, initialized, finalized, destroyed, started, stopped;
static int client_calls, server_calls, processed, bound, forgotten;
static void *last_bound;
static uint32_t server_result, init_result;
static int budget;
static uint8_t commands[64];
static const void *expected_config;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)
static int valid(const void *p) {
    valid_calls++;
    CHECK(p == expected_parameter);
    return accept;
}
static uint64_t size(const void *p) {
    size_calls++;
    CHECK(!memcmp(p, expected_internal, sizeof expected_internal));
    return 0x8000;
}
void hle_bind_system_event(void *event) { CHECK(event); last_bound = event; bound++; }
void hle_signal_system_event(void *event) { (void)event; }
void hle_forget_system_event(void *event) { CHECK(event); forgotten++; }
static void construct(void *p) { CHECK(p); constructed++; }
static uint32_t initialize(void *p, const void *parameters, uint32_t process,
                           void *work, size_t bytes, const uint64_t *applet, int session) {
    CHECK(p && work && bytes >= 0x8000 && applet && !*applet && session == 0);
    CHECK(process == 0xffff8001u);
    CHECK(!memcmp(parameters, expected_internal, sizeof expected_internal));
    initialized++;
    return init_result;
}
static void finalize(void *p) { CHECK(p); finalized++; }
static void destroy(void *p) { CHECK(p); destroyed++; }
static void signal_event(void *p) { CHECK(p); }
static uint32_t start(void *p) { CHECK(p); started++; return 0; }
static void stop(void *p) { CHECK(p); stopped++; }
static void set_limit(void *p, int value) { CHECK(p); budget = value; }
static int get_limit(const void *p) { CHECK(p); return budget; }
static void *get_buffer(const void *p) { CHECK(p); return commands; }
static size_t capacity(const void *p) { CHECK(p); return sizeof commands; }
static size_t generate(void *p, void *b, size_t n) {
    CHECK(p && b == commands && n == sizeof commands);
    return 32;
}
static void setup(void *p, const void *b, size_t n) { CHECK(p && b == commands && n == 32); }
static void process(void *p) { CHECK(p); processed++; }
static void time_limit(void *p, uint32_t n) { CHECK(p && n == UINT32_MAX); }
typedef struct { void *object, *event; } handle_t;
typedef struct { void *data; size_t size; } buffer_t;
static uint32_t update(void *p, void *out, size_t out_size, void *performance,
                        size_t performance_size, const void *in, size_t in_size) {
    CHECK(p && out == commands && out_size == 16);
    CHECK(!performance && !performance_size && in == expected_config && in_size == 24);
    server_calls++;
    return server_result;
}
static uint32_t client(handle_t handle, const void *config) {
    CHECK(config == expected_config);
    const void *const *vtable = *(const void *const **)handle.object;
    uint32_t (*call)(void *, const buffer_t *, const buffer_t *, const buffer_t *) = (void *)vtable[8];
    buffer_t out = {commands, 16}, performance = {NULL, 0}, in = {(void *)config, 24};
    client_calls++;
    return call(handle.object, &out, &performance, &in);
}
void *module_sym(module_t *module, const char *name) {
    CHECK(module == &g_mod[1]);
    if (strstr(name, "IsValidAudioRendererParameter")) return (void *)valid;
    if (strstr(name, "AudioRenderSystem17GetWorkBufferSize")) return (void *)size;
    if (strstr(name, "RequestUpdateAudioRenderer")) return (void *)client;
    if (strstr(name, "AudioRenderSystemC1Ev")) return (void *)construct;
    if (strstr(name, "AudioRenderSystem10Initialize")) return (void *)initialize;
    if (strstr(name, "AudioRenderSystem8Finalize")) return (void *)finalize;
    if (strstr(name, "AudioRenderSystem5Start")) return (void *)start;
    if (strstr(name, "AudioRenderSystem4Stop")) return (void *)stop;
    if (strstr(name, "AudioRenderSystem6Update")) return (void *)update;
    if (strstr(name, "AudioRenderSystem21SetRenderingTimeLimit")) return (void *)set_limit;
    if (strstr(name, "AudioRenderSystem21GetRenderingTimeLimit")) return (void *)get_limit;
    if (strstr(name, "AudioRenderSystem26GetCommandBufferWorkBuffer")) return (void *)get_buffer;
    if (strstr(name, "AudioRenderSystem30GetCommandBufferWorkBufferSize")) return (void *)capacity;
    if (strstr(name, "AudioRenderSystem15GenerateCommand")) return (void *)generate;
    if (strstr(name, "CommandListProcessor17SetProcessTimeMax")) return (void *)time_limit;
    if (strstr(name, "CommandListProcessor5Setup")) return (void *)setup;
    if (strstr(name, "CommandListProcessor7Process")) return (void *)process;
    if (strstr(name, "os11SignalEvent")) return (void *)signal_event;
    if (strstr(name, "DestroySystemEvent") || strstr(name, "FinalizeEvent") || strstr(name, "FinalizeMutex")) return (void *)destroy;
    CHECK(0);
    return NULL;
}
int main(void) {
    strcpy(g_mod[0].name, "main");
    strcpy(g_mod[1].name, "sdk");
    uint64_t (*get_size)(const void *) = (void *)dm_hle_force_lookup(
        "_ZN2nn5audio30GetAudioRendererWorkBufferSizeERKNS0_22AudioRendererParameterE");
    CHECK(get_size);
    CHECK(get_size(NULL) == 0 && valid_calls == 0 && size_calls == 0);
    uint32_t parameter[14];
    uint8_t *p = (uint8_t *)parameter;
    for (unsigned i = 0; i < sizeof parameter; i++) p[i] = (uint8_t)(i + 1);
    expected_parameter = parameter;
    CHECK(get_size(parameter) == 0 && valid_calls == 1 && size_calls == 0);
    memcpy(expected_internal, p, 0x20);
    expected_internal[0x20] = p[0x20];
    expected_internal[0x22] = p[0x2c];
    expected_internal[0x23] = p[0x30];
    memcpy(expected_internal + 0x24, p + 0x24, 8);
    memcpy(expected_internal + 0x30, p + 0x34, 4);
    accept = 1;
    CHECK(get_size(parameter) == 0x8000 && valid_calls == 2 && size_calls == 1);
    // A second query must use the current parameters, not a cached fixed size.
    p[0] ^= 0x80;
    expected_internal[0] = p[0];
    CHECK(get_size(parameter) == 0x8000 && valid_calls == 3 && size_calls == 2);
    uint32_t (*open_renderer)(handle_t *, void *, const void *, void *, size_t) = (void *)dm_hle_force_lookup(
        "_ZN2nn5audio17OpenAudioRendererEPNS0_19AudioRendererHandleEPNS_2os11SystemEventERKNS0_22AudioRendererParameterEPvm");
    uint32_t (*update_renderer)(handle_t, const void *) = (void *)dm_hle_force_lookup(
        "_ZN2nn5audio26RequestUpdateAudioRendererENS0_19AudioRendererHandleEPKNS0_19AudioRendererConfigE");
    uint32_t (*start_renderer)(handle_t) = (void *)dm_hle_force_lookup("_ZN2nn5audio18StartAudioRendererENS0_19AudioRendererHandleE");
    void (*stop_renderer)(handle_t) = (void *)dm_hle_force_lookup("_ZN2nn5audio17StopAudioRendererENS0_19AudioRendererHandleE");
    void (*close_renderer)(handle_t) = (void *)dm_hle_force_lookup("_ZN2nn5audio18CloseAudioRendererENS0_19AudioRendererHandleE");
    uint32_t (*set_budget)(handle_t, int) = (void *)dm_hle_force_lookup("_ZN2nn5audio34SetAudioRendererRenderingTimeLimitENS0_19AudioRendererHandleEi");
    int (*get_budget)(handle_t) = (void *)dm_hle_force_lookup("_ZN2nn5audio34GetAudioRendererRenderingTimeLimitENS0_19AudioRendererHandleE");
    uint32_t (*execute)(handle_t) = (void *)dm_hle_force_lookup("_ZN2nn5audio29ExecuteAudioRendererRenderingENS0_19AudioRendererHandleE");
    CHECK(open_renderer && update_renderer && start_renderer && stop_renderer && close_renderer && execute && set_budget && get_budget);
    void *allocation = malloc(0x9000);
    void *work = (void *)(((uintptr_t)allocation + 0xfff) & ~(uintptr_t)0xfff);
    handle_t handle = {0};
    int event;
    CHECK(open_renderer(&handle, &event, parameter, work, 0x7000) != 0 && !handle.object && !constructed);
    parameter[11] = parameter[12] = 0; // automatic mode
    expected_internal[0x22] = expected_internal[0x23] = 0;
    init_result = 0x899;
    CHECK(open_renderer(&handle, &event, parameter, work, 0x8000) == init_result);
    CHECK(!handle.object && constructed == 1 && initialized == 1 && destroyed == 3);
    init_result = 0;
    CHECK(!open_renderer(&handle, &event, parameter, work, 0x8000) && handle.object && handle.event == &event && bound == 1);
    expected_config = commands + 32;
    CHECK(!update_renderer(handle, expected_config) && client_calls == 1 && server_calls == 1 && !processed);
    CHECK(!start_renderer(handle) && started == 1);
    CHECK(!update_renderer(handle, expected_config) && processed == 1);
    server_result = 0x5299;
    CHECK(update_renderer(handle, expected_config) == server_result && processed == 1);
    server_result = 0;
    CHECK(execute(handle) != 0); // explicit execution is only valid in manual mode
    CHECK(!set_budget(handle, 65) && get_budget(handle) == 65);
    CHECK(set_budget(handle, 101) != 0 && get_budget(handle) == 65);
    stop_renderer(handle);
    CHECK(!update_renderer(handle, expected_config) && processed == 1 && stopped == 1);
    close_renderer(handle);
    CHECK(finalized == 1 && destroyed == 6 && forgotten == 1);
    CHECK(start_renderer(handle) != 0 && update_renderer(handle, expected_config) != 0);
    // Closed handles do not reach a freed SDK object.
    close_renderer(handle);
    CHECK(finalized == 1);
    parameter[11] = parameter[12] = 1;
    expected_internal[0x22] = expected_internal[0x23] = 1;
    CHECK(!open_renderer(&handle, NULL, parameter, work, 0x8000));
    CHECK(!start_renderer(handle));
    CHECK(!update_renderer(handle, expected_config) && processed == 1);
    CHECK(!execute(handle) && processed == 2);
    close_renderer(handle);
    free(allocation);
    // Native boot previously aborted while polling this uninitialized event.
    void (*notification)(void *) = (void *)dm_hle_force_lookup(
        "_ZN2nn5audio36AcquireAudioDeviceSwitchNotificationEPNS_2os11SystemEventE");
    int (*list_devices)(void *, int) = (void *)dm_hle_force_lookup(
        "_ZN2nn5audio19ListAudioDeviceNameEPNS0_15AudioDeviceNameEi");
    void (*active_device)(void *) = (void *)dm_hle_force_lookup(
        "_ZN2nn5audio24GetActiveAudioDeviceNameEPNS0_15AudioDeviceNameE");
    int (*active_channels)(void) = (void *)dm_hle_force_lookup(
        "_ZN2nn5audio21GetActiveChannelCountEv");
    CHECK(notification && list_devices && active_device && active_channels);
    int previous_bound = bound;
    notification(&event);
    CHECK(bound == previous_bound + 1 && last_bound == &event);
    struct { uint64_t before; char names[2][0x100]; uint64_t after; } devices;
    memset(&devices, 0xa5, sizeof devices);
    CHECK(list_devices(devices.names, 0) == 0 && devices.names[0][0] == (char)0xa5);
    CHECK(list_devices(NULL, 1) == 0 && list_devices(devices.names, -1) == 0);
    CHECK(list_devices(devices.names, 2) == 1);
    CHECK(!strcmp(devices.names[0], "AudioTvOutput"));
    CHECK(devices.names[0][255] == 0 && devices.names[1][0] == (char)0xa5);
    active_device(devices.names[1]);
    CHECK(!memcmp(devices.names[0], devices.names[1], 0x100));
    CHECK(devices.before == UINT64_C(0xa5a5a5a5a5a5a5a5) && devices.after == devices.before);
    CHECK(active_channels() == 2);
    printf("audio parameter ABI: %s\n", fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}
