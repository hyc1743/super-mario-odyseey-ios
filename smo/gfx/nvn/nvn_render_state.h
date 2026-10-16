/* Inline state packing adapted from d2r-mobile, Copyright (c) 2026 jhaizhou.
 * MIT license: third_party/d2r-mobile.LICENSE. Host layout, not Nintendo layout. */
#ifndef SMO_NVN_RENDER_STATE_H
#define SMO_NVN_RENDER_STATE_H
#include "include/nvn.h"
#include <string.h>
typedef struct { uint64_t target:3,src_rgb:7,dst_rgb:7,src_a:7,dst_a:7,eq_rgb:3,eq_a:3,advanced_mode:6,advanced_overlap:2,advanced_premultiplied_src:1,advanced_normalized_dst:1; } nvn_blend_state;
_Static_assert(sizeof(nvn_blend_state)==sizeof(NVNblendState), "blendState inline size");
static inline nvn_blend_state nvn_blend_snapshot(const void *obj) { nvn_blend_state v; memcpy(&v,obj,sizeof(v)); return v; }
typedef struct { uint32_t blend_en:8,logic_op:4,alpha_func:4; } nvn_color_state;
_Static_assert(sizeof(nvn_color_state)==sizeof(NVNcolorState), "colorState inline size");
static inline nvn_color_state nvn_color_snapshot(const void *obj) { nvn_color_state v; memcpy(&v,obj,sizeof(v)); return v; }
typedef struct { uint32_t mask; } nvn_channel_state;
_Static_assert(sizeof(nvn_channel_state)==sizeof(NVNchannelMaskState), "channelMaskState inline size");
static inline nvn_channel_state nvn_channel_snapshot(const void *obj) { nvn_channel_state v; memcpy(&v,obj,sizeof(v)); return v; }
typedef struct { uint32_t cull:2,front:1,mode:2,offset_en:3; } nvn_polygon_state;
_Static_assert(sizeof(nvn_polygon_state)==sizeof(NVNpolygonState), "polygonState inline size");
static inline nvn_polygon_state nvn_polygon_snapshot(const void *obj) { nvn_polygon_state v; memcpy(&v,obj,sizeof(v)); return v; }
typedef struct { uint64_t dt:1,dw:1,df:4,st:1,sf_f:4,sf_b:4,fail_f:4,zfail_f:4,zpass_f:4,fail_b:4,zfail_b:4,zpass_b:4; } nvn_depth_state;
_Static_assert(sizeof(nvn_depth_state)==sizeof(NVNdepthStencilState), "depthStencilState inline size");
static inline nvn_depth_state nvn_depth_snapshot(const void *obj) { nvn_depth_state v; memcpy(&v,obj,sizeof(v)); return v; }
typedef struct { uint32_t fmt:8,offset:19,stream:5; } nvn_vertex_attrib_state;
_Static_assert(sizeof(nvn_vertex_attrib_state)==sizeof(NVNvertexAttribState), "vertexAttribState inline size");
static inline nvn_vertex_attrib_state nvn_vertex_attrib_snapshot(const void *obj) { nvn_vertex_attrib_state v; memcpy(&v,obj,sizeof(v)); return v; }
typedef struct { uint32_t stride,divisor; } nvn_vertex_stream_state;
_Static_assert(sizeof(nvn_vertex_stream_state)==sizeof(NVNvertexStreamState), "vertexStreamState inline size");
static inline nvn_vertex_stream_state nvn_vertex_stream_snapshot(const void *obj) { nvn_vertex_stream_state v; memcpy(&v,obj,sizeof(v)); return v; }
void nvn_BlendStateSetDefaults(NVNblendState*);
void nvn_BlendStateSetBlendTarget(NVNblendState*, int);
void nvn_BlendStateSetBlendFunc(NVNblendState*, NVNblendFunc, NVNblendFunc, NVNblendFunc, NVNblendFunc);
void nvn_BlendStateSetBlendEquation(NVNblendState*, NVNblendEquation, NVNblendEquation);
void nvn_BlendStateSetAdvancedMode(NVNblendState*, NVNblendAdvancedMode);
void nvn_BlendStateSetAdvancedOverlap(NVNblendState*, NVNblendAdvancedOverlap);
void nvn_BlendStateSetAdvancedPremultipliedSrc(NVNblendState*, uint32_t);
void nvn_BlendStateSetAdvancedNormalizedDst(NVNblendState*, uint32_t);
int nvn_BlendStateGetBlendTarget(const NVNblendState*);
void nvn_BlendStateGetBlendFunc(const NVNblendState*, NVNblendFunc*, NVNblendFunc*, NVNblendFunc*, NVNblendFunc*);
void nvn_BlendStateGetBlendEquation(const NVNblendState*, NVNblendEquation*, NVNblendEquation*);
NVNblendAdvancedMode nvn_BlendStateGetAdvancedMode(const NVNblendState*);
NVNblendAdvancedOverlap nvn_BlendStateGetAdvancedOverlap(const NVNblendState*);
NVNboolean nvn_BlendStateGetAdvancedPremultipliedSrc(const NVNblendState*);
NVNboolean nvn_BlendStateGetAdvancedNormalizedDst(const NVNblendState*);
void nvn_ColorStateSetDefaults(NVNcolorState*);
void nvn_ColorStateSetBlendEnable(NVNcolorState*, int, uint32_t);
void nvn_ColorStateSetLogicOp(NVNcolorState*, NVNlogicOp);
void nvn_ColorStateSetAlphaTest(NVNcolorState*, NVNalphaFunc);
NVNboolean nvn_ColorStateGetBlendEnable(const NVNcolorState*, int);
NVNlogicOp nvn_ColorStateGetLogicOp(const NVNcolorState*);
NVNalphaFunc nvn_ColorStateGetAlphaTest(const NVNcolorState*);
void nvn_ChannelMaskStateSetDefaults(NVNchannelMaskState*);
void nvn_ChannelMaskStateSetChannelMask(NVNchannelMaskState*, int, uint32_t, uint32_t, uint32_t, uint32_t);
void nvn_ChannelMaskStateGetChannelMask(const NVNchannelMaskState*, int, NVNboolean*, NVNboolean*, NVNboolean*, NVNboolean*);
void nvn_PolygonStateSetDefaults(NVNpolygonState*);
void nvn_PolygonStateSetCullFace(NVNpolygonState*, NVNface);
void nvn_PolygonStateSetFrontFace(NVNpolygonState*, NVNfrontFace);
void nvn_PolygonStateSetPolygonMode(NVNpolygonState*, NVNpolygonMode);
void nvn_PolygonStateSetPolygonOffsetEnables(NVNpolygonState*, int);
NVNface nvn_PolygonStateGetCullFace(const NVNpolygonState*);
NVNfrontFace nvn_PolygonStateGetFrontFace(const NVNpolygonState*);
NVNpolygonMode nvn_PolygonStateGetPolygonMode(const NVNpolygonState*);
NVNpolygonOffsetEnable nvn_PolygonStateGetPolygonOffsetEnables(const NVNpolygonState*);
void nvn_DepthStencilStateSetDefaults(NVNdepthStencilState*);
void nvn_DepthStencilStateSetDepthTestEnable(NVNdepthStencilState*, uint32_t);
void nvn_DepthStencilStateSetDepthWriteEnable(NVNdepthStencilState*, uint32_t);
void nvn_DepthStencilStateSetDepthFunc(NVNdepthStencilState*, NVNdepthFunc);
void nvn_DepthStencilStateSetStencilTestEnable(NVNdepthStencilState*, uint32_t);
void nvn_DepthStencilStateSetStencilFunc(NVNdepthStencilState*, NVNface, NVNstencilFunc);
void nvn_DepthStencilStateSetStencilOp(NVNdepthStencilState*, NVNface, NVNstencilOp, NVNstencilOp, NVNstencilOp);
NVNboolean nvn_DepthStencilStateGetDepthTestEnable(const NVNdepthStencilState*);
NVNboolean nvn_DepthStencilStateGetDepthWriteEnable(const NVNdepthStencilState*);
NVNdepthFunc nvn_DepthStencilStateGetDepthFunc(const NVNdepthStencilState*);
NVNboolean nvn_DepthStencilStateGetStencilTestEnable(const NVNdepthStencilState*);
NVNstencilFunc nvn_DepthStencilStateGetStencilFunc(const NVNdepthStencilState*, NVNface);
void nvn_DepthStencilStateGetStencilOp(const NVNdepthStencilState*, NVNface, NVNstencilOp*, NVNstencilOp*, NVNstencilOp*);
void nvn_VertexAttribStateSetDefaults(NVNvertexAttribState*);
void nvn_VertexAttribStateSetFormat(NVNvertexAttribState*, NVNformat, ptrdiff_t);
void nvn_VertexAttribStateSetStreamIndex(NVNvertexAttribState*, int);
void nvn_VertexAttribStateGetFormat(const NVNvertexAttribState*, NVNformat*, ptrdiff_t*);
int nvn_VertexAttribStateGetStreamIndex(const NVNvertexAttribState*);
void nvn_VertexStreamStateSetDefaults(NVNvertexStreamState*);
void nvn_VertexStreamStateSetStride(NVNvertexStreamState*, ptrdiff_t);
void nvn_VertexStreamStateSetDivisor(NVNvertexStreamState*, int);
ptrdiff_t nvn_VertexStreamStateGetStride(const NVNvertexStreamState*);
int nvn_VertexStreamStateGetDivisor(const NVNvertexStreamState*);
#endif
