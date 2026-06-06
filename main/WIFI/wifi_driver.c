#include "wifi_driver.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "nvs_flash.h"



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


void sniffer_cb(void *buf, wifi_promiscuous_pkt_type_t type)
{
        
    wifi_promiscuous_pkt_t *pkt=(wifi_promiscuous_pkt_t *)buf;
    //printf("Got packet, RSSI=%d,length=%d\n",pkt->rx_ctrl.rssi, pkt->rx_ctrl.sig_len);
    if(pkt->rx_ctrl.sig_len< sizeof(ieee80211_frame_header_t))
    {
        return;
    }
    ieee80211_frame_header_t *hdr =(ieee80211_frame_header_t *)pkt->payload;

    
    
    uint8_t frame_type = (hdr->frame_ctrl & 0x000C) >>2;
    uint8_t frame_subtype = (hdr->frame_ctrl & 0x00F0) >>4;
    if (frame_type == 0 && frame_subtype == 8)
    {
        // Beacon frame body: Timestamp(8) + BeaconInterval(2) + Capabilities(2), then tagged parameters
        uint8_t *ie = pkt->payload + 36;   // 24 (header) + 8 + 2 + 2 = 36
        int rest = pkt->rx_ctrl.sig_len - 36;
        while (rest >= 2)
        {
            uint8_t id = ie[0];              // Tag Number
            uint8_t tag_len = ie[1];          // Tag Length
            if (id == 0)                       // SSID tag
            {
                printf("Beacon from " MACSTR " ", MAC2STR(hdr->addr2));
                 printf("To: %02X:%02X:%02X:%02X:%02X:%02X, SSID: ",hdr->addr1[0], hdr->addr1[1], hdr->addr1[2],hdr->addr1[3], hdr->addr1[4], hdr->addr1[5]);
                if (tag_len == 0)
                {
                    printf("(hidden)\n");
                }
                else
                {
                    for (int i = 0; i < tag_len; i++)
                        printf("%c", ie[2 + i]);
                    printf("\n");
                }
                break;
            }
            ie += 2 + tag_len;
            rest -= 2 + tag_len;
        }
    }
}
void init_sniffer()
{
    esp_netif_init();
    esp_event_loop_create_default();

    wifi_init_config_t t=WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&t);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    esp_wifi_set_promiscuous_rx_cb(sniffer_cb);//set callbacke function
    esp_wifi_set_promiscuous(true);//start sniffer mode

}