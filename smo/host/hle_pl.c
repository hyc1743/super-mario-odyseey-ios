// Shared-font resource service. Font parsing/rasterization stays in the SDK.
// Files are real, unencrypted SFNT fonts; load state is only 1 after validation.
#include "hle.h"
#include <pthread.h>

static const char *const names[] = {
    "standard.ttf", "simplified.ttf", "simplified-extension.ttf",
    "traditional.ttf", "korean.ttf", "symbols.ttf"
};
typedef struct { uint8_t *data; size_t size; int attempted; } font_t;
static font_t fonts[6];
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static uint32_t be32(const uint8_t *p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}
static int valid_sfnt(const uint8_t *p, size_t n) {
    if (n < 12 || (be32(p) != 0x10000 && memcmp(p, "OTTO", 4))) return 0;
    unsigned tables = (unsigned)p[4] << 8 | p[5];
    if (!tables || tables > (n - 12) / 16) return 0;
    for (unsigned i = 0; i < tables; i++) {
        const uint8_t *entry = p + 12 + i * 16;
        size_t off = be32(entry + 8), len = be32(entry + 12);
        if (off > n || len > n - off) return 0;
    }
    return 1;
}
static font_t *get_font(int type) {
    if (type < 0 || type >= 6) return NULL;
    pthread_mutex_lock(&lock);
    font_t *font = &fonts[type];
    if (!font->attempted) {
        font->attempted = 1;
        const char *dir = getenv("SMO_FONT_DIR");
        char path[2048];
        int len = snprintf(path, sizeof path, "%s/%s", dir ? dir : "SharedFonts", names[type]);
        FILE *file = len > 0 && (size_t)len < sizeof path ? fopen(path, "rb") : NULL;
        if (file) {
            long size = -1;
            if (!fseek(file, 0, SEEK_END)) size = ftell(file);
            if (size >= 12 && size <= 64 * 1024 * 1024 && !fseek(file, 0, SEEK_SET)) {
                uint8_t *data = malloc((size_t)size);
                if (data && fread(data, 1, (size_t)size, file) == (size_t)size && valid_sfnt(data, (size_t)size)) {
                    font->data = data;
                    font->size = (size_t)size;
                } else free(data);
            }
            fclose(file);
        }
        if (font->data) LOG("SHARED_FONT_READY type=%d bytes=%zu file=%s", type, font->size, names[type]);
        else LOG("【红】SharedFont type=%d missing/invalid SFNT: %s (SMO_FONT_DIR)", type, path);
    }
    pthread_mutex_unlock(&lock);
    return font;
}
static int get_state(int type) { font_t *f = get_font(type); return f && f->data ? 1 : 0; }
static const void *get_address(int type) { font_t *f = get_font(type); return f ? f->data : NULL; }
static uint64_t get_size(int type) { font_t *f = get_font(type); return f ? f->size : 0; }

static const hle_impl_ent impl[] = {
    {"_ZN2nn2pl22GetSharedFontLoadStateENS0_14SharedFontTypeE", (void *)get_state, "pl: validated host SFNT resource load state"},
    {"_ZN2nn2pl20GetSharedFontAddressENS0_14SharedFontTypeE", (void *)get_address, "pl: stable SFNT bytes for SDK font engine"},
    {"_ZN2nn2pl17GetSharedFontSizeENS0_14SharedFontTypeE", (void *)get_size, "pl: actual SFNT file size"},
};
static void __attribute__((constructor)) register_pl(void) { hle_impl_register(impl, SMO_ARRAY_LEN(impl)); }
