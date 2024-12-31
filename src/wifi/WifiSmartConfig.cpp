#include "WifiSmartConfig.h"

#include <string.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_wifi.h"

#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_smartconfig.h"

#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

const char* WifiSmartConfig::TAG = "wifi_smartconfig";
const char* WifiSmartConfig::NVS_NAMESPACE = "WIFI";
const char* WifiSmartConfig::TIMEZONE_VALUE = "TZ";

// Definieren der Event-Group Bits
const int WIFI_CONNECTED_BIT = BIT0;
const int WIFI_FAIL_BIT      = BIT1;
const int ESPTOUCH_DONE_BIT  = BIT2;


WifiSmartConfig::WifiSmartConfig(const char* aes_key, const char* hostname, 
                                 const char* ntp_server,
                                 void (*connectionCallback)(WifiConnectStatus status),
                                 void (*sntpCallback)(struct timeval *tv)) 
  : _aes_key(strdup(aes_key)),  
    _hostname(strdup(hostname)),
    _ntp_server(strdup(ntp_server)), 
    _connectionCallback(connectionCallback), 
    _sntpCallback(sntpCallback) { 

}

WifiSmartConfig::~WifiSmartConfig() {
  free(_aes_key);
  free(_hostname);
  free(_ntp_server);
  if (_wifi_event_group != NULL) {
    vEventGroupDelete(_wifi_event_group);
  }
}

esp_err_t WifiSmartConfig::init() {
  esp_err_t ret;

  // Initialize NVS
  ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to init nvs flash");
      return ret; 
    }
  }

  // Create a new event group.
  _wifi_event_group = xEventGroupCreate();
  if (_wifi_event_group == NULL) {
    ESP_LOGE(TAG, "Failed to create event group");
    return ESP_FAIL;
  }

  // Create default event loop
  ret = esp_event_loop_create_default();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set create event loop");
    vEventGroupDelete(_wifi_event_group);
    return ret;      
  }

  // Initialize the underlying TCP/IP stack
  ret = esp_netif_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to init netif");
    vEventGroupDelete(_wifi_event_group);
    return ret;    
  }

  // Creates default WIFI ST
  esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
  if (sta_netif == NULL) {
    ESP_LOGE(TAG, "Failed to create default wifi station");
    vEventGroupDelete(_wifi_event_group);
    return ESP_FAIL;
  }

  // Set hostname
  ret = esp_netif_set_hostname(sta_netif, _hostname);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set hostname");
    vEventGroupDelete(_wifi_event_group);
    return ret;
  }

  // Init WiFi
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ret = esp_wifi_init(&cfg);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to init wifi");
    vEventGroupDelete(_wifi_event_group);
    return ret;
  }
  
  // Set the WiFi operating mode
  ret = esp_wifi_set_mode(WIFI_MODE_STA);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set mode");
    vEventGroupDelete(_wifi_event_group);
    return ret;
  }

  // Set storage to flash
  ret = esp_wifi_set_storage(WIFI_STORAGE_FLASH);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set storage to flash");
    vEventGroupDelete(_wifi_event_group);
    return ret;
  }

  // Register event handler
  ret = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &connect_event_handler, this);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register handler");
    vEventGroupDelete(_wifi_event_group);
    return ret;
  }
  ret = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_event_handler, this);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register handler");
    vEventGroupDelete(_wifi_event_group);
    return ret;
  }
  ret = esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &connect_event_handler, this);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register handler");
    vEventGroupDelete(_wifi_event_group);
    return ret;
  }

  return ESP_OK;
}


esp_err_t WifiSmartConfig::connect() {
    EventBits_t bits;

    wifi_config_t wifi_config;

    esp_err_t ret = esp_wifi_get_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get config");
    return ret;
    }
    if (strlen((const char*)wifi_config.sta.ssid)) {
        ESP_LOGI(TAG, "Flash SSID: %s", wifi_config.sta.ssid);
        ESP_LOGI(TAG, "Flash Password: %s", wifi_config.sta.password);
    } else {
        ESP_LOGE(TAG, "Nothing in flash");
    }

    _connected = false;

    /* -------------- Try to connect with stored settings ------------- */
    _retry_num = 0;

    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start wifi");
        return ret;
    }

    ESP_LOGI(TAG, "WIFI startet. Waiting for events.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) 
     * or connection failed for the maximum number of re-tries (WIFI_FAIL_BIT). 
     * The bits are set by event_handler() (see above) */
    bits = xEventGroupWaitBits(_wifi_event_group,
                            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                            pdTRUE,
                            pdFALSE,
                            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to ap SSID: %s password: %s", wifi_config.sta.ssid, wifi_config.sta.password);
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID: %s, password:%s", wifi_config.sta.ssid, wifi_config.sta.password);
    } else {
        ESP_LOGE(TAG, "Unexpected event");
    }

    /* -------------- Try to connect with smartconfig ------------- */
    _retry_num = 0;

    ret = esp_smartconfig_set_type(SC_TYPE_ESPTOUCH_V2);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set smartconfig type");
        return ret;
    }
    smartconfig_start_config_t smart_cfg;
    memset(&smart_cfg, 0, sizeof(smartconfig_start_config_t)); // Ensure that the struct is initialized
    smart_cfg.enable_log = false;
    smart_cfg.esp_touch_v2_enable_crypt = true;
    smart_cfg.esp_touch_v2_key = _aes_key;

    _connectionCallback(WifiConnectStatus::Smartconfig);

    ret = esp_smartconfig_start(&smart_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start smartconfig");
        return ret;
    }

    do {
        bits = xEventGroupWaitBits(_wifi_event_group,
                                    WIFI_CONNECTED_BIT | WIFI_FAIL_BIT | ESPTOUCH_DONE_BIT,
                                    pdTRUE,
                                    pdFALSE,
                                    portMAX_DELAY);

        if (bits & WIFI_CONNECTED_BIT) {
            ESP_LOGI(TAG, "connected via smart config");
        } else if (bits & ESPTOUCH_DONE_BIT) {
            ESP_LOGI(TAG, "Touch done");
            esp_smartconfig_stop();
            return ESP_OK;
        } else if (bits & WIFI_FAIL_BIT) {
            ESP_LOGI(TAG, "Failed to connect via smart config");
            esp_smartconfig_stop();
            esp_wifi_stop();
            return ESP_FAIL;
        } else {
            ESP_LOGE(TAG, "Unexpected event");
            esp_smartconfig_stop();
            esp_wifi_stop();
            return ESP_FAIL;
        }
    } while (true);
}

esp_err_t WifiSmartConfig::start() {
  esp_err_t ret = esp_wifi_start();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start wifi");
  }
  return ret;
}

esp_err_t WifiSmartConfig::stop() {
  esp_err_t ret = esp_wifi_stop();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to stop wifi");
  }
  return ret;
}
void WifiSmartConfig::connect_event_handler(void* arg, esp_event_base_t event_base,
                                            int32_t event_id, void* event_data) {
  WifiSmartConfig* self = static_cast<WifiSmartConfig*>(arg);

  if (event_base == WIFI_EVENT) { 
    handleWifiEvent(self, event_id, event_data);
  } else if (event_base == IP_EVENT) {
    handleIpEvent(self, event_id, event_data);
  } else if (event_base == SC_EVENT) {
    handleSmartConfigEvent(self, event_id, event_data);
  } 
}

void WifiSmartConfig::handleWifiEvent(WifiSmartConfig* self, int32_t event_id, void* event_data) {
  switch (event_id) {
    case WIFI_EVENT_STA_START:
      ESP_LOGI(self->TAG, "WIFI_EVENT_STA_START");
      if (esp_wifi_connect() != ESP_OK) {
        ESP_LOGE(self->TAG, "Could not connect");
        esp_wifi_disconnect();
        xEventGroupSetBits(self->_wifi_event_group, WIFI_FAIL_BIT);
      }
      break;
    case WIFI_EVENT_STA_STOP:
      ESP_LOGI(self->TAG, "WIFI_EVENT_STA_STOP");
      break;
    case WIFI_EVENT_STA_CONNECTED:
      ESP_LOGI(self->TAG, "WIFI_EVENT_STA_CONNECTED");
      self->_connectionCallback(WifiConnectStatus::Connected);
      self->_connected = true;
      break;
    case WIFI_EVENT_STA_DISCONNECTED: {
      ESP_LOGI(self->TAG, "WIFI_EVENT_STA_DISCONNECTED");
      self->_connectionCallback(WifiConnectStatus::Disconnected);
      if (self->_connected) {
        // WIFI was already connected. Perhaps router down? Try reconnecting 
        ESP_LOGI(self->TAG, "Already connected. Retry to connect to the AP");
        vTaskDelay((1000 * 60) / portTICK_PERIOD_MS); // Wait 1 minute
        esp_wifi_connect();
      } else {
        // WIFI was not connected. So there is a problem
        if (self->_retry_num < self->MAXIMUM_RETRY) {
          ESP_LOGI(self->TAG, "Cannot connect. Retry to connect to the AP");
          esp_wifi_connect();
          self->_retry_num++;
        } else {
          xEventGroupSetBits(self->_wifi_event_group, WIFI_FAIL_BIT);
        }
      }
      ESP_LOGI(self->TAG, "connect to the AP fail");
      break;
    }
    default:
      break;
  }
}

void WifiSmartConfig::handleIpEvent(WifiSmartConfig* self, int32_t event_id, void* event_data) {
  if (event_id == IP_EVENT_STA_GOT_IP) {
    ESP_LOGI(self->TAG, "IP_EVENT_STA_GOT_IP");
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    ESP_LOGI(self->TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    self->_retry_num = 0;
    xEventGroupSetBits(self->_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

void WifiSmartConfig::handleSmartConfigEvent(WifiSmartConfig* self, int32_t event_id, void* event_data) {
  switch (event_id) {
    case SC_EVENT_SCAN_DONE:
      ESP_LOGI(self->TAG, "SC_EVENT_SCAN_DONE");
      break;
    case SC_EVENT_FOUND_CHANNEL:
      ESP_LOGI(self->TAG, "SC_EVENT_FOUND_CHANNEL");
      break;
    case SC_EVENT_GOT_SSID_PSWD: {
      ESP_LOGI(self->TAG, "SC_EVENT_GOT_SSID_PSWD");

      smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
      wifi_config_t wifi_config;
      uint8_t rvd_data[65] = { 0 };

      bzero(&wifi_config, sizeof(wifi_config_t));
      memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
      memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));
      wifi_config.sta.bssid_set = evt->bssid_set;
      if (wifi_config.sta.bssid_set == true) {
        memcpy(wifi_config.sta.bssid, evt->bssid, sizeof(wifi_config.sta.bssid));
      }

      ESP_LOGI(self->TAG, "SSID: %s", wifi_config.sta.ssid);
      ESP_LOGI(self->TAG, "PASSWORD: %s", wifi_config.sta.password);

      ESP_ERROR_CHECK( esp_smartconfig_get_rvd_data(rvd_data, sizeof(rvd_data)) );
      ESP_LOGI(self->TAG, "RVD_DATA: %s", rvd_data);

      nvs_handle_t my_handle;
      ESP_ERROR_CHECK(nvs_open(self->NVS_NAMESPACE, NVS_READWRITE, &my_handle));
      ESP_ERROR_CHECK(nvs_set_str(my_handle, self->TIMEZONE_VALUE, (char*)rvd_data));
      ESP_ERROR_CHECK(nvs_commit(my_handle));
      nvs_close(my_handle);

      ESP_ERROR_CHECK( esp_wifi_disconnect() );
      ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
      if (esp_wifi_connect() != ESP_OK) {
        ESP_LOGE(self->TAG, "Could not connect");
        esp_wifi_disconnect();
        xEventGroupSetBits(self->_wifi_event_group, WIFI_FAIL_BIT);
      }
      break;
    }
    case SC_EVENT_SEND_ACK_DONE:
      ESP_LOGI(self->TAG, "SC_EVENT_SEND_ACK_DONE");
      xEventGroupSetBits(self->_wifi_event_group, ESPTOUCH_DONE_BIT);
      break;
    default:
      break;
  }
}

esp_err_t WifiSmartConfig::initSNTP() {
    ESP_LOGI(TAG, "Init SNTP");

    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, _ntp_server);
    sntp_set_time_sync_notification_cb(_sntpCallback); 

    sntp_init();

    return ESP_OK;
}

esp_err_t WifiSmartConfig::initTimezone() {
  esp_err_t err;

  ESP_LOGI(TAG, "Init timezone");

  char *timezone_value;
  size_t timezone_size = 0;

  nvs_handle_t my_handle;
  err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &my_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to open NVS %d", err);
    return ESP_FAIL;
  }
  err = nvs_get_str(my_handle, TIMEZONE_VALUE, NULL, &timezone_size);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to read size from NVS %d", err);
    return ESP_FAIL;
  }
  timezone_value = (char*)malloc(timezone_size);
  err = nvs_get_str(my_handle, TIMEZONE_VALUE, timezone_value, &timezone_size);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to read value from NVS %d", err);
    free(timezone_value); // Release memory because the value could not be read
    return ESP_FAIL;
  }
  nvs_close(my_handle);

  ESP_LOGI(TAG, "NVS value %s", timezone_value);
  ESP_LOGI(TAG, "NVS size %d", timezone_size);

  // Set timezone
  setenv(TIMEZONE_VALUE, timezone_value, 1);
  tzset();

  free(timezone_value); // Release memory after the value has been used
  
  return ESP_OK;
}