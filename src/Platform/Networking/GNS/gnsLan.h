#ifndef GEOSITH_GNS_LAN_H
#define GEOSITH_GNS_LAN_H
#ifndef _WIN32
#include <fcntl.h>
#define closesocket close
#endif
static SOCKET lanSocket=INVALID_SOCKET;static uint16_t lanHostPort;
static void Nonblocking(SOCKET socket){
#ifdef _WIN32
    u_long enabled=1;ioctlsocket(socket,FIONBIO,&enabled);
#else
    fcntl(socket,F_SETFL,fcntl(socket,F_GETFL,0)|O_NONBLOCK);
#endif
}
static void LanStop(){if(lanSocket!=INVALID_SOCKET)closesocket(lanSocket);lanSocket=INVALID_SOCKET;}
static void LanStart(uint16_t port){LanStop();lanHostPort=port;lanSocket=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);if(lanSocket==INVALID_SOCKET)return;
    int reuse=1;setsockopt(lanSocket,SOL_SOCKET,SO_REUSEADDR,(const char*)&reuse,sizeof(reuse));sockaddr_in address={};address.sin_family=AF_INET;address.sin_port=htons(27019);
    if(bind(lanSocket,(sockaddr*)&address,sizeof(address))){LanStop();stdPlatform_Printf("GeoSith LAN discovery unavailable on UDP 27019; direct IP still works.\n");return;}Nonblocking(lanSocket);
}
static void LanPoll(){if(lanSocket==INVALID_SOCKET)return;for(int i=0;i<16;i++){
    uint32_t request[3];sockaddr_in address={};socklen_t size=sizeof(address);int count=recvfrom(lanSocket,(char*)request,sizeof(request),0,(sockaddr*)&address,&size);if(count<0)break;
    if(count!=sizeof(request)||request[0]!=GNS_MAGIC||request[1]!=GNS_VERSION||request[2]!=0x4c414e31)continue;
    GNSInfoPacket info=PublicInfo(GNS_INFO,lanHostPort);sendto(lanSocket,(const char*)&info,sizeof(info),0,(sockaddr*)&address,size);
}}
struct LanGame {SteamNetworkingIPAddr address;jkMultiEntry entry;};
static std::vector<LanGame> LanQuery(const std::atomic<bool> &running){std::vector<LanGame> games;SOCKET socketHandle=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);if(socketHandle==INVALID_SOCKET)return games;
    int broadcast=1;setsockopt(socketHandle,SOL_SOCKET,SO_BROADCAST,(const char*)&broadcast,sizeof(broadcast));Nonblocking(socketHandle);
    uint32_t request[3]={GNS_MAGIC,GNS_VERSION,0x4c414e31};sockaddr_in target={};target.sin_family=AF_INET;target.sin_port=htons(27019);target.sin_addr.s_addr=INADDR_BROADCAST;
    sendto(socketHandle,(const char*)request,sizeof(request),0,(sockaddr*)&target,sizeof(target));inet_pton(AF_INET,"127.0.0.1",&target.sin_addr);sendto(socketHandle,(const char*)request,sizeof(request),0,(sockaddr*)&target,sizeof(target));
    uint32_t began=stdPlatform_GetTimeMsec();while(running&&stdPlatform_GetTimeMsec()-began<1500){GNSInfoPacket info={};sockaddr_in source={};socklen_t length=sizeof(source);int count=recvfrom(socketHandle,(char*)&info,sizeof(info),0,(sockaddr*)&source,&length);
        if(count==sizeof(info)&&info.magic==GNS_MAGIC&&info.version==GNS_VERSION&&info.kind==GNS_INFO&&info.id>0&&info.id<=65535&&info.layout==GNSLayout()&&info.mode==(uint32_t)Main_bMotsCompat&&info.assets==gnsAssets&&info.entry.maxPlayers>0&&info.entry.maxPlayers<=32&&memchr(info.entry.episodeGobName,0,32)&&memchr(info.entry.mapJklFname,0,32)){
            char ip[64],endpoint[96];inet_ntop(AF_INET,&source.sin_addr,ip,sizeof(ip));snprintf(endpoint,sizeof(endpoint),"%s:%u",ip,info.id);SteamNetworkingIPAddr address;address.Clear();
            if(g_SteamNetworkingIPAddr_ParseString(&address,endpoint)){bool seen=false;for(const auto &game:games)if(game.address==address)seen=true;if(!seen&&games.size()<32){info.entry.serverName[31]=0;memset(info.entry.wPassword,0,sizeof(info.entry.wPassword));games.push_back({address,info.entry});}}
        }SDL_Delay(10);
    }closesocket(socketHandle);return games;
}
#endif
