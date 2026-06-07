#ifndef __WIFI_DRIVER_
#define __WIFI_DRIVER_
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdint.h>
extern QueueHandle_t xQueue_ap_info;
void send_deauth_1(const uint8_t *target_mac, const uint8_t *ap_mac, uint8_t channel);

void init_sniffer();

#endif