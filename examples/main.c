/*
        FIRST YOU MUST ADD "#define CONFIG_LWIP_SO_RCVBUF 1" TO sdkconfig.h FILE
*/

#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

//MQTT
#include "esp_lwmqtt.h"
#include "esp_mqtt.h"

#define CONFIG_MQTT_HOST "HOSTNAME"
#define CONFIG_MQTT_PORT "1883"
#define CONFIG_MQTT_RW_BUFFER_SIZE 512
#define CONFIG_MQTT_USERNAME NULL
#define CONFIG_MQTT_PASSWORD NULL
#define CONFIG_MQTT_CLIENT_ID NULL
#define COMMAND_TIMEOUT 5000

char payload[] = "Hello ESP MQTT";
char *topic = "/test_mqtt";
int QoS = 1;
//MQTT

//WIFI
#define EXAMPLE_ESP_WIFI_SSID "WIFI_SSID"
#define EXAMPLE_ESP_WIFI_PASS "WIFI_PASS"

static EventGroupHandle_t wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;
//WIFI

static const char TAG_WiFi[] = "WiFi";
static const char TAG_Main[] = "Main";
static const char TAG_MQTT[] = "MQTT";

//----- MQTT START-----
static void mqtt_message_callback(const char *topic, uint8_t *payload, size_t len)
{
    ESP_LOGI(TAG_MQTT, "incoming: %s => %s (%d)\n", topic, payload, (int)len);
}

static void mqtt_status_callback(esp_mqtt_status_t event)
{
    switch (event)
    {
    case ESP_MQTT_STATUS_CONNECTED:
        ESP_LOGI(TAG_MQTT, "MQTT CONNECTED\n");
        esp_mqtt_subscribe(topic, QoS);
        break;
    case ESP_MQTT_STATUS_DISCONNECTED:
        ESP_LOGI(TAG_MQTT, "MQTT DISCONNECTED\n");
        esp_mqtt_unsubscribe(topic);
        esp_mqtt_start(CONFIG_MQTT_HOST, CONFIG_MQTT_PORT, CONFIG_MQTT_CLIENT_ID, CONFIG_MQTT_USERNAME, CONFIG_MQTT_PASSWORD);
        break;
    default:
        break;
    }
}
//----- MQTT END-----

//----- WIFI START-----
static esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id)
    {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG_WiFi, "got ip:%s",
                 ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        esp_mqtt_start(CONFIG_MQTT_HOST, CONFIG_MQTT_PORT, CONFIG_MQTT_CLIENT_ID, CONFIG_MQTT_USERNAME, CONFIG_MQTT_PASSWORD);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        esp_mqtt_stop();
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
        break;
    default:
        break;
    }

    return ESP_OK;
}

void wifi_init_sta()
{
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS},
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG_WiFi, "wifi_init_sta finished.");
    ESP_LOGI(TAG_WiFi, "connect to ap SSID:%s password:%s",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
}
//----- WIFI END-----

void app_main()
{
    ESP_LOGI(TAG_Main, "[APP] Startup..");
    ESP_LOGI(TAG_Main, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG_Main, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);

    wifi_event_group = xEventGroupCreate();

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_mqtt_init(mqtt_status_callback, mqtt_message_callback, CONFIG_MQTT_RW_BUFFER_SIZE, COMMAND_TIMEOUT);
    wifi_init_sta();
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, false, false, portMAX_DELAY);

    while (true)
    {
        esp_mqtt_publish(topic, (uint8_t *)payload, sizeof(payload), QoS, false);
        ESP_LOGI(TAG_Main, "We sand message and Wait 3 seconds\n");
        vTaskDelay(3000 / portTICK_RATE_MS);
    }
}