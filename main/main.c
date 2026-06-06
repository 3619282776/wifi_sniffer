#include <stdio.h>
#include <WIFI/wifi_driver.h>
#include "nvs_flash.h"
void app_main(void)
{
    nvs_flash_init();
    init_sniffer();
}
