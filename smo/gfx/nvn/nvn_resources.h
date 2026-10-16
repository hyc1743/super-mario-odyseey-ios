#ifndef SMO_NVN_RESOURCES_H
#define SMO_NVN_RESOURCES_H
#include <stdint.h>

typedef struct nvn_resource_snapshot {
    uint64_t object;
    uint64_t words[16];
} nvn_resource_snapshot;

/* Inline and pointer-free: guests copy NVNtextureView by value. */
typedef struct nvn_view_state {
    uint32_t flags;
    int32_t base_level, level_count, base_layer, layer_count, format, target;
    uint8_t swizzle[4];
    int32_t depth_stencil;
    uint32_t reserved;
} nvn_view_state;
_Static_assert(sizeof(nvn_view_state) == 40, "NVNtextureView ABI");
enum {
    NVN_VIEW_LEVELS=1, NVN_VIEW_LAYERS=2, NVN_VIEW_FORMAT=4,
    NVN_VIEW_TARGET=8, NVN_VIEW_SWIZZLE=16, NVN_VIEW_DEPTH_STENCIL=32
};

/* Returns a copy, never a pointer into a pool's mutable descriptor storage.
 * An unregistered/out-of-range descriptor fails. Original resource object
 * finalization does not invalidate its registered descriptor snapshot. */
int nvn_resources_texture(const void *pool, uint32_t id, int image,
                          nvn_resource_snapshot *out, nvn_view_state *view);
int nvn_resources_sampler(const void *pool, uint32_t id, nvn_resource_snapshot *out);
int nvn_resources_snapshot(const void *object, uint32_t type, nvn_resource_snapshot *out);
/* Opaque resource identity survives guest memcpy; owner and serial are checked
 * against the live state index before following an alias. Uses 24 object bytes. */
void nvn_resources_stamp(void *object,uint32_t type);
void nvn_resources_view(const void *object, nvn_view_state *out);

#endif
