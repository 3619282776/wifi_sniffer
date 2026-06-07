#include <stdio.h>
#include <WIFI/wifi_driver.h>
#include "nvs_flash.h"
#include "esp_timer.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "QUEUE/queue.h"


void app_main(void)
{
    nvs_flash_init();
    init_sniffer();
    xTaskCreate(recive_info,"receive_ap_info",2048,NULL,1,NULL);
}
