#ifndef GEOSITH_GNS_ADDRESS_H
#define GEOSITH_GNS_ADDRESS_H
#include <string>
#include <cctype>
/* Pure parser shared by discovery and its regression tests. Unbracketed IPv6
   uses the default port; an explicit port requires [IPv6]:port. */
static bool gnsParseAddress(std::string input,std::string &host,std::string &port,std::string &error) {
    host.clear();port="27020";error.clear();
    size_t first=input.find_first_not_of(" \t\r\n"),last=input.find_last_not_of(" \t\r\n");
    if(first==std::string::npos){error="Enter the host's IP address or hostname.";return false;}
    input=input.substr(first,last-first+1);
    if(input[0]=='['){size_t close=input.find(']');if(close==std::string::npos){error="IPv6 addresses must end with ].";return false;}
        host=input.substr(1,close-1);if(close+1<input.size()){if(input[close+1]!=':'){error="Use [IPv6]:port.";return false;}port=input.substr(close+2);}}
    else {size_t colon=input.find(':');if(colon!=std::string::npos&&colon==input.rfind(':')){host=input.substr(0,colon);port=input.substr(colon+1);}else host=input;}
    if(host.empty()||host.find_first_of(" /\\\t\r\n[]")!=std::string::npos){error="Invalid host address. Use an IP or hostname without a URL prefix.";return false;}
    unsigned int value=0;if(port.empty()||port.size()>5){error="Port must be a number from 1 to 65535.";return false;}
    for(char c:port){if(!isdigit((unsigned char)c)){error="Port must be a number from 1 to 65535.";return false;}value=value*10+(c-'0');}
    if(!value||value>65535){error="Port must be a number from 1 to 65535.";return false;}return true;
}
#endif
