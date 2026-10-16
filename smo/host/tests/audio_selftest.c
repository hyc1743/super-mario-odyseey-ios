#include "../hle.h"

module_t g_mod[MAX_MOD];
int g_nmod = 2;
void unimpl_stubs(void) {}
int svc_dispatch(unsigned n, uint64_t *r) { (void)n; (void)r; return -1; }
static int fails, valid_calls, size_calls, accept;
static const void *expected_parameter;
static uint8_t expected_internal[0x34];
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)
static int valid(const void *p) {
    valid_calls++;
    CHECK(p == expected_parameter);
    return accept;
}
static uint64_t size(const void *p) {
    size_calls++;
    CHECK(!memcmp(p, expected_internal, sizeof expected_internal));
    return 0x12345000;
}
void *module_sym(module_t *module, const char *name) {
    CHECK(module == &g_mod[1]);
    if (strstr(name, "IsValidAudioRendererParameter")) return (void *)valid;
    if (strstr(name, "AudioRenderSystem17GetWorkBufferSize")) return (void *)size;
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
    CHECK(get_size(parameter) == 0x12345000 && valid_calls == 2 && size_calls == 1);
    // A second query must use the current parameters, not a cached fixed size.
    p[0] ^= 0x80;
    expected_internal[0] = p[0];
    CHECK(get_size(parameter) == 0x12345000 && valid_calls == 3 && size_calls == 2);
    printf("audio parameter ABI: %s\n", fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}
