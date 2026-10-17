#ifndef SMO_NVN_TEXTURE_TRANSFER_H
#define SMO_NVN_TEXTURE_TRANSFER_H
#include "nvn_resources.h"
#include "nvn_tex.h"
#include "include/nvn.h"

/* A checked single-mip region. z/depth address array layers (including cube
 * faces). All pointers remain owned by the guest until the API returns. */
typedef struct {
    nvn_resource_snapshot resource;
    nvn_tex_layout layout;
    NVNcopyRegion region;
    uint8_t *storage;
    uint64_t storage_size,layer_stride,mip_offset;
    uint32_t level,width,height,pool_flags;
} nvn_texture_transfer;
int nvn_texture_transfer_prepare(const nvn_resource_snapshot *s,const nvn_view_state *v,
    const NVNcopyRegion *r,nvn_texture_transfer *out);
typedef int (*nvn_texture_flush_handler)(const nvn_texture_transfer *);
void nvn_texture_set_flush_handler(nvn_texture_flush_handler handler);
#endif
