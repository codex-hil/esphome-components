#pragma once
#include <stddef.h>
#include <stdint.h>
/* Ethernet II, one IPv4 endpoint, no VLAN or bridging. */
#define ZT_PORT_MTU 1280
#define ZT_PORT_FRAME_MAX (ZT_PORT_MTU + 14)
static inline uint64_t ztp_mac(const uint8_t *p) {
 uint64_t v=0; for (int i=0;i<6;i++) v=(v<<8)|p[i]; return v;
}
static inline void ztp_put_mac(uint8_t *p,uint64_t v) {
 for (int i=5;i>=0;i--) {p[i]=(uint8_t)v;v>>=8;}
}
static inline int ztp_frame_valid(const uint8_t *p,size_t n) {
 if (!p || n<14 || n>ZT_PORT_FRAME_MAX) return 0;
 unsigned t=((unsigned)p[12]<<8)|p[13];
 return (t==0x0800 && n>=34) || (t==0x0806 && n>=42);
}
static inline uint32_t ztp_mask(unsigned prefix) {
 return prefix && prefix<=32 ? UINT32_MAX << (32-prefix) : 0;
}
/* Host-order addresses; only the canonical route already implied by netif. */
static inline int ztp_connected_route(uint32_t ip,unsigned prefix,uint32_t target,unsigned target_prefix) {
 return prefix>=1 && prefix<=30 && target_prefix==prefix && target==(ip&ztp_mask(prefix));
}
