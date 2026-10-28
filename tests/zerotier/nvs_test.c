/* Fault injection for identity persistence, no real credentials. */
#include "nvs.h"
#include <stdbool.h>
#include "ZeroTierOne.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
extern esp_err_t ztp_storage_init(void),ztp_storage_error(void);
extern void ztp_storage_close(void);
extern void ztp_state_put(ZT_Node*,void*,void*,enum ZT_StateObjectType,const uint64_t[2],const void*,int);
extern int ztp_state_get(ZT_Node*,void*,void*,enum ZT_StateObjectType,const uint64_t[2],void*,unsigned);
static int read_error, commit_error, writes, commits, closed;
static const char *stored_secret,*stored_public;
esp_err_t nvs_open(const char*n,int mode,nvs_handle_t*h){(void)n;(void)mode;*h=1;return ESP_OK;}
esp_err_t nvs_get_blob(nvs_handle_t h,const char*k,void*b,size_t*l){
 (void)h;if(read_error)return read_error;
 const char *v=!strcmp(k,"identity")?stored_secret:stored_public;
 if(!v)return ESP_ERR_NVS_NOT_FOUND;
 size_t n=strlen(v);if(n>*l)return ESP_ERR_INVALID_SIZE;
 memcpy(b,v,n);*l=n;return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h,const char*k,const void*b,size_t l){(void)h;(void)k;(void)b;(void)l;writes++;return ESP_OK;}
esp_err_t nvs_commit(nvs_handle_t h){(void)h;commits++;return commit_error;}
void nvs_close(nvs_handle_t h){(void)h;closed++;}
static void put(const char*v){uint64_t id[2]={0};ztp_state_put(NULL,NULL,NULL,ZT_STATE_OBJECT_IDENTITY_SECRET,id,v,strlen(v));}
int main(void){
 char out[64];uint64_t id[2]={0};
 assert(ztp_storage_init()==ESP_OK);
 assert(ztp_state_get(NULL,NULL,NULL,ZT_STATE_OBJECT_IDENTITY_SECRET,id,out,sizeof out)==-1);
 put("fixture-secret");assert(writes==1&&commits==1&&ztp_storage_error()==ESP_OK);
 int n=ztp_state_get(NULL,NULL,NULL,ZT_STATE_OBJECT_IDENTITY_SECRET,id,out,sizeof out);
 assert(n==14&&!memcmp(out,"fixture-secret",14));
 put("different-secret");assert(ztp_storage_error()==ESP_ERR_INVALID_STATE&&writes==1);
 ztp_storage_close();
 read_error=ESP_FAIL;assert(ztp_storage_init()==ESP_FAIL);put("fixture-secret");assert(writes==1);ztp_storage_close();read_error=0;
 stored_public="orphan";assert(ztp_storage_init()==ESP_ERR_INVALID_STATE);ztp_storage_close();stored_public=NULL;
 stored_secret="existing";assert(ztp_storage_init()==ESP_OK);
 assert(ztp_state_get(NULL,NULL,NULL,ZT_STATE_OBJECT_IDENTITY_SECRET,id,out,1)==0);
 assert(ztp_storage_error()==ESP_ERR_INVALID_SIZE);put("new");assert(writes==1);ztp_storage_close();
 stored_secret=NULL;commit_error=ESP_FAIL;assert(ztp_storage_init()==ESP_OK);put("fixture-secret");assert(ztp_storage_error()==ESP_FAIL&&commits==2);
 put("new");assert(writes==2);ztp_storage_close();assert(closed==5);
 puts("PASS: identity cache, read/commit errors, orphan public, replacement refusal");
}
