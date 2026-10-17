/* CPU texel transfer uses guest block storage; a bounded region flush updates
 * existing Metal textures without overwriting unrelated GPU-rendered pixels. */
#include "nvn_texture_transfer.h"
#include "nvn_state.h"
#include "nvn_host.h"
#include "nvn_format_info.inc"
#include <pthread.h>
#include <limits.h>

_Static_assert(sizeof(NVNcopyRegion)==24,"NVNcopyRegion ABI");
static pthread_mutex_t handler_lock=PTHREAD_MUTEX_INITIALIZER;
static nvn_texture_flush_handler flush_handler;
void nvn_texture_set_flush_handler(nvn_texture_flush_handler handler) {
    pthread_mutex_lock(&handler_lock);flush_handler=handler;pthread_mutex_unlock(&handler_lock);
}
int nvn_texture_transfer_prepare(const nvn_resource_snapshot *s,const nvn_view_state *v,
        const NVNcopyRegion *r,nvn_texture_transfer *out) {
    if(!s||!r||!out)return 0;
    const uint64_t *w=s->words;texture_fmt f=texture_format((int)w[5]);
    if(!f.bpb||!w[1]||!w[2]||!w[3]||!w[4]||w[1]>16384||w[2]>16384||w[3]>2048||w[4]>15||w[8]>1)return 0;
    nvn_texture_transfer t={0};t.resource=*s;t.region=*r;
    nvn_tex_layout *l=&t.layout;
    l->width=(uint32_t)w[1];l->height=(uint32_t)w[2];l->depth=1;
    l->layers=1;l->dims=2;l->mip_levels=(uint32_t)w[4];l->samples_x=l->samples_y=1;
    l->block_w=f.bw;l->block_h=f.bh;l->bytes_per_block=f.bpb;
    switch(w[7]) {
    case 0:l->dims=1;l->height=1;break;
    case 1:case 7:break;
    case 2:l->dims=3;l->depth=(uint32_t)w[3];break;
    case 3:l->dims=1;l->layers=l->height;l->height=1;break;
    case 4:l->layers=(uint32_t)w[3];break;
    case 8:l->layers=6;break;
    case 9:l->layers=(uint32_t)w[3]*6;break;
    default:return 0;
    }
    l->linear=!!(w[6]&0x10);l->stride=(uint32_t)w[11];
    if(l->linear) {
        if(l->mip_levels!=1||l->dims==3)return 0;
        uint32_t row=tex_div_ceil(l->width,f.bw)*f.bpb;
        if(!l->stride){uint32_t a=w[6]&0x40?128:32;l->stride=(row+a-1)&~(a-1);}
        if(l->stride<row)return 0;
        t.layer_stride=(uint64_t)l->stride*tex_div_ceil(l->height,f.bh);
    } else {
        if(l->dims==2)l->tile_h_log2=nvn_tex_default_tile_h(l->height,1,f.bh);
        if(l->dims==3)l->tile_d_log2=tex_pick_tile_size(l->depth+l->depth/2);
        if(!nvn_tex_package_tile_h(w,&l->tile_h_log2))return 0;
        t.layer_stride=nvn_tex_level_offset(l,l->mip_levels);
        if(l->layers>1)t.layer_stride=tex_align_layer_size(t.layer_stride,l->height,1,f.bh,l->tile_h_log2,0);
    }
    uint32_t base_layer=0,layer_count=l->layers;
    if(v) {
        if(v->flags&~63u)return 0;
        if(v->flags&NVN_VIEW_LEVELS){
            if(v->base_level<0||v->level_count<=0||(uint32_t)v->base_level>=l->mip_levels||
               (uint32_t)v->level_count>l->mip_levels-(uint32_t)v->base_level)return 0;
            t.level=(uint32_t)v->base_level;
        }
        if(v->flags&NVN_VIEW_LAYERS){
            if(l->dims==3)return 0;
            if(v->base_layer<0||v->layer_count<=0||(uint32_t)v->base_layer>=l->layers||
               (uint32_t)v->layer_count>l->layers-(uint32_t)v->base_layer)return 0;
            base_layer=(uint32_t)v->base_layer;layer_count=(uint32_t)v->layer_count;
        }
        if(v->flags&NVN_VIEW_FORMAT){texture_fmt vf=texture_format(v->format);
            if(vf.bpb!=f.bpb||vf.bw!=f.bw||vf.bh!=f.bh)return 0;}
        if((v->flags&NVN_VIEW_TARGET)&&(uint64_t)v->target!=w[7])return 0;
    }
    if(l->dims==3)layer_count=tex_mip_size(l->depth,t.level);
    t.width=tex_mip_size(l->width,t.level);t.height=tex_mip_size(l->height,t.level);
    if(r->xoffset<0||r->yoffset<0||r->zoffset<0||r->width<=0||r->height<=0||r->depth<=0||
       (uint32_t)r->xoffset>=t.width||(uint32_t)r->width>t.width-(uint32_t)r->xoffset||
       (uint32_t)r->yoffset>=t.height||(uint32_t)r->height>t.height-(uint32_t)r->yoffset||
       (uint32_t)r->zoffset>=layer_count||(uint32_t)r->depth>layer_count-(uint32_t)r->zoffset)return 0;
    if(r->xoffset%f.bw||r->yoffset%f.bh||
       (r->width%f.bw&&(uint32_t)(r->xoffset+r->width)!=t.width)||
       (r->height%f.bh&&(uint32_t)(r->yoffset+r->height)!=t.height))return 0;
    t.region.zoffset+=(int)base_layer;
    t.mip_offset=l->linear?0:nvn_tex_level_offset(l,t.level);
    t.storage_size=t.layer_stride*l->layers;
    nvn_st_ent *pool=nvn_st_get((void *)(uintptr_t)w[9],4);
    if(!pool||!pool->v[1]||w[10]>pool->v[2]||t.storage_size>pool->v[2]-w[10]||
       pool->v[1]>UINTPTR_MAX-pool->v[2])return 0;
    t.storage=(uint8_t *)(uintptr_t)(pool->v[1]+w[10]);
    t.pool_flags=(uint32_t)pool->v[3];
    *out=t;return 1;
}
static uint64_t byte_offset(const nvn_texture_transfer *t,uint32_t xbyte,uint32_t y,uint32_t layer) {
    const nvn_tex_layout *l=&t->layout;
    uint64_t offset=(uint64_t)layer*t->layer_stride+t->mip_offset;
    if(l->linear)return offset+(uint64_t)y*l->stride+xbyte;
    uint32_t row=tex_div_ceil(t->width,l->block_w)*l->bytes_per_block;
    uint8_t h=tex_adjust_tile_size(l->tile_h_log2,8,tex_div_ceil(t->height,l->block_h));
    if(l->dims==3){
        uint8_t d=tex_adjust_tile_size(l->tile_d_log2,1,tex_mip_size(l->depth,t->level));
        uint64_t tile=UINT64_C(512)<<(h+d);
        uint32_t columns=tex_div_ceil(row,64),tile_rows=tex_div_ceil(tex_div_ceil(t->height,l->block_h),8u<<h);
        return t->mip_offset+((uint64_t)(layer>>d)*tile_rows+y/(8u<<h))*columns*tile+
            (uint64_t)(xbyte/64)*tile+((uint64_t)(layer&((1u<<d)-1))<<(9+h))+
            (uint64_t)((y%(8u<<h))/8)*512+nvn_tex_gob_offset(xbyte,y);
    }
    return offset+nvn_tex_block_offset(xbyte,y,row,1,h);
}
static void flush_checked(const nvn_texture_transfer *t) {
    pthread_mutex_lock(&handler_lock);nvn_texture_flush_handler handler=flush_handler;pthread_mutex_unlock(&handler_lock);
    if(handler&&!handler(t))NVN_DIE("NVN_TEXTURE_TRANSFER_UNSUPPORTED operation=upload");
}
void nvn_TextureWriteTexelsStrided(const NVNtexture *texture,const NVNtextureView *view,
        const NVNcopyRegion *r,const void *data,ptrdiff_t row_stride,ptrdiff_t image_stride) {
    nvn_resource_snapshot s={0};nvn_view_state v;nvn_texture_transfer t;
    nvn_resources_view(view,&v);
    if(!data||row_stride<0||image_stride<0||!nvn_resources_snapshot(texture,12,&s)||
       !nvn_texture_transfer_prepare(&s,&v,r,&t)){
        NVN_LOG("NVN_TEXTURE_TRANSFER_STATE format=%llu target=%llu extent=%llux%llux%llu levels=%llu samples=%llu flags=%llu view=%u region=%d,%d,%d,%d,%d,%d",
            (unsigned long long)s.words[5],(unsigned long long)s.words[7],(unsigned long long)s.words[1],
            (unsigned long long)s.words[2],(unsigned long long)s.words[3],(unsigned long long)s.words[4],
            (unsigned long long)s.words[8],(unsigned long long)s.words[6],v.flags,
            r?r->xoffset:0,r?r->yoffset:0,r?r->zoffset:0,r?r->width:0,r?r->height:0,r?r->depth:0);
        NVN_DIE("NVN_TEXTURE_TRANSFER_UNSUPPORTED operation=write");
    }
    uint32_t bpb=t.layout.bytes_per_block,bw=t.layout.block_w,bh=t.layout.block_h;
    uint32_t row=tex_div_ceil((uint32_t)r->width,bw)*bpb,rows=tex_div_ceil((uint32_t)r->height,bh);
    uint64_t pitch=row_stride?(uint64_t)row_stride:row;
    if(pitch<row||pitch>PTRDIFF_MAX/rows)NVN_DIE("invalid texture row stride");
    uint64_t image=image_stride?(uint64_t)image_stride:pitch*rows;
    if(image<pitch*(rows-1)+row||image>PTRDIFF_MAX/(uint32_t)r->depth||
       (uintptr_t)data>UINTPTR_MAX-image*(uint32_t)r->depth)NVN_DIE("invalid texture image stride");
    /* Validate every destination before modifying any byte. Byte addressing
     * also handles packed three-component texels crossing a GOB boundary. */
    for(unsigned pass=0;pass<2;pass++)for(int z=0;z<r->depth;z++)for(uint32_t y=0;y<rows;y++)for(uint32_t x=0;x<row;x++){
        uint64_t offset=byte_offset(&t,(uint32_t)r->xoffset/bw*bpb+x,(uint32_t)r->yoffset/bh+y,(uint32_t)(t.region.zoffset+z));
        if(offset>=t.storage_size)NVN_DIE("texture transfer exceeds storage");
        if(pass)t.storage[offset]=((const uint8_t *)data)[(uint64_t)z*image+(uint64_t)y*pitch+x];
    }
    if(t.pool_flags&NVN_MEMORY_POOL_FLAGS_CPU_UNCACHED)flush_checked(&t);
}
void nvn_TextureWriteTexels(const NVNtexture *texture,const NVNtextureView *view,const NVNcopyRegion *r,const void *data) {
    nvn_TextureWriteTexelsStrided(texture,view,r,data,0,0);
}
void nvn_TextureFlushTexels(const NVNtexture *texture,const NVNtextureView *view,const NVNcopyRegion *r) {
    nvn_resource_snapshot s;nvn_view_state v;nvn_texture_transfer t;
    nvn_resources_view(view,&v);
    if(!nvn_resources_snapshot(texture,12,&s)||!nvn_texture_transfer_prepare(&s,&v,r,&t))
        NVN_DIE("NVN_TEXTURE_TRANSFER_UNSUPPORTED operation=flush");
    flush_checked(&t);
}
