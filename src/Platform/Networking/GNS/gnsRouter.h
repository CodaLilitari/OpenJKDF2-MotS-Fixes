#ifndef GEOSITH_GNS_ROUTER_H
#define GEOSITH_GNS_ROUTER_H
/* Optional Windows UPnP mapping. Work stays off the game thread; never replace
   another application's mapping, and remove only mappings this session owns. */
#ifdef _WIN32
#include <natupnp.h>
#include <memory>
struct RouterMapping { std::atomic<bool> running{true},done{false};uint16_t port; };
static std::shared_ptr<RouterMapping> routerMapping;
static std::mutex *routerMutex=new std::mutex;
static void StopRouterMapping(){auto state=routerMapping;if(state)state->running=false;routerMapping.reset();if(state)for(int i=0;i<20&&!state->done;i++)std::this_thread::sleep_for(std::chrono::milliseconds(25));}
static void StartRouterMapping(uint16_t port){
    StopRouterMapping();if(getenv("GEOSITH_NET_ROLE")||getenv("GEOSITH_DISABLE_UPNP"))return;
    auto state=std::make_shared<RouterMapping>();state->port=port;routerMapping=state;
    std::thread([state](){
        std::lock_guard<std::mutex> serialized(*routerMutex);if(!state->running){state->done=true;return;}
        HRESULT initialized=CoInitializeEx(nullptr,COINIT_MULTITHREADED);if(FAILED(initialized)){state->done=true;return;}
        IUPnPNAT *nat=nullptr;IStaticPortMappingCollection *mappings=nullptr;IStaticPortMapping *mapping=nullptr;
        BSTR protocol=SysAllocString(L"UDP"),client=nullptr,description=SysAllocString(L"GeoSith editor session");bool owned=false;
        SOCKET route=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);sockaddr_in remote={};remote.sin_family=AF_INET;remote.sin_port=htons(9);inet_pton(AF_INET,"8.8.8.8",&remote.sin_addr);
        sockaddr_in local={};int localLength=sizeof(local);char localIP[64]={};
        if(route!=INVALID_SOCKET&&connect(route,(sockaddr*)&remote,sizeof(remote))==0&&getsockname(route,(sockaddr*)&local,&localLength)==0)inet_ntop(AF_INET,&local.sin_addr,localIP,sizeof(localIP));
        if(route!=INVALID_SOCKET)closesocket(route);
        wchar_t wideIP[64]={};MultiByteToWideChar(CP_UTF8,0,localIP,-1,wideIP,64);client=SysAllocString(wideIP);
        if(*localIP&&SUCCEEDED(CoCreateInstance(__uuidof(UPnPNAT),nullptr,CLSCTX_INPROC_SERVER,__uuidof(IUPnPNAT),(void**)&nat))&&SUCCEEDED(nat->get_StaticPortMappingCollection(&mappings))&&mappings&&state->running){
            /* A pre-existing mapping belongs to the user or another session. */
            HRESULT existing=mappings->get_Item(state->port,protocol,&mapping);
            if(FAILED(existing)||!mapping){
                if(SUCCEEDED(mappings->Add(state->port,protocol,state->port,client,VARIANT_TRUE,description,&mapping))&&mapping)owned=true;
            }
            if(mapping&&state->running){BSTR external=nullptr,internal=nullptr;long internalPort=0;mapping->get_InternalClient(&internal);mapping->get_InternalPort(&internalPort);
                if(internal&&wcscmp(internal,wideIP)==0&&internalPort==state->port&&SUCCEEDED(mapping->get_ExternalIPAddress(&external))&&external){
                    char ip[128]={},text[256];WideCharToMultiByte(CP_UTF8,0,external,-1,ip,sizeof(ip),nullptr,nullptr);
                    snprintf(text,sizeof(text),"Router mapping ready: %s:%u UDP. Allow GeoSith through Windows Firewall. Internet reachability is not yet verified.",ip,state->port);NetworkStatus(text);
                }else NetworkStatus("Automatic mapping is unavailable or this UDP port is mapped to another computer. Use router forwarding or a shared VPN.");
                SysFreeString(external);SysFreeString(internal);
            }else if(state->running)NetworkStatus("UPnP is unavailable. Forward the hosting UDP port manually, or use the host's address on a shared VPN.");
        }else if(state->running)NetworkStatus("Router does not offer UPnP. Forward the hosting UDP port manually, or use a shared VPN. CGNAT may require a relay/VPN.");
        while(owned&&state->running)std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if(owned&&mappings){IStaticPortMapping *current=nullptr;if(SUCCEEDED(mappings->get_Item(state->port,protocol,&current))&&current){BSTR name=nullptr,address=nullptr;long mappedPort=0;current->get_Description(&name);current->get_InternalClient(&address);current->get_InternalPort(&mappedPort);
            if(name&&address&&wcscmp(name,L"GeoSith editor session")==0&&wcscmp(address,wideIP)==0&&mappedPort==state->port)mappings->Remove(state->port,protocol);
            SysFreeString(name);SysFreeString(address);current->Release();}}
        if(mapping)mapping->Release();if(mappings)mappings->Release();if(nat)nat->Release();SysFreeString(protocol);SysFreeString(client);SysFreeString(description);CoUninitialize();state->done=true;
    }).detach();
}
#else
static void StopRouterMapping(){}
static void StartRouterMapping(uint16_t){}
#endif
#endif
