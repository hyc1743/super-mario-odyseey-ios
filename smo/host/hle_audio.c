// Audren service boundary. Keep the SDK's client configuration implementation.
// Its shipped server also exports the pure work-buffer calculation, allowing
// the host to answer this query without IPC or a guessed fixed allocation.
#include "hle.h"

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
    const uint8_t *external = parameter;
    // SDK+0x17db28..0x17dba4: public parameter -> IPC internal layout.
    // In particular, execution/rendering modes move from words to bytes;
    // the revision moves from +0x34 to +0x30, and reserved bytes remain zero.
    uint32_t internal_words[13] = {0};
    uint8_t *internal = (uint8_t *)internal_words;
    memcpy(internal, external, 0x20);
    internal[0x20] = external[0x20];
    memcpy(internal + 0x24, external + 0x24, 8);
    internal[0x22] = external[0x2c];
    internal[0x23] = external[0x30];
    memcpy(internal + 0x30, external + 0x34, 4);
    uint64_t (*size)(const void *) = sdk_function(
        "_ZN2nn5audio6server17AudioRenderSystem17GetWorkBufferSizeERKNS0_6detail30AudioRendererParameterInternalE");
    uint64_t bytes = size(internal);
    if (!bytes || (bytes & 0xfff)) die("Audio SDK returned invalid work-buffer size: %#llx", (unsigned long long)bytes);
    LOG("AUDIO_RENDERER_WORK_SIZE bytes=%llu", (unsigned long long)bytes);
    return bytes;
}

static const hle_impl_ent impl[] = {
    {"_ZN2nn5audio30GetAudioRendererWorkBufferSizeERKNS0_22AudioRendererParameterE",
     (void *)h_renderer_work_size, "SDK server's pure size calculation; no service IPC"},
};
static void __attribute__((constructor)) audio_reg(void) {
    hle_impl_register(impl, SMO_ARRAY_LEN(impl));
}
