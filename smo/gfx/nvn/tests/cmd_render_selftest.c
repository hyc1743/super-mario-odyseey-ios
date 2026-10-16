#include "../nvn_host.h"
#include "../nvn_cmd_render.h"
#include "../nvn_render_state.h"
#include "../nvn_state.h"
#include <assert.h>
#define P(x) ((uint64_t)(uintptr_t)(x))
static uint64_t call(const char *suffix,void *obj,uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e) {
    char name[128]; snprintf(name,sizeof name,"nvnCommandBuffer%s",suffix);
    nvn_fn fn=nvn_lookup(name); assert(fn && fn!=nvn_unimplemented && !nvn_is_missing_command(fn));
    return fn(P(obj),a,b,c,d,e,0,0);
}
static int inspect(void *context,uint32_t op,const void *data,size_t size) {
    unsigned *count=context; ++*count;
    switch (op) {
    case NVN_CMD_SET_RT: {
        assert(size==sizeof(nvn_cmd_targets)); const nvn_cmd_targets *p=data;
        assert(p->count==1 && p->colors[0].resource.words[1]==128);
        assert(p->colors[0].resource.words[15]==42);
        assert(p->colors[0].has_view && p->colors[0].view.base_level==2);
        assert(!p->depth.resource.object);
        break;
    }
    case NVN_CMD_BIND_PROGRAM: {
        const nvn_cmd_program *p=data; assert(size==sizeof *p);
        assert(p->resource.words[2]==0x12345678 && p->stages==3); break;
    }
    case NVN_CMD_BIND_STATE: {
        const nvn_cmd_state *p=data; assert(size==sizeof *p);
        assert(p->kind==NVN_STATE_BLEND && p->size==8);
        assert(nvn_blend_snapshot(p->bytes).target==3); break;
    }
    case NVN_CMD_VIEWPORTS: {
        const nvn_cmd_array *p=data; float v[4]; memcpy(v,p+1,sizeof v);
        assert(p->first==0 && p->count==1 && v[2]==128 && v[3]==64); break;
    }
    case NVN_CMD_DEPTH_RANGES: {
        const nvn_cmd_array *p=data; float v[2]; memcpy(v,p+1,sizeof v);
        assert(p->first==0 && p->count==1 && v[0]==0.25f && v[1]==0.75f); break;
    }
    case NVN_CMD_BIND_VATTR: {
        const nvn_cmd_array *p=data; assert(p->count==1);
        assert(nvn_vertex_attrib_snapshot(p+1).offset==16); break;
    }
    case NVN_CMD_BIND_BUFFER: {
        const nvn_cmd_buffers *p=data; NVNbufferRange r; memcpy(&r,p+1,sizeof r);
        assert(p->kind==NVN_BIND_UNIFORM && p->stage==1 && p->first==4 && r.address==0x4000 && r.size==64); break;
    }
    case NVN_CMD_BIND_HANDLES: {
        const nvn_cmd_handles *p=data; uint64_t h; memcpy(&h,p+1,8);
        assert(p->stage==1 && p->first==2 && p->count==1 && h==0x12345); break;
    }
    case NVN_CMD_DRAW_INDEXED: {
        const nvn_cmd_draw_indexed *p=data;
        assert(p->count==6 && p->address==0x5000 && p->base_vertex==0 && p->instances==1); break;
    }
    default: assert(!"unexpected render record");
    }
    return 1;
}
int main(void) {
    NVNcommandBuffer cb; NVNmemoryPool pool; NVNtexture texture; NVNprogram program;
    _Alignas(8) uint8_t memory[8192];
    nvn_st_ent *s=nvn_st_new(&pool,4); assert(s); s->v[1]=P(memory); s->v[2]=sizeof memory;
    nvn_st_ent *t=nvn_st_new(&texture,12); assert(t); t->v[1]=128; t->v[15]=42;
    nvn_st_ent *p=nvn_st_new(&program,10); assert(p); p->v[2]=0x12345678;
    nvn_view_state view={0}; view.flags=NVN_VIEW_LEVELS; view.base_level=2; view.level_count=1;
    const NVNtexture *targets[1]={&texture}; const NVNtextureView *views[1]={(void *)&view};
    NVNblendState blend; nvn_BlendStateSetDefaults(&blend); nvn_BlendStateSetBlendTarget(&blend,3);
    NVNvertexAttribState attribute; nvn_VertexAttribStateSetDefaults(&attribute);
    nvn_VertexAttribStateSetFormat(&attribute,NVN_FORMAT_RG32F,16);
    NVNbufferRange uniform={0x4000,64}; uint64_t handle=0x12345;
    assert(call("Initialize",&cb,0,0,0,0,0));
    call("AddCommandMemory",&cb,P(&pool),0,sizeof memory,0,0);
    call("BeginRecording",&cb,0,0,0,0,0);
    call("SetRenderTargets",&cb,1,P(targets),P(views),0,0);
    call("BindProgram",&cb,P(&program),3,0,0,0);
    call("BindBlendState",&cb,P(&blend),0,0,0,0);
    call("SetViewport",&cb,0,0,128,64,0);
    ((PFNNVNCOMMANDBUFFERSETDEPTHRANGEPROC)(uintptr_t)nvn_lookup("nvnCommandBufferSetDepthRange"))(&cb,0.25f,0.75f);
    call("BindVertexAttribState",&cb,1,P(&attribute),0,0,0);
    call("BindUniformBuffers",&cb,1,4,1,P(&uniform),0);
    call("BindTextures",&cb,1,2,1,P(&handle),0);
    call("DrawElements",&cb,NVN_DRAW_PRIMITIVE_TRIANGLES,NVN_INDEX_TYPE_UNSIGNED_SHORT,6,0x5000,0);
    uint64_t commands=call("EndRecording",&cb,0,0,0,0,0);
    memset(&view,0,sizeof view); memset(&blend,0,sizeof blend); memset(&attribute,0,sizeof attribute);
    memset(&uniform,0,sizeof uniform); handle=0; targets[0]=NULL; views[0]=NULL;
    nvn_st_del(&texture,12); nvn_st_del(&program,10);
    unsigned count=0; assert(nvn_cmd_walk(commands,inspect,&count) && count==9);
    call("Finalize",&cb,0,0,0,0,0); nvn_st_reset();
    puts("render command snapshots: resources/views/state/arrays/float ABI/draw defaults passed");
    return 0;
}
