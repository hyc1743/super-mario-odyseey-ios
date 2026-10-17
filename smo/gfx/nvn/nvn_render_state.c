/* Inline state packing adapted from d2r-mobile, Copyright (c) 2026 jhaizhou.
 * MIT license: third_party/d2r-mobile.LICENSE. Host layout, not Nintendo layout. */
#include "nvn_render_state.h"
/* Only the low byte of NVNboolean register arguments is significant. */
static unsigned boolean(uint32_t x) { return (x&255u)!=0; }
#define LOAD(k) nvn_##k##_state v=nvn_##k##_snapshot(s)
#define STORE() memcpy(s,&v,sizeof(v))
void nvn_MultisampleStateSetDefaults(NVNmultisampleState *s) { memset(s,0,sizeof *s); }
void nvn_MultisampleStateSetSamples(NVNmultisampleState *s,int samples) { LOAD(multisample);v.samples=(uint32_t)samples;STORE(); }
int nvn_MultisampleStateGetSamples(const NVNmultisampleState *s) { LOAD(multisample);return (int)v.samples; }
#define MULTI_BOOL(Name,field) \
void nvn_MultisampleStateSet##Name(NVNmultisampleState *s,uint32_t enable) { LOAD(multisample);v.field=boolean(enable);STORE(); } \
NVNboolean nvn_MultisampleStateGet##Name(const NVNmultisampleState *s) { LOAD(multisample);return (NVNboolean)v.field; }
MULTI_BOOL(MultisampleEnable,enabled)
MULTI_BOOL(AlphaToCoverageEnable,alpha_to_coverage)
MULTI_BOOL(AlphaToCoverageDither,dither)
#undef MULTI_BOOL
void nvn_BlendStateSetDefaults(NVNblendState *s) { nvn_blend_state v; memset(&v,0,sizeof(v)); v.src_rgb=v.src_a=NVN_BLEND_FUNC_ONE; v.dst_rgb=v.dst_a=NVN_BLEND_FUNC_ZERO; v.eq_rgb=v.eq_a=NVN_BLEND_EQUATION_ADD; v.advanced_premultiplied_src=v.advanced_normalized_dst=1; STORE(); }
void nvn_BlendStateSetBlendTarget(NVNblendState *s,int value) { LOAD(blend); v.target=value; STORE(); }
int nvn_BlendStateGetBlendTarget(const NVNblendState *s) { LOAD(blend); return v.target; }
void nvn_BlendStateSetAdvancedMode(NVNblendState *s,NVNblendAdvancedMode value) { LOAD(blend); v.advanced_mode=value; STORE(); }
NVNblendAdvancedMode nvn_BlendStateGetAdvancedMode(const NVNblendState *s) { LOAD(blend); return v.advanced_mode; }
void nvn_BlendStateSetAdvancedOverlap(NVNblendState *s,NVNblendAdvancedOverlap value) { LOAD(blend); v.advanced_overlap=value; STORE(); }
NVNblendAdvancedOverlap nvn_BlendStateGetAdvancedOverlap(const NVNblendState *s) { LOAD(blend); return v.advanced_overlap; }
void nvn_BlendStateSetAdvancedPremultipliedSrc(NVNblendState *s,uint32_t value) { LOAD(blend); v.advanced_premultiplied_src=boolean(value); STORE(); }
NVNboolean nvn_BlendStateGetAdvancedPremultipliedSrc(const NVNblendState *s) { LOAD(blend); return v.advanced_premultiplied_src; }
void nvn_BlendStateSetAdvancedNormalizedDst(NVNblendState *s,uint32_t value) { LOAD(blend); v.advanced_normalized_dst=boolean(value); STORE(); }
NVNboolean nvn_BlendStateGetAdvancedNormalizedDst(const NVNblendState *s) { LOAD(blend); return v.advanced_normalized_dst; }
void nvn_BlendStateSetBlendFunc(NVNblendState *s,NVNblendFunc sr,NVNblendFunc dr,NVNblendFunc sa,NVNblendFunc da) { LOAD(blend); v.src_rgb=sr; v.dst_rgb=dr; v.src_a=sa; v.dst_a=da; STORE(); }
void nvn_BlendStateGetBlendFunc(const NVNblendState *s,NVNblendFunc *sr,NVNblendFunc *dr,NVNblendFunc *sa,NVNblendFunc *da) { LOAD(blend); *sr=v.src_rgb; *dr=v.dst_rgb; *sa=v.src_a; *da=v.dst_a; }
void nvn_BlendStateSetBlendEquation(NVNblendState *s,NVNblendEquation rgb,NVNblendEquation alpha) { LOAD(blend); v.eq_rgb=rgb; v.eq_a=alpha; STORE(); }
void nvn_BlendStateGetBlendEquation(const NVNblendState *s,NVNblendEquation *rgb,NVNblendEquation *alpha) { LOAD(blend); *rgb=v.eq_rgb; *alpha=v.eq_a; }
void nvn_ColorStateSetDefaults(NVNcolorState *s) { nvn_color_state v; memset(&v,0,sizeof(v)); v.logic_op=NVN_LOGIC_OP_COPY; v.alpha_func=NVN_ALPHA_FUNC_ALWAYS; STORE(); }
void nvn_ColorStateSetLogicOp(NVNcolorState *s,NVNlogicOp value) { LOAD(color); v.logic_op=value; STORE(); }
NVNlogicOp nvn_ColorStateGetLogicOp(const NVNcolorState *s) { LOAD(color); return v.logic_op; }
void nvn_ColorStateSetAlphaTest(NVNcolorState *s,NVNalphaFunc value) { LOAD(color); v.alpha_func=value; STORE(); }
NVNalphaFunc nvn_ColorStateGetAlphaTest(const NVNcolorState *s) { LOAD(color); return v.alpha_func; }
void nvn_ColorStateSetBlendEnable(NVNcolorState *s,int i,uint32_t en) { if ((unsigned)i>=8) return; LOAD(color); v.blend_en=(v.blend_en&~(1u<<i))|(boolean(en)<<i); STORE(); }
NVNboolean nvn_ColorStateGetBlendEnable(const NVNcolorState *s,int i) { if ((unsigned)i>=8) return 0; LOAD(color); return (v.blend_en>>i)&1; }
void nvn_ChannelMaskStateSetDefaults(NVNchannelMaskState *s) { nvn_channel_state v; memset(&v,0,sizeof(v)); v.mask=UINT32_MAX; STORE(); }
void nvn_ChannelMaskStateSetChannelMask(NVNchannelMaskState *s,int i,uint32_t r,uint32_t g,uint32_t b,uint32_t a) {
    if ((unsigned)i>=8) return;
    LOAD(channel); unsigned mask=boolean(r)|(boolean(g)<<1)|(boolean(b)<<2)|(boolean(a)<<3);
    v.mask=(v.mask&~(15u<<(i*4)))|(mask<<(i*4)); STORE();
}
void nvn_ChannelMaskStateGetChannelMask(const NVNchannelMaskState *s,int i,NVNboolean *r,NVNboolean *g,NVNboolean *b,NVNboolean *a) {
    LOAD(channel); unsigned mask=(unsigned)i<8 ? (v.mask>>(i*4)) : 0; *r=mask&1; *g=(mask>>1)&1; *b=(mask>>2)&1; *a=(mask>>3)&1;
}
void nvn_PolygonStateSetDefaults(NVNpolygonState *s) { nvn_polygon_state v; memset(&v,0,sizeof(v)); v.cull=NVN_FACE_NONE; v.front=NVN_FRONT_FACE_CCW; v.mode=NVN_POLYGON_MODE_FILL; STORE(); }
void nvn_PolygonStateSetCullFace(NVNpolygonState *s,NVNface value) { LOAD(polygon); v.cull=value; STORE(); }
NVNface nvn_PolygonStateGetCullFace(const NVNpolygonState *s) { LOAD(polygon); return v.cull; }
void nvn_PolygonStateSetFrontFace(NVNpolygonState *s,NVNfrontFace value) { LOAD(polygon); v.front=value; STORE(); }
NVNfrontFace nvn_PolygonStateGetFrontFace(const NVNpolygonState *s) { LOAD(polygon); return v.front; }
void nvn_PolygonStateSetPolygonMode(NVNpolygonState *s,NVNpolygonMode value) { LOAD(polygon); v.mode=value; STORE(); }
NVNpolygonMode nvn_PolygonStateGetPolygonMode(const NVNpolygonState *s) { LOAD(polygon); return v.mode; }
void nvn_PolygonStateSetPolygonOffsetEnables(NVNpolygonState *s,int value) { LOAD(polygon); v.offset_en=value; STORE(); }
NVNpolygonOffsetEnable nvn_PolygonStateGetPolygonOffsetEnables(const NVNpolygonState *s) { LOAD(polygon); return v.offset_en; }
void nvn_DepthStencilStateSetDefaults(NVNdepthStencilState *s) { nvn_depth_state v; memset(&v,0,sizeof(v)); v.dw=1; v.df=NVN_DEPTH_FUNC_LESS; v.sf_f=v.sf_b=NVN_STENCIL_FUNC_ALWAYS; v.fail_f=v.zfail_f=v.zpass_f=v.fail_b=v.zfail_b=v.zpass_b=NVN_STENCIL_OP_KEEP; STORE(); }
void nvn_DepthStencilStateSetDepthTestEnable(NVNdepthStencilState *s,uint32_t value) { LOAD(depth); v.dt=boolean(value); STORE(); }
NVNboolean nvn_DepthStencilStateGetDepthTestEnable(const NVNdepthStencilState *s) { LOAD(depth); return v.dt; }
void nvn_DepthStencilStateSetDepthWriteEnable(NVNdepthStencilState *s,uint32_t value) { LOAD(depth); v.dw=boolean(value); STORE(); }
NVNboolean nvn_DepthStencilStateGetDepthWriteEnable(const NVNdepthStencilState *s) { LOAD(depth); return v.dw; }
void nvn_DepthStencilStateSetStencilTestEnable(NVNdepthStencilState *s,uint32_t value) { LOAD(depth); v.st=boolean(value); STORE(); }
NVNboolean nvn_DepthStencilStateGetStencilTestEnable(const NVNdepthStencilState *s) { LOAD(depth); return v.st; }
void nvn_DepthStencilStateSetDepthFunc(NVNdepthStencilState *s,NVNdepthFunc value) { LOAD(depth); v.df=value; STORE(); }
NVNdepthFunc nvn_DepthStencilStateGetDepthFunc(const NVNdepthStencilState *s) { LOAD(depth); return v.df; }
void nvn_DepthStencilStateSetStencilFunc(NVNdepthStencilState *s,NVNface f,NVNstencilFunc fn) { LOAD(depth); if (f&NVN_FACE_FRONT) v.sf_f=fn; if (f&NVN_FACE_BACK) v.sf_b=fn; STORE(); }
NVNstencilFunc nvn_DepthStencilStateGetStencilFunc(const NVNdepthStencilState *s,NVNface f) { LOAD(depth); return f==NVN_FACE_BACK ? v.sf_b : v.sf_f; }
void nvn_DepthStencilStateSetStencilOp(NVNdepthStencilState *s,NVNface f,NVNstencilOp fail,NVNstencilOp zfail,NVNstencilOp zpass) { LOAD(depth); if (f&NVN_FACE_FRONT) { v.fail_f=fail; v.zfail_f=zfail; v.zpass_f=zpass; } if (f&NVN_FACE_BACK) { v.fail_b=fail; v.zfail_b=zfail; v.zpass_b=zpass; } STORE(); }
void nvn_DepthStencilStateGetStencilOp(const NVNdepthStencilState *s,NVNface f,NVNstencilOp *fail,NVNstencilOp *zfail,NVNstencilOp *zpass) { LOAD(depth); if (f==NVN_FACE_BACK) { *fail=v.fail_b; *zfail=v.zfail_b; *zpass=v.zpass_b; } else { *fail=v.fail_f; *zfail=v.zfail_f; *zpass=v.zpass_f; } }
void nvn_VertexAttribStateSetDefaults(NVNvertexAttribState *s) { nvn_vertex_attrib_state v; memset(&v,0,sizeof(v));  STORE(); }
void nvn_VertexAttribStateSetStreamIndex(NVNvertexAttribState *s,int value) { LOAD(vertex_attrib); v.stream=value; STORE(); }
int nvn_VertexAttribStateGetStreamIndex(const NVNvertexAttribState *s) { LOAD(vertex_attrib); return v.stream; }
void nvn_VertexAttribStateSetFormat(NVNvertexAttribState *s,NVNformat f,ptrdiff_t off) { LOAD(vertex_attrib); v.fmt=f; v.offset=(uint32_t)off; STORE(); }
void nvn_VertexAttribStateGetFormat(const NVNvertexAttribState *s,NVNformat *f,ptrdiff_t *off) { LOAD(vertex_attrib); *f=v.fmt; *off=v.offset; }
void nvn_VertexStreamStateSetDefaults(NVNvertexStreamState *s) { nvn_vertex_stream_state v; memset(&v,0,sizeof(v));  STORE(); }
void nvn_VertexStreamStateSetStride(NVNvertexStreamState *s,ptrdiff_t value) { LOAD(vertex_stream); v.stride=value; STORE(); }
ptrdiff_t nvn_VertexStreamStateGetStride(const NVNvertexStreamState *s) { LOAD(vertex_stream); return v.stride; }
void nvn_VertexStreamStateSetDivisor(NVNvertexStreamState *s,int value) { LOAD(vertex_stream); v.divisor=value; STORE(); }
int nvn_VertexStreamStateGetDivisor(const NVNvertexStreamState *s) { LOAD(vertex_stream); return v.divisor; }
