#pragma once

#ifndef WIFI_SMART_CONFIG_H
#define WIFI_SMART_CONFIG_H

#include <Arduino.h>
#include "esp_err.h"
#include "esp_event.h"

class WifiSmartConfig {
public:
  /**
   * @brief Enum for the various connection states.
   */
  enum class WifiConnectStatus {
    Connected,
    Disconnected,
    Smartconfig
  };

  WifiSmartConfig(const char* aes_key, const char* hostname, const char* ntp_server,
                 void (*connectionCallback)(WifiConnectStatus status),
                 void (*sntpCallback)(struct timeval *tv));

  ~WifiSmartConfig();

  esp_err_t init();
  esp_err_t connect();
  esp_err_t start();
  esp_err_t stop();
  esp_err_t initSNTP();
  esp_err_t initTimezone();



private:
  static const char* TAG;
  static const int MAXIMUM_RETRY = 10;
  static const char* NVS_NAMESPACE;
  static const char* TIMEZONE_VALUE;

  char* _aes_key;
  char* _hostname;
  char* _ntp_server;

  EventGroupHandle_t _wifi_event_group;
  int _retry_num;
  bool _connected;

  void (*_connectionCallback)(WifiConnectStatus status);
  void (*_sntpCallback)(struct timeval *tv);

  static void connect_event_handler(void* arg, esp_event_base_t event_base, 
                                   int32_t event_id, void* event_data);
  static void handleWifiEvent(WifiSmartConfig* self, int32_t event_id, void* event_data);
  static void handleIpEvent(WifiSmartConfig* self, int32_t event_id, void* event_data);
  static void handleSmartConfigEvent(WifiSmartConfig* self, int32_t event_id, void* event_data);

};

#endif // WIFI_SMART_CONFIG_H