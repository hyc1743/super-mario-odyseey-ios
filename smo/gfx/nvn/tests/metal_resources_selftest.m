/* Real Metal allocation, upload, texture alias, sampler and GPU-write checks. */
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include "../nvn_metal_resources.h"
#include "../nvn_state.h"
#include "../nvn_host.h"
#include "../nvn_tex.h"
#include <assert.h>

int main(void) {
    @autoreleasepool {
        id<MTLDevice> device=MTLCreateSystemDefaultDevice(); assert(device);
        uint8_t *memory=calloc(1,4096); assert(memory);
        uint64_t pool[8]={0},object[8]={0},builder[8]={0},sampler[8]={0};
        nvn_st_ent *s=nvn_st_new(pool,4 /* memory pool */); assert(s);
        s->v[1]=(uintptr_t)memory;s->v[2]=4096;
        for(int y=0;y<4;++y)for(int x=0;x<4;++x){
            uint8_t *p=memory+y*32+x*4;p[0]=17;p[1]=34;p[2]=51;p[3]=255;
        }
        nvn_resource_snapshot texture={.object=(uintptr_t)object,
            .words={0,4,4,1,1,0x25,0x10,1,1,(uintptr_t)pool,0,32,0,0,0,1}};
        id<MTLTexture> tex=nvn_metal_texture(device,&texture,NULL,0);assert(tex);
        uint8_t pixels[64]={0};
        [tex getBytes:pixels bytesPerRow:16 fromRegion:MTLRegionMake2D(0,0,4,4) mipmapLevel:0];
        for(int i=0;i<16;++i)assert(pixels[i*4]==17 && pixels[i*4+2]==51 && pixels[i*4+3]==255);
        assert(nvn_metal_texture(device,&texture,NULL,1)==tex);
        id<MTLCommandQueue> q=[device newCommandQueue];id<MTLCommandBuffer> cb=[q commandBuffer];
        MTLRenderPassDescriptor *pass=[MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture=tex;pass.colorAttachments[0].loadAction=MTLLoadActionClear;
        pass.colorAttachments[0].storeAction=MTLStoreActionStore;
        pass.colorAttachments[0].clearColor=MTLClearColorMake(1,0,0,1);
        id<MTLRenderCommandEncoder> encoder=[cb renderCommandEncoderWithDescriptor:pass];assert(encoder);
        [encoder endEncoding];[cb commit];[cb waitUntilCompleted];assert(cb.status==MTLCommandBufferStatusCompleted);
        tex=nvn_metal_texture(device,&texture,NULL,0);
        [tex getBytes:pixels bytesPerRow:16 fromRegion:MTLRegionMake2D(0,0,4,4) mipmapLevel:0];
        assert(pixels[0]==255 && pixels[1]==0 && pixels[2]==0 && pixels[3]==255);
        nvn_fn defaults=nvn_lookup("nvnSamplerBuilderSetDefaults");
        defaults((uintptr_t)builder,0,0,0,0,0,0,0);
        assert(nvn_lookup("nvnSamplerInitialize")((uintptr_t)sampler,(uintptr_t)builder,0,0,0,0,0,0));
        nvn_resource_snapshot sampler_state;assert(nvn_resources_snapshot(sampler,8 /* sampler */,&sampler_state));
        assert(nvn_metal_sampler(device,&sampler_state));
        nvn_view_state view={.flags=NVN_VIEW_FORMAT,.format=0x38};
        id<MTLTexture> srgb=nvn_metal_texture(device,&texture,&view,0);assert(srgb);
        assert(srgb.pixelFormat==MTLPixelFormatRGBA8Unorm_sRGB);
        /* A packaged 16x65 image uses one GOB in height, whereas the automatic
         * layout chooses eight. Check uploaded pixels, not only metadata. */
        uint8_t *packed_memory=calloc(1,8192);assert(packed_memory);
        uint64_t packed_pool[8]={0},packed_object[8]={0};
        nvn_st_ent *pp=nvn_st_new(packed_pool,4);assert(pp);
        pp->v[1]=(uintptr_t)packed_memory;pp->v[2]=8192;
        for(unsigned y=0;y<65;y++)for(unsigned x=0;x<16;x++){
            uint64_t offset=nvn_tex_block_offset(x,y,16,4,0);
            packed_memory[offset]=(uint8_t)y;packed_memory[offset+1]=(uint8_t)x;
            packed_memory[offset+2]=97;packed_memory[offset+3]=255;
        }
        nvn_resource_snapshot packaged={.object=(uintptr_t)packed_object,
            .words={0,16,65,1,1,0x25,0,1,1,(uintptr_t)packed_pool,0,0,
                NVN_TEX_HAS_PACKAGE|NVN_TEX_HAS_SWIZZLE|UINT64_C(0x01020304),0,0,2}};
        id<MTLTexture> packed_tex=nvn_metal_texture(device,&packaged,NULL,1);assert(packed_tex);
        uint8_t packed_pixels[16*65*4];
        [packed_tex getBytes:packed_pixels bytesPerRow:64 fromRegion:MTLRegionMake2D(0,0,16,65) mipmapLevel:0];
        for(unsigned y=0;y<65;y++)for(unsigned x=0;x<16;x++){
            unsigned i=(y*16+x)*4;
            assert(packed_pixels[i]==y&&packed_pixels[i+1]==x&&packed_pixels[i+2]==97&&packed_pixels[i+3]==255);
        }
        id<MTLTexture> swizzled=nvn_metal_texture(device,&packaged,NULL,0);assert(swizzled);
        assert(swizzled.swizzle.red==MTLTextureSwizzleBlue&&swizzled.swizzle.alpha==MTLTextureSwizzleOne);
        nvn_view_state identity_view={.flags=NVN_VIEW_SWIZZLE,.swizzle={2,3,4,5}};
        id<MTLTexture> identity=nvn_metal_texture(device,&packaged,&identity_view,0);assert(identity);
        assert(identity.swizzle.red==MTLTextureSwizzleRed&&identity.swizzle.alpha==MTLTextureSwizzleAlpha);
        texture.words[15]++;assert(nvn_metal_texture(device,&texture,NULL,0)!=tex);
        texture.words[5]=0;assert(!nvn_metal_texture(device,&texture,NULL,0));
        nvn_metal_resources_reset();nvn_st_reset();free(memory);free(packed_memory);
        puts("METAL_RESOURCES_PASS: upload, shared render/sample identity, GPU writes, views, serial reuse and default sampler");
    }
    return 0;
}
