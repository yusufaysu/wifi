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
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#define PORT 8080
#define WIFI_SSID "OFIS_WIFI_SSID"
#define WIFI_PASS "OFIS_WIFI_PASSWORD"
#define STATIC_IP "192.168.1.100"
#define GATEWAY_IP "192.168.1.1"
#define SUBNET_MASK "255.255.255.0"
#define DNS_IP "8.8.8.8"

static const char *TAG = "tcp_server";

static void tcp_server_task(void *pvParameters) {
    char rx_buffer[128]; // gelen veri
    char addr_str[128]; // ip addr
    int addr_family;
    int ip_protocol;

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);
    addr_family = AF_INET;
    ip_protocol = IPPROTO_IP;

    // socket oluştu
    int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    // socketi ip adresine bind ettik
    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    // socketi dinlemeye başladı ama sıraya max 1 tane alır
    err = listen(listen_sock, 1);
    if (err != 0) {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    while (1) {
        ESP_LOGI(TAG, "Server listening on port %d", PORT);

        // bağlantı bulursa kabul et
        struct sockaddr_in source_addr;// bağlanan cihazın adres bilgileri
        uint addr_len = sizeof(source_addr);
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;
        }

        inet_ntoa_r(source_addr.sin_addr, addr_str, sizeof(addr_str) - 1);
        ESP_LOGI(TAG, "Socket accepted ip address: %s", addr_str);

        // gelen veriyi okuma
        int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if (len < 0) {
            ESP_LOGE(TAG, "recv failed: errno %d", errno);
            break;
        } else {
            rx_buffer[len] = 0;  // gelen veriyi stringe cevirdip ve sonuna null terminate 
            ESP_LOGI(TAG, "Received %d bytes: %s", len, rx_buffer);
        }

        close(sock);
    }

    close(listen_sock);
    vTaskDelete(NULL);
}

void wifi_init_sta(void) {
    esp_netif_init();
    esp_event_loop_create_default();

    esp_netif_t *netif = esp_netif_create_default_wifi_sta();

    // statik ip konfigurasyonları
    esp_netif_ip_info_t ip_info;
    inet_pton(AF_INET, STATIC_IP, &ip_info.ip);
    inet_pton(AF_INET, GATEWAY_IP, &ip_info.gw);
    inet_pton(AF_INET, SUBNET_MASK, &ip_info.netmask);
    esp_netif_dhcpc_stop(netif); // dhvp durdur
    esp_netif_set_ip_info(netif, &ip_info); // statip ip yap

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
    xTaskCreate(tcp_server_task, "tcp_server", 4096, NULL, 5, NULL);
}