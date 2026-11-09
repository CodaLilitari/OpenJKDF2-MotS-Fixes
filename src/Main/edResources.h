#ifndef ED_RESOURCES_H
#define ED_RESOURCES_H
#include <map>
#include <memory>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <stdexcept>
#include <cstdio>
#include <cctype>

/* Only game data is mounted. Network paths never become filesystem paths. */
class EdResources {
public:
    using Bytes=std::vector<unsigned char>;
    using Files=std::map<std::string,std::shared_ptr<const Bytes>>;
    static constexpr size_t MaxFile=32*1024*1024,MaxBytes=128*1024*1024,MaxFiles=8192;
    Files files;
    Bytes (*pngToMat)(const Bytes &)=nullptr;
    static std::string path(std::string name) {
        if(name.empty()||name.size()>127)return {};
        for(char &c:name){if(c=='\\')c='/';if(c>='A'&&c<='Z')c+=32;}
        if(name.front()=='/'||name.back()=='/')return {};
        size_t at=0;while(at<name.size()) {size_t end=name.find('/',at);if(end==std::string::npos)end=name.size();std::string part=name.substr(at,end-at);
            if(part.empty()||part=="."||part==".."||part.back()=='.'||part.back()==' ')return {};
            for(unsigned char c:part)if(c<32||c==127||c==':'||c=='*'||c=='?'||c=='"'||c=='<'||c=='>'||c=='|')return {};
            at=end+1;
        }
        size_t dot=name.rfind('.');if(dot==std::string::npos)return {};std::string ext=name.substr(dot);
        const char *allowed[]={".mat",".3do",".cog",".spr",".cmp",".wav",".snd",".ai",".key",".pup",".par"};
        bool ok=false;for(auto e:allowed)ok|=ext==e;if(!ok)return {};
        const char *roots[]={"mat/","3do/","cog/","sprite/","spr/","sound/","sfx/","misc/","ai/","key/","pup/"};
        for(auto root:roots)if(name.rfind(root,0)==0)return name;
        return {};
    }
    size_t size()const{size_t n=0;for(auto &f:files)n+=f.second->size();return n;}
    void add(const std::string &name,Bytes data) {
        auto key=path(name);if(key.empty())throw std::runtime_error("Invalid custom resource path");
        if(data.empty()||data.size()>MaxFile)throw std::runtime_error("Custom resource exceeds 32 MiB or is empty");
        auto old=files.find(key);size_t previous=old==files.end()?0:old->second->size();
        if(size()-previous+data.size()>MaxBytes||(old==files.end()&&files.size()>=MaxFiles))throw std::runtime_error("Custom resources exceed 128 MiB / 8192 files");
        files[key]=std::make_shared<const Bytes>(std::move(data));
    }
    static uint32_t get(const unsigned char *p){return p[0]|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
    static void put(Bytes &out,uint32_t n){for(int i=0;i<4;i++)out.push_back((unsigned char)(n>>(8*i)));}
    static Bytes read(const std::filesystem::path &file,uint64_t offset,size_t count){
        if(count>MaxFile)throw std::runtime_error("Custom resource exceeds 32 MiB");
        std::ifstream in(file,std::ios::binary);Bytes b(count);if(!in.seekg(offset)||!in.read((char*)b.data(),count))throw std::runtime_error("Cannot read custom resource");return b;
    }
    void archive(const std::filesystem::path &file) {
        uint64_t size=std::filesystem::file_size(file);if(size<16)throw std::runtime_error("Invalid resource GOB header");
        auto header=read(file,0,12);if(memcmp(header.data(),"GOB ",4))throw std::runtime_error("Invalid resource GOB magic");
        uint64_t index=get(header.data()+8);if(index>size-4)throw std::runtime_error("Invalid resource GOB directory");
        auto countBytes=read(file,index,4);uint32_t count=get(countBytes.data());if(count>100000||uint64_t(count)*136>size-index-4)throw std::runtime_error("Invalid resource GOB entries");
        std::map<std::string,bool> seen;
        for(uint32_t i=0;i<count;i++){auto e=read(file,index+4+uint64_t(i)*136,136);if(!memchr(e.data()+8,0,128))throw std::runtime_error("Invalid resource GOB name");
            auto key=path((const char*)e.data()+8);if(key.empty())continue;
            uint64_t offset=get(e.data()),length=get(e.data()+4);if(offset>size||length>size-offset)throw std::runtime_error("Invalid resource GOB range");
            if(seen[key])throw std::runtime_error("Duplicate resource GOB path");seen[key]=true;add(key,read(file,offset,(size_t)length));
        }
    }
    void directory(const std::filesystem::path &root) {
        std::vector<std::filesystem::path> entries;
        for(auto &e:std::filesystem::recursive_directory_iterator(root)){if(e.is_symlink())throw std::runtime_error("Symlinks are not supported in resource mods");if(e.is_regular_file())entries.push_back(e.path());}
        std::sort(entries.begin(),entries.end());
        // Convert PNGs first; an explicit MAT with the same name wins.
        if(pngToMat)for(auto &f:entries){
            auto name=f.lexically_relative(root).generic_string();
            for(char &c:name)if(c>='A'&&c<='Z')c+=32;
            if(!name.ends_with(".png")||(!name.starts_with("mat/")&&!name.starts_with("3do/mat/")))continue;
            auto key=path(name.substr(0,name.size()-4)+".mat");if(key.empty())continue;
            try{add(key,pngToMat(read(f,0,std::filesystem::file_size(f))));}
            catch(const std::exception &e){throw std::runtime_error(f.filename().string()+": "+e.what());}
        }
        for(auto &f:entries){auto key=path(f.lexically_relative(root).generic_string());if(!key.empty())add(key,read(f,0,std::filesystem::file_size(f)));}
    }
    void discover(const std::filesystem::path &root) {
        if(!std::filesystem::exists(root))return;
        // Accept mods/mat/... as well as mods/MyMod/mat/.... Alphabetically
        // later packages win; a level's embedded bundle takes final priority.
        directory(root);std::vector<std::filesystem::path> entries;
        for(auto &e:std::filesystem::directory_iterator(root)){if(e.is_symlink())throw std::runtime_error("Symlinks are not supported in resource mods");entries.push_back(e.path());}
        std::sort(entries.begin(),entries.end());for(auto &f:entries){if(std::filesystem::is_directory(f))directory(f);else{auto ext=f.extension().string();for(auto &c:ext)c=(char)tolower((unsigned char)c);if(ext==".gob")archive(f);}}
    }
    Bytes encode()const {Bytes b;put(b,0x31525347);put(b,(uint32_t)files.size());for(auto &f:files){put(b,(uint32_t)f.first.size());put(b,(uint32_t)f.second->size());b.insert(b.end(),f.first.begin(),f.first.end());b.insert(b.end(),f.second->begin(),f.second->end());}return b;}
    static bool decode(const unsigned char *p,size_t n,EdResources &out)try {
        if(n<8||n>MaxBytes+MaxFiles*135||get(p)!=0x31525347)return false;uint32_t count=get(p+4);if(count>MaxFiles)return false;p+=8;n-=8;EdResources draft;
        for(uint32_t i=0;i<count;i++){if(n<8)return false;uint32_t a=get(p),b=get(p+4);p+=8;n-=8;if(!a||a>127||b>MaxFile||a>n||b>n-a)return false;
            std::string name((const char*)p,a);if(path(name)!=name||draft.files.count(name))return false;p+=a;n-=a;draft.add(name,Bytes(p,p+b));p+=b;n-=b;}
        if(n)return false;out=std::move(draft);return true;
    }catch(const std::exception&){return false;}
};
struct EdResourceFile {
    std::shared_ptr<const EdResources::Bytes> data;size_t position=0;bool eof=false;
    size_t read(void *out,size_t n){size_t count=(std::min)(n,data->size()-position);if(count)memcpy(out,data->data()+position,count);position+=count;if(count<n)eof=true;return count;}
    int seek(int offset,int whence){int64_t next=(whence==SEEK_SET?0:whence==SEEK_CUR?(int64_t)position:whence==SEEK_END?(int64_t)data->size():-INT64_MAX)+(int64_t)offset;if(next<0||next>(int64_t)data->size())return -1;position=(size_t)next;eof=false;return 0;}
    char *gets(char *out,size_t n){if(!n||position==data->size()){eof=true;return nullptr;}size_t i=0;while(i+1<n&&position<data->size()){char c=(char)(*data)[position++];if(c=='\r')continue;out[i++]=c;if(c=='\n')break;}out[i]=0;return out;}
};
#endif
