#include "../src/Platform/Networking/GNS/relayChannel.h"
#include <cstdio>
#include <cstdlib>
#include <random>
#include <algorithm>

static void require(bool ok){if(!ok){std::puts("FAIL: relay channel");std::exit(1);}}
int main(){
    RelayChannel sender,receiver;std::mt19937 random(42);
    struct Datagram {bool ack;RelayChannel::Bytes bytes;};std::vector<Datagram> wire;
    auto enqueue=[&](bool ack,const auto &bytes){if(random()%5==0)return;wire.push_back({ack,bytes});if(random()%4==0)wire.push_back({ack,bytes});};
    unsigned delivered=0;
    for(unsigned i=0;i<200;i++){std::vector<uint8_t> payload(1900,uint8_t(i));RelayChannel::put(payload.data(),i);require(sender.queue(payload.data(),payload.size(),2));}
    for(uint32_t time=1;time<30000&&delivered<200;time+=20){
        require(sender.tick(time,[&](const auto &bytes){enqueue(false,bytes);}));
        std::shuffle(wire.begin(),wire.end(),random);auto batch=std::move(wire);wire.clear();
        for(auto &d:batch){if(d.ack)sender.receive(d.bytes.data(),d.bytes.size(),[](const auto&){},[](const auto&){});
            else receiver.receive(d.bytes.data(),d.bytes.size(),[&](const auto &bytes){enqueue(true,bytes);},[&](const auto &bytes){require(bytes.size()==1900&&RelayChannel::get(bytes.data())==delivered);for(size_t i=4;i<bytes.size();i++)require(bytes[i]==uint8_t(delivered));delivered++;});}
    }
    require(delivered==200);
    RelayChannel dead;uint32_t value=1;require(dead.queue(&value,4,1));require(dead.tick(1,[](const auto&){}));require(!dead.tick(30002,[](const auto&){}));
    RelayChannel bounds;require(!bounds.queue(&value,0,1));require(!bounds.queue(&value,RelayChannel::MaxPayload+1,1));
    std::puts("PASS: relay windows preserve ordering under loss, duplication and reordering; payload bounds and timeout");
}
