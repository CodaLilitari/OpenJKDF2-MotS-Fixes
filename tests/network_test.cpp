#include "../src/Platform/Networking/GNS/gnsAddress.h"
#include "../src/Platform/Networking/GNS/gnsPosition.h"
#include <cstdio>
#include <cstdlib>
static void check(bool value){if(!value){std::puts("FAIL: network address parser");std::exit(1);}}
int main(){std::string host,port,error;
check(gnsParseAddress("127.0.0.1",host,port,error)&&host=="127.0.0.1"&&port=="27020");
check(gnsParseAddress(" example.com:3456 ",host,port,error)&&host=="example.com"&&port=="3456");
check(gnsParseAddress("[2001:db8::1]:1234",host,port,error)&&host=="2001:db8::1"&&port=="1234");
check(gnsParseAddress("2001:db8::1",host,port,error)&&host=="2001:db8::1"&&port=="27020");
for(const char *bad:{"","  ","host:0","host:65536","host:","host:no","[::1","[::1]garbage","[::1]:-1","http://host","host name"})check(!gnsParseAddress(bad,host,port,error)&&!error.empty());
std::map<uint64_t,int64_t> positions;check(gnsAcceptPosition(positions,2,100,20));check(!gnsAcceptPosition(positions,2,100,19));check(!gnsAcceptPosition(positions,2,100,20));check(gnsAcceptPosition(positions,2,101,18));check(gnsAcceptPosition(positions,3,100,1));check(gnsAcceptPosition(positions,2,100,21));check(!gnsAcceptPosition(positions,2,100,0));
std::puts("PASS: sequenced positions reject stale/duplicate packets without dropping other things or senders");
std::puts("PASS: IPv4, hostnames, IPv6, whitespace, invalid ports and malformed addresses");}
