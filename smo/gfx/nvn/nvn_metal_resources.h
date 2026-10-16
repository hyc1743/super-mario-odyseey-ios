#ifndef SMO_NVN_METAL_RESOURCES_H
#define SMO_NVN_METAL_RESOURCES_H
#include "nvn_resources.h"
#ifdef __OBJC__
#import <Metal/Metal.h>
id<MTLTexture> nvn_metal_texture(id<MTLDevice> device,
    const nvn_resource_snapshot *resource, const nvn_view_state *view, int render_target);
id<MTLSamplerState> nvn_metal_sampler(id<MTLDevice> device,
    const nvn_resource_snapshot *resource);
/* Caller must drain GPU submissions before clearing cached resources. */
void nvn_metal_resources_reset(void);
#endif
#endif
