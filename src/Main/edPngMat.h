#ifndef ED_PNG_MAT_H
#define ED_PNG_MAT_H
#include "edResources.h"
#include <png.h>

struct EdPngInput {const unsigned char *p;size_t left;};
static void edPngRead(png_structp png,png_bytep out,png_size_t n){
    auto *in=(EdPngInput*)png_get_io_ptr(png);
    if(n>in->left)png_error(png,"Truncated PNG");
    memcpy(out,in->p,n);in->p+=n;in->left-=n;
}
static EdResources::Bytes edPngToMat(const EdResources::Bytes &source){
    png_structp png=png_create_read_struct(PNG_LIBPNG_VER_STRING,nullptr,nullptr,nullptr);
    if(!png)throw std::runtime_error("Cannot allocate PNG decoder");
    png_infop info=png_create_info_struct(png);
    if(!info){png_destroy_read_struct(&png,nullptr,nullptr);throw std::runtime_error("Cannot allocate PNG metadata");}
    EdPngInput input{source.data(),source.size()};
    if(setjmp(png_jmpbuf(png))){png_destroy_read_struct(&png,&info,nullptr);throw std::runtime_error("Invalid PNG material (maximum dimensions 4096 x 4096)");}
    png_set_user_limits(png,4096,4096);
    png_set_read_fn(png,&input,edPngRead);
    // Keep 16-bit alpha intact: even alpha=1 must remain opaque.
    png_read_png(png,info,PNG_TRANSFORM_EXPAND|PNG_TRANSFORM_GRAY_TO_RGB,nullptr);
    const unsigned w=png_get_image_width(png,info),h=png_get_image_height(png,info);
    const unsigned bytes=png_get_bit_depth(png,info)/8,channels=png_get_channels(png,info);
    if(!w||!h||(channels!=3&&channels!=4)||(bytes!=1&&bytes!=2)||size_t(w)*h>(EdResources::MaxFile-140)/2){
        png_destroy_read_struct(&png,&info,nullptr);throw std::runtime_error("PNG material exceeds MAT size limit");
    }
    try {
        auto rows=png_get_rows(png,info);
        bool alpha=false;
        for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){
            const auto *p=rows[y]+size_t(x)*channels*bytes;
            if(channels==4&&!p[3*bytes]&&(bytes==1||!p[3*bytes+1]))alpha=true;
        }
        EdResources::Bytes mat={'M','A','T',' '};
        for(uint32_t v:{50u,2u,1u,1u, 1u,16u,5u,6u,5u,11u,5u,0u,3u,2u,3u,0u,0u,0u})EdResources::put(mat,v);
        // One textured cel, one RGB565 texture, one full-resolution mip.
        for(uint32_t v:{8u,0u,0u,0u,0u,0u, 0u,0u,0u,0u})EdResources::put(mat,v);
        constexpr unsigned key=0xf81f;
        for(uint32_t v:{w,h,alpha?1u:0u,0u,alpha?key:0u,1u})EdResources::put(mat,v);
        mat.reserve(140+size_t(w)*h*2);
        for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){
            const auto *p=rows[y]+size_t(x)*channels*bytes;
            bool clear=channels==4&&!p[3*bytes]&&(bytes==1||!p[3*bytes+1]);
            unsigned pixel=((p[0]>>3)<<11)|((p[bytes]>>2)<<5)|(p[2*bytes]>>3);
            // Reserve the key only for transparent pixels, preserving opaque magenta.
            if(clear)pixel=key;else if(alpha&&pixel==key)pixel^=1;
            mat.push_back((unsigned char)pixel);mat.push_back((unsigned char)(pixel>>8));
        }
        png_destroy_read_struct(&png,&info,nullptr);return mat;
    }catch(...){png_destroy_read_struct(&png,&info,nullptr);throw;}
}
#endif
