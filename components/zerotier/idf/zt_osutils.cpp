// MPL-2.0. Minimal IDF implementations of upstream OS formatting functions.
#include "OSUtils.hpp"
#include <cstdarg>
#include <inttypes.h>
namespace ZeroTier {
unsigned int OSUtils::ztsnprintf(char *buf,unsigned int len,const char *fmt,...) {
 va_list ap;va_start(ap,fmt);int n=vsnprintf(buf,len,fmt,ap);va_end(ap);
 if(n<0 || (unsigned)n>=len){if(len)buf[len-1]=0;throw std::length_error("buf[] overflow");}return n;
}
std::string OSUtils::networkIDStr(uint64_t id){char b[32];ztsnprintf(b,sizeof b,"%016" PRIx64,id);return b;}
std::string OSUtils::nodeIDStr(uint64_t id){char b[32];ztsnprintf(b,sizeof b,"%010" PRIx64,id);return b;}
}
