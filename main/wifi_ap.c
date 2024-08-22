#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include "rest_server.h"

const char *web_base_path = "/www";
const char *csv_base_path = "/csvfiles";

static const char *TAG = "wifi softAP";

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_AP_STACONNECTED:
            ESP_LOGI(TAG, "Station connected");
            if (server == NULL)
            {
                ESP_LOGI(TAG, "Starting HTTP server");
                server = start_rest_server(web_base_path, csv_base_path, &rest_context);
            }
            break;
        case WIFI_EVENT_AP_STADISCONNECTED:
            ESP_LOGI(TAG, "Station disconnected");
            if (server != NULL)
            {
                ESP_LOGI(TAG, "Stopping HTTP server");
                httpd_stop(server);
                server = NULL;
                if (rest_context != NULL)
                {
                    free(rest_context);
                    rest_context = NULL;
                }
            }
            break;
        default:
            break;
        }
    }
}

void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = CONFIG_ESP_WIFI_SSID,
            .ssid_len = strlen(CONFIG_ESP_WIFI_SSID),
            .channel = CONFIG_ESP_WIFI_CHANNEL,
            .password = CONFIG_ESP_WIFI_PASSWORD,
            .max_connection = CONFIG_ESP_MAX_STA_CONN,
#ifdef CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT
            .authmode = WIFI_AUTH_WPA3_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
#else /* CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT */
            .authmode = WIFI_AUTH_WPA2_PSK,
#endif
            .pmf_cfg = {
                .required = true,
            },
        },
    };
    if (strlen(CONFIG_ESP_WIFI_PASSWORD) == 0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD, CONFIG_ESP_WIFI_CHANNEL);
}

// this also stops the server
void wifi_stop_softap(void)
{
    if (server != NULL)
    {
        httpd_stop(server);
        server = NULL;
        if (rest_context != NULL)
        {
            free(rest_context);
            rest_context = NULL;
        }
    }
    ESP_ERROR_CHECK(esp_wifi_stop());
}
