#ifndef NETWORK_TIME_H
#define NETWORK_TIME_H

#include <esp_sntp.h>
#include <ctime>
#include <functional>

class NetworkTime {
public:
    // Constructor
    NetworkTime(const char* ntpServer, const char* timeZone);

    // Initialization of SNTP synchronization
    void init(std::function<void()> syncCallback = nullptr);

    // Call up the current time
    bool getTime(struct tm& timeInfo);

private:
    const char* ntpServer;
    const char* timeZone;
    std::function<void()> syncCallback;

    // Internal callback wrapping for SNTP
    static void onSyncCallback(struct timeval* tv);
};

#endif // NETWORK_TIME_H