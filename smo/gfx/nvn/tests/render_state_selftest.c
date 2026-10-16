#include "../nvn_render_state.h"
#include <assert.h>
#include <stdio.h>

static void blend(void) {
    NVNblendState original, copy;
    nvn_BlendStateSetDefaults(&original);
    NVNblendFunc sr, dr, sa, da;
    NVNblendEquation er, ea;
    nvn_BlendStateGetBlendFunc(&original, &sr, &dr, &sa, &da);
    assert(sr == NVN_BLEND_FUNC_ONE && sa == sr);
    assert(dr == NVN_BLEND_FUNC_ZERO && da == dr);
    nvn_BlendStateSetBlendTarget(&original, 7);
    nvn_BlendStateSetBlendFunc(&original, NVN_BLEND_FUNC_SRC_ALPHA,
        NVN_BLEND_FUNC_ONE_MINUS_SRC_ALPHA, NVN_BLEND_FUNC_CONSTANT_ALPHA,
        NVN_BLEND_FUNC_ONE_MINUS_CONSTANT_ALPHA);
    nvn_BlendStateSetBlendEquation(&original, NVN_BLEND_EQUATION_SUB,
        NVN_BLEND_EQUATION_REVERSE_SUB);
    nvn_BlendStateSetAdvancedMode(&original, NVN_BLEND_ADVANCED_MODE_HSL_COLOR);
    nvn_BlendStateSetAdvancedOverlap(&original, NVN_BLEND_ADVANCED_OVERLAP_CONJOINT);
    nvn_BlendStateSetAdvancedNormalizedDst(&original, 0x100);
    nvn_BlendStateSetAdvancedPremultipliedSrc(&original, 0x102);
    memcpy(&copy, &original, sizeof(copy));
    nvn_BlendStateSetDefaults(&original);
    assert(nvn_BlendStateGetBlendTarget(&copy) == 7);
    nvn_BlendStateGetBlendFunc(&copy, &sr, &dr, &sa, &da);
    assert(sr == NVN_BLEND_FUNC_SRC_ALPHA && dr == NVN_BLEND_FUNC_ONE_MINUS_SRC_ALPHA);
    assert(sa == NVN_BLEND_FUNC_CONSTANT_ALPHA && da == NVN_BLEND_FUNC_ONE_MINUS_CONSTANT_ALPHA);
    nvn_BlendStateGetBlendEquation(&copy, &er, &ea);
    assert(er == NVN_BLEND_EQUATION_SUB && ea == NVN_BLEND_EQUATION_REVERSE_SUB);
    assert(nvn_BlendStateGetAdvancedMode(&copy) == NVN_BLEND_ADVANCED_MODE_HSL_COLOR);
    assert(nvn_BlendStateGetAdvancedOverlap(&copy) == NVN_BLEND_ADVANCED_OVERLAP_CONJOINT);
    assert(nvn_BlendStateGetAdvancedNormalizedDst(&copy) == 0);
    assert(nvn_BlendStateGetAdvancedPremultipliedSrc(&copy) == 1);
    nvn_blend_state snapshot = nvn_blend_snapshot(&copy);
    nvn_BlendStateSetDefaults(&copy);
    assert(snapshot.target == 7 && snapshot.src_a == NVN_BLEND_FUNC_CONSTANT_ALPHA);
}

static void color_and_mask(void) {
    NVNcolorState state;
    nvn_ColorStateSetDefaults(&state);
    assert(nvn_ColorStateGetLogicOp(&state) == NVN_LOGIC_OP_COPY);
    assert(nvn_ColorStateGetAlphaTest(&state) == NVN_ALPHA_FUNC_ALWAYS);
    for (int i = 0; i < 8; i++) {
        assert(!nvn_ColorStateGetBlendEnable(&state, i));
        nvn_ColorStateSetBlendEnable(&state, i, i % 2 ? 0x100 : 0x101);
    }
    nvn_ColorStateSetLogicOp(&state, NVN_LOGIC_OP_XOR);
    nvn_ColorStateSetAlphaTest(&state, NVN_ALPHA_FUNC_GEQUAL);
    NVNcolorState copy = state;
    nvn_ColorStateSetDefaults(&state);
    assert(nvn_ColorStateGetLogicOp(&copy) == NVN_LOGIC_OP_XOR);
    assert(nvn_ColorStateGetAlphaTest(&copy) == NVN_ALPHA_FUNC_GEQUAL);
    for (int i = 0; i < 8; i++) assert(nvn_ColorStateGetBlendEnable(&copy, i) == !(i % 2));

    NVNchannelMaskState mask;
    nvn_ChannelMaskStateSetDefaults(&mask);
    NVNboolean out[6] = {0x55, 0, 0, 0, 0, 0xaa};
    for (int i = 0; i < 8; i++) {
        nvn_ChannelMaskStateGetChannelMask(&mask, i, out+1, out+2, out+3, out+4);
        assert(out[1] && out[2] && out[3] && out[4]);
        nvn_ChannelMaskStateSetChannelMask(&mask, i, i&1, i&2, i&4, 0x100);
    }
    NVNchannelMaskState mask_copy = mask;
    nvn_ChannelMaskStateSetDefaults(&mask);
    for (int i = 0; i < 8; i++) {
        nvn_ChannelMaskStateGetChannelMask(&mask_copy, i, out+1, out+2, out+3, out+4);
        assert(out[1] == !!(i&1) && out[2] == !!(i&2) && out[3] == !!(i&4) && !out[4]);
        assert(out[0] == 0x55 && out[5] == 0xaa);
    }
}

static void depth_and_polygon(void) {
    NVNdepthStencilState state;
    nvn_DepthStencilStateSetDefaults(&state);
    assert(!nvn_DepthStencilStateGetDepthTestEnable(&state));
    assert(nvn_DepthStencilStateGetDepthWriteEnable(&state));
    assert(nvn_DepthStencilStateGetDepthFunc(&state) == NVN_DEPTH_FUNC_LESS);
    assert(!nvn_DepthStencilStateGetStencilTestEnable(&state));
    nvn_DepthStencilStateSetDepthTestEnable(&state, 0x201);
    nvn_DepthStencilStateSetDepthWriteEnable(&state, 0x200);
    nvn_DepthStencilStateSetDepthFunc(&state, NVN_DEPTH_FUNC_GEQUAL);
    nvn_DepthStencilStateSetStencilTestEnable(&state, 1);
    nvn_DepthStencilStateSetStencilFunc(&state, NVN_FACE_FRONT_AND_BACK, NVN_STENCIL_FUNC_EQUAL);
    nvn_DepthStencilStateSetStencilFunc(&state, NVN_FACE_BACK, NVN_STENCIL_FUNC_NEVER);
    nvn_DepthStencilStateSetStencilOp(&state, NVN_FACE_FRONT_AND_BACK,
        NVN_STENCIL_OP_REPLACE, NVN_STENCIL_OP_INCR, NVN_STENCIL_OP_DECR);
    nvn_DepthStencilStateSetStencilOp(&state, NVN_FACE_BACK,
        NVN_STENCIL_OP_KEEP, NVN_STENCIL_OP_ZERO, NVN_STENCIL_OP_INVERT);
    NVNdepthStencilState copy = state;
    nvn_DepthStencilStateSetDefaults(&state);
    assert(nvn_DepthStencilStateGetDepthTestEnable(&copy));
    assert(!nvn_DepthStencilStateGetDepthWriteEnable(&copy));
    assert(nvn_DepthStencilStateGetDepthFunc(&copy) == NVN_DEPTH_FUNC_GEQUAL);
    assert(nvn_DepthStencilStateGetStencilTestEnable(&copy));
    assert(nvn_DepthStencilStateGetStencilFunc(&copy, NVN_FACE_FRONT) == NVN_STENCIL_FUNC_EQUAL);
    assert(nvn_DepthStencilStateGetStencilFunc(&copy, NVN_FACE_BACK) == NVN_STENCIL_FUNC_NEVER);
    NVNstencilOp fail, zfail, zpass;
    nvn_DepthStencilStateGetStencilOp(&copy, NVN_FACE_FRONT, &fail, &zfail, &zpass);
    assert(fail == NVN_STENCIL_OP_REPLACE && zfail == NVN_STENCIL_OP_INCR && zpass == NVN_STENCIL_OP_DECR);
    nvn_DepthStencilStateGetStencilOp(&copy, NVN_FACE_BACK, &fail, &zfail, &zpass);
    assert(fail == NVN_STENCIL_OP_KEEP && zfail == NVN_STENCIL_OP_ZERO && zpass == NVN_STENCIL_OP_INVERT);

    NVNpolygonState polygon;
    nvn_PolygonStateSetDefaults(&polygon);
    assert(nvn_PolygonStateGetCullFace(&polygon) == NVN_FACE_NONE);
    assert(nvn_PolygonStateGetFrontFace(&polygon) == NVN_FRONT_FACE_CCW);
    assert(nvn_PolygonStateGetPolygonMode(&polygon) == NVN_POLYGON_MODE_FILL);
    nvn_PolygonStateSetCullFace(&polygon, NVN_FACE_BACK);
    nvn_PolygonStateSetFrontFace(&polygon, NVN_FRONT_FACE_CW);
    nvn_PolygonStateSetPolygonMode(&polygon, NVN_POLYGON_MODE_LINE);
    nvn_PolygonStateSetPolygonOffsetEnables(&polygon, NVN_POLYGON_OFFSET_ENABLE_LINE | NVN_POLYGON_OFFSET_ENABLE_FILL);
    NVNpolygonState pc = polygon;
    nvn_PolygonStateSetDefaults(&polygon);
    assert(nvn_PolygonStateGetCullFace(&pc) == NVN_FACE_BACK);
    assert(nvn_PolygonStateGetFrontFace(&pc) == NVN_FRONT_FACE_CW);
    assert(nvn_PolygonStateGetPolygonMode(&pc) == NVN_POLYGON_MODE_LINE);
    assert(nvn_PolygonStateGetPolygonOffsetEnables(&pc) == 6);
}

static void vertex_and_unaligned(void) {
    /* NVN opaque objects have byte alignment. Exercise a deliberately unaligned
     * instance so that snapshots cannot depend on typed pointer casts. */
    unsigned char storage[sizeof(NVNvertexAttribState)+1];
    NVNvertexAttribState *attribute = (void *)(storage+1);
    nvn_VertexAttribStateSetDefaults(attribute);
    nvn_VertexAttribStateSetFormat(attribute, NVN_FORMAT_RGBA32F, (1<<19)-1);
    nvn_VertexAttribStateSetStreamIndex(attribute, 15);
    NVNvertexAttribState copy;
    memcpy(&copy, attribute, sizeof(copy));
    nvn_VertexAttribStateSetDefaults(attribute);
    NVNformat format;
    ptrdiff_t offset;
    nvn_VertexAttribStateGetFormat(&copy, &format, &offset);
    assert(format == NVN_FORMAT_RGBA32F && offset == (1<<19)-1);
    assert(nvn_VertexAttribStateGetStreamIndex(&copy) == 15);
    NVNvertexStreamState stream;
    nvn_VertexStreamStateSetDefaults(&stream);
    assert(nvn_VertexStreamStateGetStride(&stream) == 0);
    assert(nvn_VertexStreamStateGetDivisor(&stream) == 0);
    nvn_VertexStreamStateSetStride(&stream, 65535);
    nvn_VertexStreamStateSetDivisor(&stream, 7);
    NVNvertexStreamState sc = stream;
    nvn_VertexStreamStateSetDefaults(&stream);
    assert(nvn_VertexStreamStateGetStride(&sc) == 65535);
    assert(nvn_VertexStreamStateGetDivisor(&sc) == 7);
}

int main(void) {
    blend(); color_and_mask(); depth_and_polygon(); vertex_and_unaligned();
    puts("render_state_selftest: PASS (inline copies, snapshots, defaults, boolean ABI)");
    return 0;
}
