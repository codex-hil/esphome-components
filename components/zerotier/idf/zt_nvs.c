/* MPL-2.0. Preflight NVS reads before Node_new; never turn read errors into a
 * missing identity. Callbacks use the validated RAM snapshot, one owner only. */
#include "nvs.h"
#include <stdbool.h>
#include "ZeroTierOne.h"
#include <string.h>
static nvs_handle_t handle;
static int opened;
static esp_err_t error;
static char secret[2048],public_id[512];
static size_t secret_len,public_len;
static esp_err_t read_blob(const char *key,char *buf,size_t *len,size_t capacity){
 *len=capacity;esp_err_t e=nvs_get_blob(handle,key,buf,len);
 if(e==ESP_ERR_NVS_NOT_FOUND){*len=0;return ESP_OK;}
 if(e==ESP_OK && (*len==0 || *len>=capacity))return ESP_ERR_INVALID_SIZE;
 return e;
}
esp_err_t ztp_storage_init(void){
 error=nvs_open("zerotier",NVS_READWRITE,&handle);
 if(error!=ESP_OK)return error;
 opened=1;
 error=read_blob("identity",secret,&secret_len,sizeof secret);
 if(error==ESP_OK)error=read_blob("public",public_id,&public_len,sizeof public_id);
 /* Orphan public identity means incomplete/corrupt storage, not a new node. */
 if(error==ESP_OK && !secret_len && public_len)error=ESP_ERR_INVALID_STATE;
 return error;
}
esp_err_t ztp_storage_error(void){return error;}
void ztp_storage_close(void){
 if(opened)nvs_close(handle);
 opened=0;
 volatile char *v=secret;for(size_t i=0;i<sizeof secret;i++)v[i]=0;
 memset(public_id,0,sizeof public_id);secret_len=public_len=0;
}
void ztp_state_put(ZT_Node*n,void*u,void*t,enum ZT_StateObjectType type,
 const uint64_t id[2],const void*data,int len){
 const char *key=NULL;char *buf=NULL;size_t *used=NULL,cap=0;
 if(type==ZT_STATE_OBJECT_IDENTITY_SECRET){key="identity";buf=secret;used=&secret_len;cap=sizeof secret;}
 if(type==ZT_STATE_OBJECT_IDENTITY_PUBLIC){key="public";buf=public_id;used=&public_len;cap=sizeof public_id;}
 if(!key||error!=ESP_OK)return;
 if(len<=0||(size_t)len>=cap){error=ESP_ERR_INVALID_SIZE;return;}
 if(type==ZT_STATE_OBJECT_IDENTITY_SECRET && secret_len &&
    (secret_len!=(size_t)len || memcmp(secret,data,len))){error=ESP_ERR_INVALID_STATE;return;}
 error=nvs_set_blob(handle,key,data,len);
 if(error==ESP_OK)error=nvs_commit(handle);
 if(error==ESP_OK){memcpy(buf,data,len);*used=len;}
}
int ztp_state_get(ZT_Node*n,void*u,void*t,enum ZT_StateObjectType type,
 const uint64_t id[2],void*data,unsigned len){
 const void *buf;size_t size;
 if(type==ZT_STATE_OBJECT_IDENTITY_SECRET){buf=secret;size=secret_len;}
 else if(type==ZT_STATE_OBJECT_IDENTITY_PUBLIC){buf=public_id;size=public_len;}
 else return -1;
 if(!size)return -1;
 if(size>len){error=ESP_ERR_INVALID_SIZE;return 0;}
 memcpy(data,buf,size);return (int)size;
}
