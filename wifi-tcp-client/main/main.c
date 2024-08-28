#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#define HOST_IP_ADDR "192.168.31.100"
#define PORT 8080
#define WIFI_SSID "Xiaomi_4BB2"
#define WIFI_PASS "ayTFvhyk"

static const char *TAG = "tcp_client";

static void tcp_client_task(void *pvParameters) {
    char rx_buffer[128];
    char addr_str[128];
    int addr_family = 0;
    int ip_protocol = 0;

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(HOST_IP_ADDR);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);
    addr_family = AF_INET;
    ip_protocol = IPPROTO_IP;

    int sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to connect: errno %d", errno);
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Server connection successfull");

    while (1) {
        int err = send(sock, "Hello Server", strlen("Hello Server"), 0);
        if (err < 0) {
            ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
            break;
        }

        int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if (len < 0) {
            ESP_LOGE(TAG, "recv failed: errno %d", errno);
            break;
        } else {
            rx_buffer[len] = 0;
            ESP_LOGI(TAG, "Received %d bytes from server: %s", len, rx_buffer);
        }

        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

    close(sock);
    vTaskDelete(NULL);
}

void wifi_init_sta(void) {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    esp_wifi_start();

    esp_wifi_connect();
}

void app_main(void) {
    nvs_flash_init();
    wifi_init_sta();
    vTaskDelay(pdMS_TO_TICKS(2000));
    xTaskCreate(tcp_client_task, "tcp_client", 4096, NULL, 5, NULL);
}
