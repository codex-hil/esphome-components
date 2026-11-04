/* Experimental adapter, MPL-2.0. One task owns every call into ZT_Node.
 * Never invoke ZT_Node from tcpip_thread: linkoutput copies into bounded queue. */
#include "zerotier_native.h"
#include "zt_frame.h"
#include "ZeroTierOne.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include <stdatomic.h>
#include "esp_task_wdt.h"
#include "lwip/sockets.h"
#include "lwip/netifapi.h"
#include "lwip/tcpip.h"
#include "lwip/etharp.h"
#include "netif/ethernet.h"
#include <string.h>
#include <errno.h>
#include <stdlib.h>
extern void ztp_state_put(ZT_Node*,void*,void*,enum ZT_StateObjectType,const uint64_t[2],const void*,int);
extern int ztp_state_get(ZT_Node*,void*,void*,enum ZT_StateObjectType,const uint64_t[2],void*,unsigned);
static atomic_bool running, stop_requested;
static portMUX_TYPE status_mux=portMUX_INITIALIZER_UNLOCKED;
static zerotier_native_status_t status;
static struct {
 ZT_Node *node; uint64_t nwid; int fd; esp_netif_t *eth;
 struct netif netif; QueueHandle_t tx; int added, active, link_ok, online; uint64_t node_id; esp_err_t error; int network_status;
 struct {uint32_t ip,mask;uint64_t mac;int up;} config;
 uint32_t subscribed_ip;
 unsigned wire_tx,wire_rx,wire_error,rx_frames;
 int pending; atomic_uint rx_drop,tx_drop,tx_frames;
} s;
typedef struct {uint16_t len;uint8_t bytes[ZT_PORT_FRAME_MAX];} frame_t;
/* The upstream fatal macro evaluates its argument twice; pass a value. */
static int fatal(enum ZT_ResultCode code){return ZT_ResultCode_isFatal(code);}
static int64_t now(void){return esp_timer_get_time()/1000;}
static err_t output(struct netif *n,struct pbuf *p){
 s.tx_frames++;
 frame_t f;if(p->tot_len>sizeof f.bytes){s.tx_drop++;return ERR_BUF;}
 f.len=pbuf_copy_partial(p,f.bytes,p->tot_len,0);
 if(!ztp_frame_valid(f.bytes,f.len)||xQueueSend(s.tx,&f,0)!=pdTRUE){s.tx_drop++;return ERR_MEM;}
 return ERR_OK;
}
static err_t init(struct netif *n){
 n->name[0]='z';n->name[1]='t';n->hwaddr_len=6;n->mtu=ZT_PORT_MTU;
 n->flags=NETIF_FLAG_BROADCAST|NETIF_FLAG_ETHARP;n->output=etharp_output;n->linkoutput=output;return ERR_OK;
}
/* Always wait for a queued tcpip callback. netifapi is not a queue barrier
 * when ESPHome enables LWIP_TCPIP_CORE_LOCKING. */
struct sync_call {void (*fn)(void *); void *arg; SemaphoreHandle_t done;};
static void sync_callback(void *arg){
 struct sync_call *call=arg;call->fn(call->arg);xSemaphoreGive(call->done);
}
static err_t call_tcpip(void (*fn)(void *),void *arg){
 struct sync_call call={.fn=fn,.arg=arg,.done=xSemaphoreCreateBinary()};
 if(!call.done)return ERR_MEM;
 err_t err=tcpip_callback(sync_callback,&call);
 if(err==ERR_OK)xSemaphoreTake(call.done,portMAX_DELAY);
 vSemaphoreDelete(call.done);return err;
}
static void apply(void *arg){
 if(!s.config.up||!s.link_ok){netif_set_link_down(&s.netif);netif_set_down(&s.netif);return;}
 ip4_addr_t ip={.addr=s.config.ip},mask={.addr=s.config.mask},gw={0};
 ztp_put_mac(s.netif.hwaddr,s.config.mac);
 netif_set_addr(&s.netif,&ip,&mask,&gw);netif_set_up(&s.netif);netif_set_link_up(&s.netif);
}
static int configure(ZT_Node*n,void*u,void*t,uint64_t id,void**nu,
 enum ZT_VirtualNetworkConfigOperation op,const ZT_VirtualNetworkConfig*c){
 ESP_LOGI("zt","network=%016llx operation=%d status=%d",(unsigned long long)id,op,c?(int)c->status:-1);
 s.network_status=c?(int)c->status:-1;
 s.config.up=0;s.pending=1;
 if(op==ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_DOWN||op==ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_DESTROY)return 0;
 if(!c||c->status!=ZT_NETWORK_STATUS_OK)return 0;
 /* Deliberately limited endpoint profile. No silent route/address truncation. */
 if(c->type!=ZT_NETWORK_TYPE_PRIVATE||c->bridge||c->routeCount>ZT_MAX_NETWORK_ROUTES||c->assignedAddressCount!=1||c->mtu<ZT_PORT_MTU)return -1;
 const struct sockaddr_in *a=(const void*)&c->assignedAddresses[0];
 unsigned prefix=ntohs(a->sin_port);
 if(a->sin_family!=AF_INET||prefix<1||prefix>30)return -1;
 esp_netif_ip_info_t eth;if(esp_netif_get_ip_info(s.eth,&eth)!=ESP_OK)return -1;
 uint32_t mask=htonl(ztp_mask(prefix));
 for(unsigned ri=0;ri<c->routeCount;ri++){
  const ZT_VirtualNetworkRoute *r=&c->routes[ri];const struct sockaddr_in *target=(const void*)&r->target;
  if(target->sin_family!=AF_INET||r->via.ss_family!=0||r->flags||
     !ztp_connected_route(ntohl(a->sin_addr.s_addr),prefix,ntohl(target->sin_addr.s_addr),ntohs(target->sin_port))){
   ESP_LOGE("zt","Unsupported managed route %u",ri);return -1;
  }
 }

 /* Reject either direction of subnet overlap; physical default route stays intact. */
 if(eth.ip.addr && ((a->sin_addr.s_addr&eth.netmask.addr)==(eth.ip.addr&eth.netmask.addr)||
    (a->sin_addr.s_addr&mask)==(eth.ip.addr&mask)))return -1;
 char address[INET_ADDRSTRLEN];inet_ntop(AF_INET,&a->sin_addr,address,sizeof address);
 ESP_LOGI("zt","overlay=%s/%u",address,prefix);
 s.config.ip=a->sin_addr.s_addr;s.config.mask=mask;s.config.mac=c->mac;s.config.up=1;
 return 0;
}
static int wire(ZT_Node*n,void*u,void*t,int64_t local,const struct sockaddr_storage*a,
 const void*p,unsigned len,unsigned ttl){
 if(s.fd<0||a->ss_family!=AF_INET)return -1;
 int ok=sendto(s.fd,p,len,0,(const void*)a,sizeof(struct sockaddr_in))==(int)len;
 if(ok)s.wire_tx++;else s.wire_error++;return ok?0:-1;
}
static int path(ZT_Node*n,void*u,void*t,uint64_t peer,int64_t local,const struct sockaddr_storage*a){
 if(a->ss_family!=AF_INET)return 0;
 const struct sockaddr_in *ip=(const void*)a;
 return !s.config.up || (ip->sin_addr.s_addr&s.config.mask)!=(s.config.ip&s.config.mask);
}
static void receive(ZT_Node*n,void*u,void*t,uint64_t id,void**nu,uint64_t src,uint64_t dst,
 unsigned type,unsigned vlan,const void*data,unsigned len){
 s.rx_frames++;
 if(!s.active||vlan||len>ZT_PORT_MTU||(type!=0x800&&type!=0x806)){s.rx_drop++;return;}
 frame_t f;f.len=len+14;ztp_put_mac(f.bytes,dst);ztp_put_mac(f.bytes+6,src);
 f.bytes[12]=type>>8;f.bytes[13]=type;memcpy(f.bytes+14,data,len);
 if(!ztp_frame_valid(f.bytes,f.len)){s.rx_drop++;return;}
 struct pbuf*p=pbuf_alloc(PBUF_RAW,f.len,PBUF_RAM);
 if(!p){s.rx_drop++;return;}
 if(pbuf_take(p,f.bytes,f.len)!=ERR_OK||tcpip_input(p,&s.netif)!=ERR_OK){pbuf_free(p);s.rx_drop++;}
}
static void event(ZT_Node*n,void*u,void*t,enum ZT_Event e,const void*d){if(e==ZT_EVENT_ONLINE)s.online=1;if(e==ZT_EVENT_OFFLINE||e==ZT_EVENT_DOWN)s.online=0;ESP_LOGI("zt","event=%d",e);}

extern void ztp_rng_init(void),ztp_rng_deinit(void),ztp_storage_close(void);
extern esp_err_t ztp_storage_init(void),ztp_storage_error(void);
extern int ztp_crypto_selftest(void);
static void publish(int is_running){
 zerotier_native_status_t v={.node_id=s.node_id,.ipv4=s.active?s.config.ip:0,
  .running=is_running,.online=s.online,.active=s.active,.error=s.error,
  .network_status=s.network_status,
  .wire_tx=s.wire_tx,.wire_rx=s.wire_rx,.rx_frames=s.rx_frames,.tx_frames=s.tx_frames,
  .rx_drop=s.rx_drop,.tx_drop=s.tx_drop,
  .internal_free=heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
  .psram_free=heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
  .stack_free=uxTaskGetStackHighWaterMark(NULL)};
 portENTER_CRITICAL(&status_mux);status=v;portEXIT_CRITICAL(&status_mux);
}
void zerotier_native_get_status(zerotier_native_status_t*out){
 portENTER_CRITICAL(&status_mux);*out=status;portEXIT_CRITICAL(&status_mux);
}
void zerotier_native_stop(void){atomic_store(&stop_requested,1);}
static esp_err_t apply_pending(void){
 s.active=0;
 if(s.subscribed_ip)ZT_Node_multicastUnsubscribe(s.node,s.nwid,0xffffffffffffULL,ntohl(s.subscribed_ip));
 s.subscribed_ip=0;xQueueReset(s.tx);
 if(call_tcpip(apply,NULL)!=ERR_OK)return ESP_ERR_NO_MEM;
 s.active=s.config.up&&s.link_ok;s.pending=0;
 if(s.active){
  ZT_Node_multicastSubscribe(s.node,NULL,s.nwid,0xffffffffffffULL,0);
  ZT_Node_multicastSubscribe(s.node,NULL,s.nwid,0xffffffffffffULL,ntohl(s.config.ip));
  s.subscribed_ip=s.config.ip;
 }
 return ESP_OK;
}
static void run(void*arg){
 int watched=0;int64_t deadline=0,stats_at=0;
 esp_netif_ip_info_t last={0};
 ztp_rng_init();
 if(ztp_crypto_selftest()){s.error=ESP_FAIL;goto cleanup;}
 if(esp_task_wdt_add(NULL)!=ESP_OK){s.error=ESP_ERR_INVALID_STATE;goto cleanup;}
 watched=1;
 struct ZT_Node_Callbacks cb={.statePutFunction=ztp_state_put,.stateGetFunction=ztp_state_get,
 .wirePacketSendFunction=wire,.virtualNetworkFrameFunction=receive,.virtualNetworkConfigFunction=configure,
 .eventCallback=event,.pathCheckFunction=path};
 struct ZT_Node_Config config={0};
 if(netifapi_netif_add(&s.netif,NULL,NULL,NULL,NULL,init,tcpip_input)!=ERR_OK){s.error=ESP_ERR_NO_MEM;goto cleanup;}
 s.added=1;
 if(ZT_Node_new(&s.node,&config,NULL,NULL,&cb,now())!=ZT_RESULT_OK){s.error=ESP_FAIL;goto cleanup;}
 if(ztp_storage_error()!=ESP_OK){s.error=ztp_storage_error();goto cleanup;}
 s.node_id=ZT_Node_address(s.node);
 ESP_LOGI("zt","node=%010llx",(unsigned long long)s.node_id);
 if(ZT_Node_join(s.node,s.nwid,NULL,NULL)!=ZT_RESULT_OK){s.error=ESP_FAIL;goto cleanup;}
 while(!atomic_load(&stop_requested)){
  esp_netif_ip_info_t ip={0};esp_netif_get_ip_info(s.eth,&ip);
  if(!esp_netif_is_netif_up(s.eth))ip.ip.addr=0;
  if(ip.ip.addr!=last.ip.addr||s.fd<0){
   if(s.fd>=0){close(s.fd);s.fd=-1;}
   if(ip.ip.addr){
    s.fd=socket(AF_INET,SOCK_DGRAM,0);
    struct sockaddr_in local={.sin_family=AF_INET,.sin_port=htons(9993),.sin_addr.s_addr=ip.ip.addr};
    struct timeval tv={.tv_usec=20000};
    if(s.fd>=0&&(bind(s.fd,(void*)&local,sizeof local)<0||setsockopt(s.fd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof tv)<0)){close(s.fd);s.fd=-1;}
   }
   last=ip;
  }
  int link_ok=ip.ip.addr && s.fd>=0;
  if(s.config.up && ip.ip.addr &&
     ((s.config.ip&ip.netmask.addr)==(ip.ip.addr&ip.netmask.addr)||
      (s.config.ip&s.config.mask)==(ip.ip.addr&s.config.mask)))link_ok=0;
  if(link_ok!=s.link_ok){s.link_ok=link_ok;s.pending=1;}
  if(s.fd>=0){uint8_t buf[4096];struct sockaddr_storage a={0};socklen_t alen=sizeof a;
   int len=recvfrom(s.fd,buf,sizeof buf,0,(void*)&a,&alen);
   if(len>0){s.wire_rx++;if(fatal(ZT_Node_processWirePacket(s.node,NULL,now(),0,&a,buf,len,&deadline))){s.error=ESP_FAIL;break;}}
  }else vTaskDelay(pdMS_TO_TICKS(20));
  if(now()>=deadline && fatal(ZT_Node_processBackgroundTasks(s.node,NULL,now(),&deadline))){s.error=ESP_FAIL;break;}
  if(s.pending && (s.error=apply_pending())!=ESP_OK)break;
  frame_t f;for(int i=0;i<8&&xQueueReceive(s.tx,&f,0)==pdTRUE;i++){
   if(s.active && fatal(ZT_Node_processVirtualNetworkFrame(s.node,NULL,now(),s.nwid,ztp_mac(f.bytes+6),ztp_mac(f.bytes),
     ((unsigned)f.bytes[12]<<8)|f.bytes[13],0,f.bytes+14,f.len-14,&deadline))){s.error=ESP_FAIL;break;}
  }
  if(s.error!=ESP_OK)break;
  if(ztp_storage_error()!=ESP_OK){s.error=ztp_storage_error();break;}
  publish(1);
  if(now()>=stats_at){stats_at=now()+30000;
   ESP_LOGI("zt","heap_internal=%u psram=%u stack_free=%u tx=%u rx=%u min_internal=%u min_psram=%u",
    (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),(unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
    (unsigned)uxTaskGetStackHighWaterMark(NULL),s.wire_tx,s.wire_rx,
    (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),(unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM));
  }
  esp_task_wdt_reset();vTaskDelay(1);
 }
cleanup:
 s.active=0;s.config.up=0;
 if(s.added){
  /* Barrier drains prior tcpip work before queue deletion. On OOM, retain all
   * resources and yield until tcpip accepts the teardown; never free live netif. */
  while(call_tcpip(apply,NULL)!=ERR_OK ||
        netifapi_netif_remove(&s.netif)!=ERR_OK){
   if(watched){esp_task_wdt_reset();}
   vTaskDelay(pdMS_TO_TICKS(100));
  }
  s.added=0;
 }
 if(s.node){ZT_Node_delete(s.node);s.node=NULL;}
 if(s.fd>=0){close(s.fd);s.fd=-1;}
 if(watched)esp_task_wdt_delete(NULL);
 ztp_rng_deinit();ztp_storage_close();
 vQueueDelete(s.tx);s.tx=NULL;
 publish(0);atomic_store(&running,0);vTaskDelete(NULL);
}
esp_err_t zerotier_native_start(uint64_t id,esp_netif_t*eth){
 if(!eth||!id)return ESP_ERR_INVALID_ARG;
 if(atomic_load(&running))return ESP_ERR_INVALID_STATE;
 if(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)<8*1024*1024)return ESP_ERR_NO_MEM;
 memset(&s,0,sizeof s);s.fd=-1;s.network_status=-1;
 esp_err_t e=ztp_storage_init();if(e!=ESP_OK){ztp_storage_close();return e;}
 s.tx=xQueueCreate(8,sizeof(frame_t));if(!s.tx){ztp_storage_close();return ESP_ERR_NO_MEM;}
 s.eth=eth;s.nwid=id;atomic_store(&stop_requested,0);atomic_store(&running,1);
 portENTER_CRITICAL(&status_mux);memset(&status,0,sizeof status);status.running=true;portEXIT_CRITICAL(&status_mux);
 if(xTaskCreate(run,"zerotier",131072,NULL,4,NULL)!=pdPASS){
  vQueueDelete(s.tx);ztp_storage_close();atomic_store(&running,0);
  portENTER_CRITICAL(&status_mux);status.running=false;status.error=ESP_ERR_NO_MEM;portEXIT_CRITICAL(&status_mux);
  return ESP_ERR_NO_MEM;
 }
 return ESP_OK;
}
