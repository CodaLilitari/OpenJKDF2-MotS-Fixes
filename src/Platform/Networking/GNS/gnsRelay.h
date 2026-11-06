// Optional Imperial Network Protocol transport for GeoSith, 2026-11-05.
// GPL-2.0-or-later. The imported fork owns UDP, relay discovery and NAT links;
// this adapter retains GeoSith authentication, IDs, messages and editor sync.
#include "relayChannel.h"
extern "C" {
#include "../DPlay/dplay_backend.h"
void jkrelay_SetServer(const char*);
void jkrelay_SetDirect(int);
void jkrelay_SetMasters(const char*);
void jkrelay_SetHostTail(const char*);
const char* jkrelay_GetSessionTail(const uint8_t*);
uint32_t jkrelay_LocalSystemId(void);
uint32_t jkrelay_HostSystemId(void);
void jkrelay_SetBigHandler(void (*)(uint32_t,const uint8_t*,size_t));
int jkrelay_SendBig(uint32_t,uint32_t,const uint8_t*,size_t);
void jkrelay_Idle(void);
void jkrelay_DescribeLinks(char*,size_t);
}

static bool relayActive=false;
static char relayAddress[256]="master.jkjunkie.com:27900";
static bool relayDirect=true;
static const uint8_t relayApp[16]={0x47,0x65,0x6f,0x53,0x69,0x74,0x68,0x52,0x65,0x6c,0x61,0x79,0,0,0,1};
struct RelayListing {dps_session session; jkMultiEntry entry;};

class GeoRelay {
    struct Peer {
        RelayChannel lanes[2];
        uint32_t slot=0,started=0;
        std::map<uint64_t,uint32_t> positions;
    };
    struct Packet {int sender;std::vector<uint8_t> bytes;};
    std::map<uint32_t,Peer> peers;
    std::queue<Packet> packets;
    std::queue<int> departed;
    bool started=false,hosting=false,joined=false,failed=false,disconnectReported=false;
    uint32_t connectTime=0,sequence=0;
    char16_t password[32]={};
    static GeoRelay* instance;
    static void Big(uint32_t from,const uint8_t* bytes,size_t size){if(instance)instance->Incoming(from,bytes,size);}
    void Raw(uint32_t to,const std::vector<uint8_t>& packet) {
        uint32_t me=jkrelay_LocalSystemId();if(!me||!to)return;
        if(packet.size()>1300)jkrelay_SendBig(me,to,packet.data(),packet.size());
        else jkrelay_backend.Send(me,to,packet.data(),(uint32_t)packet.size());
    }
    bool Queue(uint32_t to,uint32_t kind,const void* data,size_t size,int lane=1) {
        if(!to||size>RelayChannel::MaxPayload-4||peers.size()>=64&&!peers.count(to))return false;
        auto &p=peers[to];if(!p.started)p.started=stdPlatform_GetTimeMsec();
        std::vector<uint8_t> message(4+size);RelayChannel::put(message.data(),kind);
        if(size)memcpy(message.data()+4,data,size);
        return p.lanes[lane-1].queue(message.data(),message.size(),lane);
    }
    void Reject(uint32_t from,const char* text){Queue(from,4,text,strlen(text)+1);}
    void Message(uint32_t from,const RelayChannel::Bytes& data) {
        if(data.size()<4)return;uint32_t kind=RelayChannel::get(data.data());
        const uint8_t* body=data.data()+4;size_t size=data.size()-4;
        if(hosting&&kind==1&&size==sizeof(GNSAuthPacket)){
            GNSAuthPacket auth;memcpy(&auth,body,size);
            if(auth.magic!=GNS_MAGIC||auth.version!=GNS_VERSION||auth.kind!=GNS_AUTH||auth.layout!=GNSLayout()){Reject(from,"Different GeoSith version.");return;}
            if(auth.mode!=(uint32_t)Main_bMotsCompat||auth.assets!=gnsAssets){Reject(from,"Game resources differ from the host.");return;}
            unsigned mismatch=0;for(int i=0;i<32;i++)mismatch|=auth.password[i]^sithDplayGNS_storedEntry.wPassword[i];
            if(mismatch){Reject(from,"Incorrect game password.");return;}
            auto &peer=peers[from];if(!peer.slot){
                unsigned used=1;for(const auto &p:peers)if(p.second.slot)used++;
                if(used>=(unsigned)sithDplayGNS_storedEntry.maxPlayers){Reject(from,"The game is full.");return;}
                for(unsigned slot=2;slot<=32;slot++){bool occupied=false;for(const auto &p:peers)occupied|=p.second.slot==slot;if(!occupied){peer.slot=slot;break;}}
            }
            if(peer.slot){GNSInfoPacket info=PublicInfo(GNS_ACCEPT,peer.slot);Queue(from,2,&info,sizeof(info));}
        }else if(!hosting&&from==jkrelay_HostSystemId()&&kind==2&&size==sizeof(GNSInfoPacket)){
            GNSInfoPacket info;memcpy(&info,body,size);
            if(info.magic!=GNS_MAGIC||info.version!=GNS_VERSION||info.kind!=GNS_ACCEPT||info.layout!=GNSLayout()||info.assets!=gnsAssets||info.mode!=(uint32_t)Main_bMotsCompat||info.id<2||info.id>32){Fail("Invalid relay host authentication.");return;}
            id=info.id;peers[from].slot=1;NetworkStatus("Relay authenticated. Synchronizing level...");
        }else if(!hosting&&from==jkrelay_HostSystemId()&&kind==4&&size&&size<256&&body[size-1]==0){Fail((const char*)body);}
        else if(kind==3&&size>=12&&size<=2060){
            uint32_t sender=RelayChannel::get(body),to=RelayChannel::get(body+4);
            if(hosting){auto it=peers.find(from);if(it==peers.end()||!it->second.slot)return;sender=it->second.slot;
                if(to&&to!=1){Send(sender,to,body+8,size-8);return;}
            }else if(from!=jkrelay_HostSystemId()||id==0xffffffff||to&&to!=id||!sender||sender>32)return;
            if(packets.size()<4096)packets.push(Packet{(int)sender,std::vector<uint8_t>(body+8,body+size)});
            else Fail("Relay receive queue overflow.");
        }
    }
    void Incoming(uint32_t from,const uint8_t* bytes,size_t size) {
        if(!joined||!from||from==jkrelay_LocalSystemId()||size<16||size>2116||RelayChannel::get(bytes)!=RelayChannel::Magic)return;
        if(!hosting&&from!=jkrelay_HostSystemId())return;
        if(!peers.count(from)&&peers.size()>=64)return;
        uint32_t lane=RelayChannel::get(bytes+4);auto &peer=peers[from];if(!peer.started)peer.started=stdPlatform_GetTimeMsec();
        if(lane==0){
            if(size<32||RelayChannel::get(bytes+16)!=3||(RelayChannel::get(bytes+28)&255)!=DSS_THINGPOS)return;
            if(hosting&&!peer.slot)return;
            uint32_t seq=RelayChannel::get(bytes+8);if(size<36||!seq)return;
            uint64_t key=uint64_t(RelayChannel::get(bytes+20))<<32|RelayChannel::get(bytes+32);
            auto it=peer.positions.find(key);if(it!=peer.positions.end()){if(seq<=it->second)return;it->second=seq;}
            else{if(peer.positions.size()>=65536)return;peer.positions[key]=seq;}
            Message(from,RelayChannel::Bytes(bytes+16,bytes+size));return;
        }
        if(lane<1||lane>2)return;
        peer.lanes[lane-1].receive(bytes,size,[&](const auto &packet){Raw(from,packet);},[&](const auto &message){Message(from,message);});
    }
    void Fail(const char* message){if(!failed)NetworkStatus(message);failed=true;}
public:
    uint32_t id=0xffffffff;
    bool Start() {
        if(started)return true;instance=this;
        jkrelay_SetServer(relayAddress);
        if(jkrelay_backend.Startup(relayApp)){NetworkStatus("Could not start the relay client.");return false;}
        jkrelay_SetDirect(relayDirect);jkrelay_SetMasters("");jkrelay_SetBigHandler(Big);started=true;return true;
    }
    void Close(){if(started)jkrelay_backend.Close();peers.clear();packets={};departed={};joined=hosting=failed=disconnectReported=false;id=0xffffffff;sequence=0;memset(password,0,sizeof(password));}
    void UpdateDescription(){if(joined&&hosting)jkrelay_SetHostTail(Metadata(sithDplayGNS_storedEntry).c_str());}
    void Shutdown(){Close();if(started)jkrelay_backend.Shutdown();started=false;}
    bool Connecting()const{return joined&&!hosting&&!failed&&id==0xffffffff;}
    static std::string Metadata(const jkMultiEntry& entry) {
        // A relay's opaque session tail is at most 255 characters. Passwords
        // are never advertised; the complete handshake follows joining.
        uint8_t bytes[108]={};memcpy(bytes,entry.episodeGobName,32);memcpy(bytes+32,entry.mapJklFname,32);
        uint32_t values[]={GNS_VERSION,GNSLayout(),(uint32_t)Main_bMotsCompat,(uint32_t)gnsAssets,(uint32_t)(gnsAssets>>32),(uint32_t)entry.checksumSeed,(uint32_t)entry.tickRateMs,(uint32_t)entry.maxRank,(uint32_t)entry.multiModeFlags,(uint32_t)entry.sessionFlags};
        for(int i=0;i<10;i++)RelayChannel::put(bytes+64+4*i,values[i]);
        std::string hex;for(auto b:bytes){hex+="0123456789abcdef"[b>>4];hex+="0123456789abcdef"[b&15];}return hex;
    }
    static bool Decode(const dps_session& session,jkMultiEntry &entry) {
        const char* text=jkrelay_GetSessionTail(session.guidInstance);if(!text||strlen(text)!=216||session.maxPlayers<1||session.maxPlayers>32)return false;
        uint8_t bytes[108];for(size_t i=0;i<sizeof(bytes);i++){unsigned v=0;for(int j=0;j<2;j++){char c=text[2*i+j];int n=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;if(n<0)return false;v=v*16+n;}bytes[i]=(uint8_t)v;}
        if(!memchr(bytes,0,32)||!memchr(bytes+32,0,32)||RelayChannel::get(bytes+64)!=GNS_VERSION||RelayChannel::get(bytes+68)!=GNSLayout()||RelayChannel::get(bytes+72)!=(uint32_t)Main_bMotsCompat||RelayChannel::get(bytes+76)!=(uint32_t)gnsAssets||RelayChannel::get(bytes+80)!=(uint32_t)(gnsAssets>>32))return false;
        entry={};memcpy(&entry.guidInstance,session.guidInstance,16);memcpy(entry.episodeGobName,bytes,32);memcpy(entry.mapJklFname,bytes+32,32);
        memcpy(entry.serverName,session.name,31*2);entry.maxPlayers=session.maxPlayers;entry.numPlayers=session.curPlayers;
        entry.checksumSeed=RelayChannel::get(bytes+84);entry.tickRateMs=RelayChannel::get(bytes+88);entry.maxRank=RelayChannel::get(bytes+92);entry.multiModeFlags=RelayChannel::get(bytes+96);entry.sessionFlags=RelayChannel::get(bytes+100);entry.field_E0=10;return true;
    }
    std::vector<RelayListing> Enumerate() {
        std::vector<RelayListing> found;if(!Start())return found;jkrelay_SetServer(relayAddress);
        long result=jkrelay_backend.EnumSessions([](void* context,const dps_session* session){RelayListing listing={};listing.session=*session;if(Decode(*session,listing.entry)){auto &out=*(std::vector<RelayListing>*)context;if(out.size()<32)out.push_back(listing);}return 1;},&found);
        NetworkStatus(result?"Relay did not answer. Check its address and UDP access.":found.empty()?"No compatible GeoSith games on this relay.":"Relay games found. Select one and join.");return found;
    }
    bool Host() {
        Close();if(!HashAssets()||!Start())return false;jkrelay_SetServer(relayAddress);
        dps_session desc={};desc.maxPlayers=sithDplayGNS_storedEntry.maxPlayers;
        memcpy(desc.name,sithDplayGNS_storedEntry.serverName,64);memcpy(desc.password,sithDplayGNS_storedEntry.wPassword,64);
        if(desc.password[0])desc.flags|=DPS_SESSION_PASSWORDREQUIRED;
        jkrelay_SetHostTail(Metadata(sithDplayGNS_storedEntry).c_str());
        if(jkrelay_backend.OpenCreate(&desc)){NetworkStatus("Could not host on the relay. Check its address.");return false;}
        uint32_t player=0;if(jkrelay_backend.CreatePlayer((const uint16_t*)jkPlayer_playerShortName,&player)){Close();return false;}
        joined=hosting=true;id=1;NetworkStatus("Hosting through relay. Players need this relay address; no player port forwarding.");return true;
    }
    bool Join(const RelayListing &game,const char16_t* pw) {
        Close();if(!HashAssets()||!Start())return false;
        if(jkrelay_backend.OpenJoin(game.session.guidInstance,(const uint16_t*)pw)){NetworkStatus("Relay join failed. Check the password, slots and game listing.");return false;}
        uint32_t player=0;if(jkrelay_backend.CreatePlayer((const uint16_t*)jkPlayer_playerShortName,&player)){Close();return false;}
        joined=true;connectTime=stdPlatform_GetTimeMsec();if(pw)for(int i=0;i<31&&pw[i];i++)password[i]=pw[i];
        GNSAuthPacket auth={};auth.magic=GNS_MAGIC;auth.version=GNS_VERSION;auth.kind=GNS_AUTH;auth.mode=Main_bMotsCompat;auth.layout=GNSLayout();auth.assets=gnsAssets;memcpy(auth.password,password,sizeof(password));
        if(!Queue(jkrelay_HostSystemId(),1,&auth,sizeof(auth))){Fail("Relay host identity unavailable.");return false;}return true;
    }
    void Tick() {
        if(!joined||failed)return;jkrelay_Idle();
        for(int i=0;i<256;i++){uint32_t from=0,size=0;const void* data=nullptr;dps_sysmsg sys={};int r=jkrelay_backend.Receive(&from,&data,&size,&sys);if(r==DPS_RECV_NONE)break;
            if(r==DPS_RECV_DATA)Incoming(from,(const uint8_t*)data,size);
            else if(sys.kind==DPS_SYS_SESSIONLOST){Fail("Relay session ended.");break;}
            else if(sys.kind==DPS_SYS_PLAYERDESTROYED){auto it=peers.find(sys.playerId);if(it!=peers.end()){if(it->second.slot){departed.push(it->second.slot);jkEditor_NetPlayerLeft(it->second.slot);}peers.erase(it);}}
        }
        uint32_t now=stdPlatform_GetTimeMsec();
        if(Connecting()&&uint32_t(now-connectTime)>30000)Fail("Relay host authentication timed out.");
        for(auto it=peers.begin();it!=peers.end();){auto &p=it->second;bool ok=true;
            for(auto &lane:p.lanes)ok=lane.tick(now,[&](const auto &packet){Raw(it->first,packet);})&&ok;
            if(!ok||hosting&&!p.slot&&uint32_t(now-p.started)>30000){if(!hosting){Fail("Relay host stopped responding.");break;}if(p.slot){departed.push(p.slot);jkEditor_NetPlayerLeft(p.slot);}it=peers.erase(it);}else ++it;
        }
    }
    bool Send(uint32_t from,uint32_t to,const void* data,size_t size) {
        if(!joined||failed||id==0xffffffff||size<4||size>2052||to>32)return false;if(to==id)return true;
        uint32_t route=0;if(hosting){for(auto &p:peers)if(p.second.slot==to){route=p.first;break;}}else route=jkrelay_HostSystemId();if(!route)return false;
        std::vector<uint8_t> envelope(8+size);RelayChannel::put(envelope.data(),hosting?from:id);RelayChannel::put(envelope.data()+4,to);memcpy(envelope.data()+8,data,size);
        uint32_t type=RelayChannel::get((const uint8_t*)data)&255;
        if(type==DSS_THINGPOS){if(++sequence==0){Fail("Relay sequence exhausted; reconnect.");return false;}
            std::vector<uint8_t> packet(20+envelope.size());RelayChannel::put(packet.data(),RelayChannel::Magic);RelayChannel::put(packet.data()+8,sequence);RelayChannel::put(packet.data()+16,3);memcpy(packet.data()+20,envelope.data(),envelope.size());Raw(route,packet);return true;}
        return Queue(route,3,envelope.data(),envelope.size(),type==JKEDITOR_NET_MESSAGE?2:1);
    }
    int Receive(int* sender,void* data,int* size) {
        Tick();if(failed){if(disconnectReported)return -1;disconnectReported=true;*sender=1;*size=0;return 2;}
        if(!departed.empty()){*sender=departed.front();departed.pop();*size=0;return 2;}
        if(packets.empty())return -1;auto p=std::move(packets.front());packets.pop();if(p.bytes.size()>(size_t)*size)return -1;
        *sender=p.sender;*size=(int)p.bytes.size();memcpy(data,p.bytes.data(),p.bytes.size());return 0;
    }
};
GeoRelay* GeoRelay::instance=nullptr;
static GeoRelay relay;
