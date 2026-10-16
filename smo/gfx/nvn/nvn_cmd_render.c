#include "nvn_cmd_render.h"
#include "nvn_state.h"
#include "nvn_host.h"

static void snapshot(nvn_resource_snapshot *out, const void *object, unsigned type) {
    memset(out,0,sizeof *out);
    if (!object) return;
    if (!nvn_resources_snapshot(object,type,out)) NVN_DIE("missing resource type=%u object=%p",type,object);
}
static void attachment(nvn_attachment *out, const NVNtexture *texture, const NVNtextureView *view) {
    memset(out,0,sizeof *out);
    snapshot(&out->resource,texture,12);
    if (view) { out->has_view=1; memcpy(&out->view,view,sizeof out->view); }
}
void nvn_CommandBufferSetRenderTargets(NVNcommandBuffer *c, int count,
        const NVNtexture *const *colors, const NVNtextureView *const *views,
        const NVNtexture *depth, const NVNtextureView *depth_view) {
    if (count<0 || count>8) NVN_DIE("invalid render target count=%d",count);
    nvn_cmd_targets *p=nvn_cmd_alloc(c,NVN_CMD_SET_RT,sizeof *p);
    p->count=count;
    for (int i=0;i<count;++i) attachment(&p->colors[i],colors?colors[i]:NULL,views?views[i]:NULL);
    attachment(&p->depth,depth,depth_view);
}
void nvn_CommandBufferClearColor(NVNcommandBuffer *c, int index, const float *rgba, int mask) {
    if (index<0 || index>=8 || !rgba || (mask&~15)) NVN_DIE("invalid ClearColor");
    nvn_cmd_color p={index,mask,{rgba[0],rgba[1],rgba[2],rgba[3]}};
    memcpy(nvn_cmd_alloc(c,NVN_CMD_CLEAR_COLOR,sizeof p),&p,sizeof p);
}
void nvn_CommandBufferClearDepthStencil(NVNcommandBuffer *c,float depth,NVNboolean mask,int stencil,int stencil_mask) {
    nvn_cmd_depth p={depth,mask!=0,stencil,stencil_mask};
    memcpy(nvn_cmd_alloc(c,NVN_CMD_CLEAR_DEPTH,sizeof p),&p,sizeof p);
}
static void array(NVNcommandBuffer *c,uint32_t op,int first,int count,int limit,const void *data,size_t element) {
    if (first<0 || first>limit || count<0 || count>limit-first || (count && !data))
        NVN_DIE("invalid command array op=%u first=%d count=%d",op,first,count);
    size_t bytes=(size_t)count*element;
    nvn_cmd_array *p=nvn_cmd_alloc(c,op,sizeof *p+bytes);
    p->first=first; p->count=count;
    if (bytes) memcpy(p+1,data,bytes);
}
void nvn_CommandBufferSetViewports(NVNcommandBuffer *c,int first,int count,const float *v) {
    array(c,NVN_CMD_VIEWPORTS,first,count,16,v,4*sizeof(float));
}
void nvn_CommandBufferSetViewport(NVNcommandBuffer *c,int x,int y,int width,int height) {
    float v[4]={(float)x,(float)y,(float)width,(float)height};
    nvn_CommandBufferSetViewports(c,0,1,v);
}
void nvn_CommandBufferSetScissors(NVNcommandBuffer *c,int first,int count,const int *v) {
    array(c,NVN_CMD_SCISSORS,first,count,16,v,4*sizeof(int));
}
void nvn_CommandBufferSetScissor(NVNcommandBuffer *c,int x,int y,int width,int height) {
    int v[4]={x,y,width,height}; nvn_CommandBufferSetScissors(c,0,1,v);
}
void nvn_CommandBufferSetDepthRanges(NVNcommandBuffer *c,int first,int count,const float *v) {
    array(c,NVN_CMD_DEPTH_RANGES,first,count,16,v,2*sizeof(float));
}
void nvn_CommandBufferSetDepthRange(NVNcommandBuffer *c,float near,float far) {
    float v[2]={near,far}; nvn_CommandBufferSetDepthRanges(c,0,1,v);
}
static void state(NVNcommandBuffer *c,unsigned kind,const void *s,size_t size) {
    if (!s || size>24) NVN_DIE("invalid render state");
    nvn_cmd_state *p=nvn_cmd_alloc(c,NVN_CMD_BIND_STATE,sizeof *p);
    p->kind=kind; p->size=(uint32_t)size; memcpy(p->bytes,s,size);
}
#define BIND_STATE(name,kind,type) void nvn_CommandBufferBind##name(NVNcommandBuffer *c,const type *s) { state(c,kind,s,sizeof *s); }
BIND_STATE(BlendState,NVN_STATE_BLEND,NVNblendState)
BIND_STATE(ColorState,NVN_STATE_COLOR,NVNcolorState)
BIND_STATE(ChannelMaskState,NVN_STATE_CHANNEL,NVNchannelMaskState)
BIND_STATE(DepthStencilState,NVN_STATE_DEPTH,NVNdepthStencilState)
BIND_STATE(PolygonState,NVN_STATE_POLYGON,NVNpolygonState)
BIND_STATE(MultisampleState,NVN_STATE_MULTISAMPLE,NVNmultisampleState)
#undef BIND_STATE
void nvn_CommandBufferBindVertexAttribState(NVNcommandBuffer *c,int count,const NVNvertexAttribState *s) {
    array(c,NVN_CMD_BIND_VATTR,0,count,16,s,sizeof *s);
}
void nvn_CommandBufferBindVertexStreamState(NVNcommandBuffer *c,int count,const NVNvertexStreamState *s) {
    array(c,NVN_CMD_BIND_VSTREAM,0,count,16,s,sizeof *s);
}
static void buffers(NVNcommandBuffer *c,int kind,int stage,int first,int count,const NVNbufferRange *ranges) {
    if (stage<0 || stage>=6 || first<0 || first>32 || count<0 || count>32-first || (count&&!ranges))
        NVN_DIE("invalid buffer bindings");
    for (int i=0;i<count;++i)
        if (ranges[i].size>UINT64_MAX-ranges[i].address) NVN_DIE("buffer binding overflow");
    size_t size=sizeof(nvn_cmd_buffers)+(size_t)count*sizeof *ranges;
    nvn_cmd_buffers *p=nvn_cmd_alloc(c,NVN_CMD_BIND_BUFFER,size);
    *p=(nvn_cmd_buffers){kind,stage,first,count};
    if (count) memcpy(p+1,ranges,(size_t)count*sizeof *ranges);
}
void nvn_CommandBufferBindVertexBuffers(NVNcommandBuffer *c,int first,int count,const NVNbufferRange *r) {
    buffers(c,NVN_BIND_VERTEX,0,first,count,r);
}
void nvn_CommandBufferBindVertexBuffer(NVNcommandBuffer *c,int index,NVNbufferAddress address,size_t size) {
    NVNbufferRange r={address,size}; buffers(c,NVN_BIND_VERTEX,0,index,1,&r);
}
void nvn_CommandBufferBindUniformBuffers(NVNcommandBuffer *c,NVNshaderStage stage,int first,int count,const NVNbufferRange *r) {
    buffers(c,NVN_BIND_UNIFORM,stage,first,count,r);
}
void nvn_CommandBufferBindUniformBuffer(NVNcommandBuffer *c,NVNshaderStage stage,int index,NVNbufferAddress address,size_t size) {
    NVNbufferRange r={address,size}; buffers(c,NVN_BIND_UNIFORM,stage,index,1,&r);
}
void nvn_CommandBufferBindStorageBuffers(NVNcommandBuffer *c,NVNshaderStage stage,int first,int count,const NVNbufferRange *r) {
    buffers(c,NVN_BIND_STORAGE,stage,first,count,r);
}
void nvn_CommandBufferBindStorageBuffer(NVNcommandBuffer *c,NVNshaderStage stage,int index,NVNbufferAddress address,size_t size) {
    NVNbufferRange r={address,size}; buffers(c,NVN_BIND_STORAGE,stage,index,1,&r);
}
void nvn_CommandBufferBindProgram(NVNcommandBuffer *c,const NVNprogram *program,int stages) {
    nvn_cmd_program *p=nvn_cmd_alloc(c,NVN_CMD_BIND_PROGRAM,sizeof *p);
    snapshot(&p->resource,program,10); p->stages=(uint32_t)stages;
}
static void pool(NVNcommandBuffer *c,int kind,const void *object) {
    nvn_cmd_pool p={(uintptr_t)object,kind,0};
    memcpy(nvn_cmd_alloc(c,NVN_CMD_SET_POOL,sizeof p),&p,sizeof p);
}
void nvn_CommandBufferSetTexturePool(NVNcommandBuffer *c,const NVNtexturePool *p) { pool(c,0,p); }
void nvn_CommandBufferSetSamplerPool(NVNcommandBuffer *c,const NVNsamplerPool *p) { pool(c,1,p); }
static void handles(NVNcommandBuffer *c,int kind,int stage,int first,int count,const uint64_t *h) {
    if (stage<0 || stage>=6 || first<0 || first>128 || count<0 || count>128-first || (count&&!h))
        NVN_DIE("invalid texture/sampler bindings");
    nvn_cmd_handles *p=nvn_cmd_alloc(c,NVN_CMD_BIND_HANDLES,sizeof *p+(size_t)count*8);
    *p=(nvn_cmd_handles){kind,stage,first,count};
    if (count) memcpy(p+1,h,(size_t)count*8);
}
void nvn_CommandBufferBindTextures(NVNcommandBuffer *c,NVNshaderStage stage,int first,int count,const NVNtextureHandle *h) {
    handles(c,NVN_BIND_TEXTURE,stage,first,count,h);
}
void nvn_CommandBufferBindTexture(NVNcommandBuffer *c,NVNshaderStage stage,int index,NVNtextureHandle h) {
    handles(c,NVN_BIND_TEXTURE,stage,index,1,&h);
}
void nvn_CommandBufferSetBlendColor(NVNcommandBuffer *c,const float *v) {
    if (!v) NVN_DIE("null blend color");
    memcpy(nvn_cmd_alloc(c,NVN_CMD_BLEND_COLOR,16),v,16);
}
void nvn_CommandBufferSetShaderScratchMemory(NVNcommandBuffer *c,const NVNmemoryPool *memory,ptrdiff_t offset,size_t size) {
    nvn_st_ent *s=nvn_st_get(memory,4);
    if (size && (!s || offset<0 || (uint64_t)offset>s->v[2] || size>s->v[2]-(uint64_t)offset))
        NVN_DIE("invalid shader scratch memory");
    nvn_cmd_scratch p={(uintptr_t)memory,(uint64_t)offset,size};
    memcpy(nvn_cmd_alloc(c,NVN_CMD_SCRATCH,sizeof p),&p,sizeof p);
}
void nvn_CommandBufferReportCounter(NVNcommandBuffer *c,NVNcounterType type,NVNbufferAddress address) {
    nvn_cmd_counter p={(uint32_t)type,address};
    memcpy(nvn_cmd_alloc(c,NVN_CMD_COUNTER,sizeof p),&p,sizeof p);
}
void nvn_CommandBufferDrawArraysInstanced(NVNcommandBuffer *c,NVNdrawPrimitive primitive,int first,int count,int base_instance,int instances) {
    if (first<0 || count<0 || base_instance<0 || instances<0) NVN_DIE("invalid DrawArrays");
    nvn_cmd_draw p={primitive,first,count,base_instance,instances,0};
    memcpy(nvn_cmd_alloc(c,NVN_CMD_DRAW,sizeof p),&p,sizeof p);
}
void nvn_CommandBufferDrawArrays(NVNcommandBuffer *c,NVNdrawPrimitive primitive,int first,int count) {
    nvn_CommandBufferDrawArraysInstanced(c,primitive,first,count,0,1);
}
void nvn_CommandBufferDrawElementsInstanced(NVNcommandBuffer *c,NVNdrawPrimitive primitive,NVNindexType type,int count,NVNbufferAddress address,
        int base_vertex,int base_instance,int instances) {
    if (count<0 || base_instance<0 || instances<0) NVN_DIE("invalid DrawElements");
    nvn_cmd_draw_indexed p={primitive,type,count,base_vertex,base_instance,instances,address};
    memcpy(nvn_cmd_alloc(c,NVN_CMD_DRAW_INDEXED,sizeof p),&p,sizeof p);
}
void nvn_CommandBufferDrawElementsBaseVertex(NVNcommandBuffer *c,NVNdrawPrimitive primitive,NVNindexType type,int count,NVNbufferAddress address,int base_vertex) {
    nvn_CommandBufferDrawElementsInstanced(c,primitive,type,count,address,base_vertex,0,1);
}
void nvn_CommandBufferDrawElements(NVNcommandBuffer *c,NVNdrawPrimitive primitive,NVNindexType type,int count,NVNbufferAddress address) {
    nvn_CommandBufferDrawElementsInstanced(c,primitive,type,count,address,0,0,1);
}
