#include "../src/Main/edPngMat.h"
#include <iostream>
#include <chrono>
static void check(bool value){if(!value)throw std::runtime_error("PNG material test failed");}
static void writeBytes(png_structp p,png_bytep b,png_size_t n){auto &out=*(EdResources::Bytes*)png_get_io_ptr(p);out.insert(out.end(),b,b+n);}
static EdResources::Bytes makePng(int depth,int type,const EdResources::Bytes &pixels){
    EdResources::Bytes out;
    auto p=png_create_write_struct(PNG_LIBPNG_VER_STRING,nullptr,nullptr,nullptr);auto info=png_create_info_struct(p);
    if(setjmp(png_jmpbuf(p)))throw std::runtime_error("PNG fixture failure");
    png_set_write_fn(p,&out,writeBytes,nullptr);png_set_IHDR(p,info,3,1,depth,type,PNG_INTERLACE_NONE,PNG_COMPRESSION_TYPE_DEFAULT,PNG_FILTER_TYPE_DEFAULT);
    png_color colors[3]={{255,0,0},{255,0,255},{0,255,0}};png_byte alpha[3]={0,1,255};
    if(type==PNG_COLOR_TYPE_PALETTE){png_set_PLTE(p,info,colors,3);png_set_tRNS(p,info,alpha,3,nullptr);}
    png_write_info(p,info);png_write_row(p,(png_bytep)pixels.data());png_write_end(p,info);png_destroy_write_struct(&p,&info);return out;
}
static unsigned pixel(const EdResources::Bytes &m,int i){return m.at(140+2*i)|unsigned(m.at(141+2*i))<<8;}
int main(){
    auto png=makePng(8,PNG_COLOR_TYPE_RGBA,{255,0,0,0, 255,0,255,1, 0,255,0,255});
    auto mat=edPngToMat(png);check(mat.size()==146&&EdResources::get(mat.data()+24)==16&&EdResources::get(mat.data()+124)==1);
    check(pixel(mat,0)==0xf81f&&pixel(mat,1)==0xf81e&&pixel(mat,2)==0x07e0);
    check(edPngToMat(makePng(8,PNG_COLOR_TYPE_PALETTE,{0,1,2}))==mat);
    auto rgb=edPngToMat(makePng(8,PNG_COLOR_TYPE_RGB,{255,0,255, 255,255,255, 0,0,0}));
    check(EdResources::get(rgb.data()+124)==0&&pixel(rgb,0)==0xf81f&&pixel(rgb,1)==0xffff&&pixel(rgb,2)==0);
    auto gray=edPngToMat(makePng(8,PNG_COLOR_TYPE_GRAY_ALPHA,{255,0, 0,1, 255,255}));
    check(pixel(gray,0)==0xf81f&&pixel(gray,1)==0&&pixel(gray,2)==0xffff);
    auto deep=edPngToMat(makePng(16,PNG_COLOR_TYPE_RGBA,{255,255,0,0,0,0,0,0, 255,255,0,0,0,0,0,1, 0,0,255,255,0,0,255,255}));
    check(pixel(deep,0)==0xf81f&&pixel(deep,1)==0xf800&&pixel(deep,2)==0x07e0);
    bool rejected=false;try{edPngToMat({1,2,3});}catch(const std::exception&){rejected=true;}check(rejected);
    auto root=std::filesystem::temp_directory_path()/("geosith-png-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root/"Mod/mat");auto file=root/"Mod/mat/Test.PNG";
    {std::ofstream f(file,std::ios::binary);f.write((char*)png.data(),png.size());}
    EdResources resources;resources.pngToMat=edPngToMat;resources.discover(root);
    check(resources.files.size()==1&&*resources.files.at("mat/test.mat")==mat);
    EdResources remote;auto wire=resources.encode();check(EdResources::decode(wire.data(),wire.size(),remote)&&*remote.files.at("mat/test.mat")==mat);
    {std::ofstream f(root/"Mod/mat/test.mat",std::ios::binary);f<<"explicit MAT";}
    resources.discover(root);check(resources.files.at("mat/test.mat")->size()==12);
    std::filesystem::remove(file);std::filesystem::remove(root/"Mod/mat/test.mat");std::filesystem::remove(root/"Mod/mat");std::filesystem::remove(root/"Mod");std::filesystem::remove(root);
    std::cout<<"PASS: RGB/gray/RGBA PNG, exact 8/16-bit alpha, opaque key collision, folder import, MAT precedence and network roundtrip\n";
}
