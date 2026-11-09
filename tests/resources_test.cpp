#include "../src/Main/edResources.h"
#include <cassert>
#include <iostream>
#include <chrono>
static void check(bool ok){if(!ok)throw std::runtime_error("Resource test failed");}
int main(){
    for(auto name:{"../mat/a.mat","/mat/a.mat","mat/../a.mat","mat/a.exe","cog/a.dll","C:/mat/a.mat","mat/a:ads.mat","mat//a.mat","mat/a./b.mat"})check(EdResources::path(name).empty());
    check(EdResources::path("3DO\\MAT\\Test.MAT")=="3do/mat/test.mat");
    EdResources a;a.add("mat/a.mat",{1,2,3});a.add("cog/test.cog",{'a','\r','\n','b'});
    auto data=a.encode();EdResources b;check(EdResources::decode(data.data(),data.size(),b));check(*a.files.at("mat/a.mat")==*b.files.at("mat/a.mat"));
    for(size_t i=0;i<data.size();i++)check(!EdResources::decode(data.data(),i,b));
    auto bad=data;bad[4]=255;check(!EdResources::decode(bad.data(),bad.size(),b));bad=data;bad[12]=255;bad[13]=255;bad[14]=255;bad[15]=255;check(!EdResources::decode(bad.data(),bad.size(),b));
    EdResourceFile file{a.files.at("cog/test.cog")};a.files.clear();char line[8];check(file.gets(line,8)&&!strcmp(line,"a\n"));check(file.seek(-1,SEEK_END)==0);check(file.read(line,8)==1&&line[0]=='b'&&file.eof);check(file.seek(0,SEEK_SET)==0&&!file.eof);check(file.seek(-1,SEEK_SET)==-1);
    auto root=std::filesystem::temp_directory_path()/std::filesystem::path("geosith-resource-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root/"Mod/mat");{std::ofstream f(root/"Mod/mat/a.mat",std::ios::binary);f<<"abc";}
    EdResources disk;disk.discover(root);check(disk.files.count("mat/a.mat")==1&&disk.files.size()==1);
    EdResources::Bytes gob={'G','O','B',' '};EdResources::put(gob,20);EdResources::put(gob,15);gob.insert(gob.end(),{'x','y','z'});EdResources::put(gob,1);EdResources::put(gob,12);EdResources::put(gob,3);
    size_t name=gob.size();gob.resize(name+128);memcpy(gob.data()+name,"mat\\gob.mat",11);
    auto archive=root/"pack.gob";{std::ofstream f(archive,std::ios::binary);f.write((char*)gob.data(),gob.size());}
    disk.archive(archive);check(disk.files.count("mat/gob.mat")==1&&disk.files.at("mat/gob.mat")->at(0)=='x');
    gob[19]=255;gob[20]=255;gob[21]=255;gob[22]=127;{std::ofstream f(archive,std::ios::binary);f.write((char*)gob.data(),gob.size());}
    bool rejected=false;try{disk.archive(archive);}catch(const std::exception&){rejected=true;}check(rejected);std::filesystem::remove(archive);
    std::filesystem::remove(root/"Mod/mat/a.mat");std::filesystem::remove(root/"Mod/mat");std::filesystem::remove(root/"Mod");std::filesystem::remove(root);
    std::cout<<"PASS: resource paths, bounded decoding, memory file lifetime, folder discovery\n";
}
