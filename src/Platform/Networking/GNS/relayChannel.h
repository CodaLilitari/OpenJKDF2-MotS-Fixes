// GeoSith relay adapter, 2026-11-05. GPL-2.0-or-later.
#ifndef GEOSITH_RELAY_CHANNEL_H
#define GEOSITH_RELAY_CHANNEL_H
#include <cstdint>
#include <algorithm>
#include <vector>
#include <map>
#include <functional>

// Reliable, ordered windows over the fork's datagrams. Separate gameplay and
// editor channels prevent a large level transfer from blocking gameplay.
class RelayChannel {
public:
    using Bytes = std::vector<uint8_t>;
    using Send = std::function<void(const Bytes&)>;
    using Deliver = std::function<void(const Bytes&)>;
    static constexpr uint32_t Magic=0x31525347, Window=16, MaxPayload=2100;
    static uint32_t get(const uint8_t* p) {return p[0]|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
    static void put(uint8_t* p,uint32_t v) {for(int i=0;i<4;i++)p[i]=uint8_t(v>>(8*i));}
private:
    struct Pending {Bytes packet;uint32_t first=0,last=0;bool sent=false;};
    uint32_t next=1,expected=1;
    std::map<uint32_t,Pending> pending;
    std::map<uint32_t,Bytes> incoming;
public:
    bool queue(const void* data,size_t size,uint32_t lane) {
        if(!size||size>MaxPayload||pending.size()>=4096||next==UINT32_MAX)return false;
        Bytes packet(16+size);put(packet.data(),Magic);put(packet.data()+4,lane);
        put(packet.data()+8,next);put(packet.data()+12,0);
        std::copy_n((const uint8_t*)data,size,packet.data()+16);
        pending.emplace(next++,Pending{std::move(packet)});return true;
    }
    bool tick(uint32_t now,const Send& send) {
        unsigned count=0;
        for(auto &entry:pending){if(count++==Window)break;auto &p=entry.second;
            if(p.sent&&uint32_t(now-p.first)>30000)return false;
            if(!p.sent||uint32_t(now-p.last)>=150){if(!p.sent)p.first=now;p.last=now;p.sent=true;send(p.packet);}
        }return true;
    }
    bool receive(const uint8_t* data,size_t size,const Send& send,const Deliver& deliver) {
        if(size<16||size>16+MaxPayload||get(data)!=Magic)return false;
        uint32_t seq=get(data+8),ack=get(data+12);
        if(ack){if(size!=16||seq||ack>=next)return false;
            auto it=pending.find(ack);if(it!=pending.end()&&it->second.sent)pending.erase(it);return true;}
        if(!seq||size==16||seq>=uint64_t(expected)+Window)return false;
        Bytes response(data,data+16);put(response.data()+8,0);put(response.data()+12,seq);send(response);
        if(seq<expected)return true;
        incoming.emplace(seq,Bytes(data+16,data+size));
        for(;;){auto it=incoming.find(expected);if(it==incoming.end())break;
            Bytes message=std::move(it->second);incoming.erase(it);++expected;deliver(message);}
        return true;
    }
    size_t queued() const {return pending.size();}
};
#endif
