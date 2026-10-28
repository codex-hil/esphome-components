#include "zt_frame.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
 uint8_t b[ZT_PORT_FRAME_MAX+1]={0};
 ztp_put_mac(b,0x0123456789abULL);assert(ztp_mac(b)==0x0123456789abULL);
 assert(!ztp_frame_valid(NULL,42));
 b[12]=8;b[13]=0;
 for(size_t n=0;n<34;n++) assert(!ztp_frame_valid(b,n));
 assert(ztp_frame_valid(b,34));assert(ztp_frame_valid(b,ZT_PORT_FRAME_MAX));
 assert(!ztp_frame_valid(b,sizeof b));
 b[13]=6; assert(!ztp_frame_valid(b,41));assert(ztp_frame_valid(b,42));
 b[12]=0x81;b[13]=0;assert(!ztp_frame_valid(b,42));
 b[12]=0x86;b[13]=0xdd;assert(!ztp_frame_valid(b,42));
 assert(ztp_mask(0)==0);assert(ztp_mask(24)==0xffffff00);assert(ztp_mask(32)==UINT32_MAX);assert(ztp_mask(33)==0);
 assert(ztp_connected_route(0xc0a8c0d8,24,0xc0a8c000,24));
 assert(!ztp_connected_route(0xc0a8c0d8,24,0xc0a80200,24));
 assert(!ztp_connected_route(0xc0a8c0d8,24,0,0));
 assert(!ztp_connected_route(0xc0a8c0d8,24,0xc0a8c0d8,24));
 assert(!ztp_connected_route(0xc0a8c0d8,24,0xc0a80000,16));
 puts("PASS: frame bounds, MAC byte order, rejected VLAN/IPv6, prefix bounds");
}
