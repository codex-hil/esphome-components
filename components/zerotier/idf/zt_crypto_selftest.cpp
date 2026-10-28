#include "AES.hpp"
#include "SHA512.hpp"
#include <cstdio>
#include <cstring>
using namespace ZeroTier;
extern "C" int ztp_crypto_selftest(){
 unsigned char key[32],plain[16],out[16],back[16];
 for(int i=0;i<32;i++)key[i]=i;
 for(int i=0;i<16;i++)plain[i]=i*17;
 const unsigned char expected[]={0x8e,0xa2,0xb7,0xca,0x51,0x67,0x45,0xbf,0xea,0xfc,0x49,0x90,0x4b,0x49,0x60,0x89};
 AES a(key);a.encrypt(plain,out);a.decrypt(expected,back);
 printf("AES256 encrypt=%s decrypt=%s accelerated=%d\n",memcmp(out,expected,16)?"FAIL":"PASS",memcmp(back,plain,16)?"FAIL":"PASS",a.accelerated());
 for(int i=0;i<16;i++){printf("%02x",back[i]);}puts("");
 unsigned char pt[31],ct[31],dt[31],aad[11];
 for(int i=0;i<31;i++)pt[i]=i;
 for(int i=0;i<11;i++)aad[i]=i+20;
 AES k1("11111111111111111111111111111111");
 AES::GMACSIVEncryptor enc(a,k1);enc.init(0x0102030405060708ULL,ct);enc.aad(aad,11);enc.update1(pt,31);enc.finish1();enc.update2(pt,31);
 uint64_t tag[2];memcpy(tag,enc.finish2(),16);
 printf("TAG=");for(int i=0;i<16;i++)printf("%02x",((unsigned char*)tag)[i]);puts("");
 printf("CT=");for(int i=0;i<31;i++)printf("%02x",ct[i]);puts("");
 AES::GMACSIVDecryptor dec(a,k1);dec.init(tag,dt);dec.aad(aad,11);dec.update(ct,31);bool ok=dec.finish()&&!memcmp(dt,pt,31);
 printf("GMACSIV roundtrip=%s\n",ok?"PASS":"FAIL");
 const unsigned char expected_tag[16]={0x99,0x7e,0xfe,0xd2,0xac,0x87,0x4c,0xfe,0x43,0xda,0x85,0x05,0x5b,0xe0,0xaf,0xa5};
 const unsigned char expected_ct[31]={0xb1,0x99,0x5e,0xaa,0x7e,0xdb,0xe9,0x55,0xeb,0xf5,0xa1,0xa5,0x85,0x0e,0x25,0xf2,0xa6,0x73,0x68,0x59,0xc5,0xa0,0x8d,0xd2,0x44,0x4b,0xd4,0x06,0xd3,0x75,0x92};
 bool inplace_ok=true;
 for(unsigned off=0;off<8;off++){
  unsigned char buf[40];memcpy(buf+off,expected_ct,31);
  uint64_t public_tag[2];memcpy(public_tag,expected_tag,16);
  AES::GMACSIVDecryptor test(a,k1);test.init(public_tag,buf+off);test.aad(aad,11);test.update(buf+off,31);
  inplace_ok=inplace_ok && test.finish() && !memcmp(buf+off,pt,31);
 }
 printf("crypto_kat inplace_offsets_0_7=%s\n",inplace_ok?"PASS":"FAIL");
 bool fixture_ok=!memcmp(tag,expected_tag,16)&&!memcmp(ct,expected_ct,31);
 unsigned char dk[48],derived[48];for(int i=0;i<48;i++)dk[i]=i;
 ZeroTier::KBKDFHMACSHA384(dk,'0',0,0,derived);
 const unsigned char expected_kdf[48]={0x3c,0x9f,0x2d,0x14,0x6f,0x74,0x0b,0x35,0x62,0x19,0x36,0x9b,0x6e,0x60,0x49,0xa6,0x53,0xeb,0x78,0x2d,0xda,0x0d,0xc7,0xb2,0x29,0xaf,0x44,0x78,0x09,0x63,0xb9,0x33,0xf0,0x54,0xe1,0x21,0xcc,0xf3,0xf9,0xdc,0xfe,0xef,0xba,0x2d,0x3b,0x0d,0xce,0x2a};
 bool kdf_ok=!memcmp(derived,expected_kdf,48);
 printf("crypto_kat aes_gmac_siv_fixture=%s kdf=%s\n",fixture_ok?"PASS":"FAIL",kdf_ok?"PASS":"FAIL");
 return memcmp(out,expected,16)||memcmp(back,plain,16)||!ok||!fixture_ok||!kdf_ok||!inplace_ok;
}

#ifdef ZTP_SELFTEST_HOST
int main(){return ztp_crypto_selftest();}
#endif
