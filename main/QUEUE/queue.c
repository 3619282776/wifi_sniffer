#include "QUEUE/queue.h"
#include "WIFI/wifi_driver.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "string.h"
#include "esp_mac.h"
uint8_t ap_list[32][6];
uint16_t ap_count=0;

void ap_append(uint8_t temp[6])
{
    if(ap_count==32)return;
    for (int i =0; i<ap_count;i++)
    {
        if(memcmp(ap_list[i],temp,6)==0){printf("this mac already exit\n"); return;}
    }
    memcpy(ap_list[ap_count],temp,6);
    ap_count++;
    printf("fing ap mac is "MACSTR" \n",MAC2STR(temp));
}


void recive_info(void *args)
{
    uint8_t temp[6];
    while(1)
    {
    if(xQueueReceive(xQueue_ap_info,temp,portMAX_DELAY)==pdPASS)
    ap_append(temp);
    }
}