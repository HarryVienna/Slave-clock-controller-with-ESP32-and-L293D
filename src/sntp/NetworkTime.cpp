#include "NetworkTime.h"
#include <cstring>
#include <esp_log.h>

static std::function<void()> globalSyncCallback = nullptr;

NetworkTime::NetworkTime(const char* ntpServer, const char* timeZone)
    : ntpServer(ntpServer), timeZone(timeZone), syncCallback(nullptr) {}

void NetworkTime::init(std::function<void()> syncCallback) {
    this->syncCallback = syncCallback;
    globalSyncCallback = syncCallback; // Statisc variable für SNTP-CB

    // Configure SNTP
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, ntpServer);
    sntp_set_time_sync_notification_cb(onSyncCallback);
    sntp_init();

    // Set timezone
    setenv("TZ", timeZone, 1);
    tzset();
}

// Get the current time
bool NetworkTime::getTime(struct tm& timeInfo) {
    time_t now;
    time(&now);
    if (localtime_r(&now, &timeInfo) == nullptr) {
        ESP_LOGW("NetworkTime", "Zeit konnte nicht abgerufen werden");
        return false;
    }
    return true;
}

// Static Callback
void NetworkTime::onSyncCallback(struct timeval* tv) {
    if (globalSyncCallback) {
        globalSyncCallback(); // Calls the user-defined function
    }
}