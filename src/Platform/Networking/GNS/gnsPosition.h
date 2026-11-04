#ifndef GEOSITH_GNS_POSITION_H
#define GEOSITH_GNS_POSITION_H
#include <map>
#include <cstdint>
/* Position packets use lane 0, whose GNS message numbers increase even for
   unreliable messages.
   Track each sender/thing separately so reordered positions cannot rewind a
   thing, and updates to different things never suppress one another. */
static bool gnsAcceptPosition(std::map<uint64_t,int64_t> &numbers,uint32_t sender,uint32_t guid,int64_t number) {
    if(number<=0)return false;
    uint64_t key=(uint64_t)sender<<32|guid;
    auto previous=numbers.find(key);
    if(previous!=numbers.end()){if(number<=previous->second)return false;previous->second=number;return true;}
    if(numbers.size()>=4096)numbers.clear();
    numbers.emplace(key,number);return true;
}
#endif
