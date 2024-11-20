#include <TFT_eSPI.h>
#include <SPI.h>
#include <WiFi.h>
#include <time.h>

#include "sntp/NetworkTime.h"
#include "buttons/ButtonHandler.h"


// Farben definieren
#define RED TFT_RED
#define ORANGE TFT_ORANGE
#define GREEN TFT_GREEN

#define BUTTON_MOVE_PIN  GPIO_NUM_0
#define BUTTON_START_PIN GPIO_NUM_35

#define PULSE_WIDTH_MS    350  // Pulse duration in milliseconds
#define PULSE_INTERVAL_MS 150  // Time between pulses
#define PULSE_GPIO_ENABLE GPIO_NUM_25 // Pin for Enable of LM293D
#define PULSE_GPIO_INPUT1 GPIO_NUM_26 // Pin for Input1 of LM293D
#define PULSE_GPIO_INPUT2 GPIO_NUM_27 // Pin for Input2 of LM293D



const char* ssid       = "SSID";     // Change to your SSID
const char* password   = "PASSWORD"; // Change to your WIFI password
const char* timezone   = "CET-1CEST,M3.5.0,M10.5.0/3";
const char* ntpserver  = "pool.ntp.org";
const char* hostname   = "ESP32-Nebenuhr";

bool timeSynced = false;    // Status of time-synchronisation
bool wifiConnected = false; // Status of WiFi connection


// Inits objects
TFT_eSPI tft = TFT_eSPI();
NetworkTime networkTime(ntpserver, timezone);
ButtonHandler buttons(BUTTON_MOVE_PIN, BUTTON_START_PIN);


// Prototype for tasks
void displayTimeTask(void *param);


void printInfo() {
  
  // Board Name und Typ
  Serial.println("ESP32 Information:");
  Serial.print("Board Name: ");
  Serial.println(ARDUINO_BOARD);  // Gibt den Namen des verwendeten Boards aus

  // Prozessor- und Taktfrequenzinformationen
  Serial.print("CPU Frequency: ");
  Serial.print(ESP.getCpuFreqMHz());
  Serial.println(" MHz");

  // RAM-Informationen
  Serial.print("Free heap memory: ");
  Serial.print(ESP.getFreeHeap());
  Serial.println(" bytes");

  // MAC-Adresse des ESP32
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());

  // SDK-Version
  Serial.print("ESP32 SDK Version: ");
  Serial.println(ESP.getSdkVersion());

  // Flash-Größe
  Serial.print("Flash Size: ");
  Serial.print(ESP.getFlashChipSize() / (1024 * 1024));  // Größe in MB
  Serial.println(" MB");

  // Flash-Geschwindigkeit
  Serial.print("Flash Speed: ");
  Serial.print(ESP.getFlashChipSpeed() / 1000000);  // Geschwindigkeit in MHz
  Serial.println(" MHz");

  // PSRAM
  Serial.print("PSRAM Size: ");
  Serial.print(ESP.getPsramSize() / (1024 * 1024));  // Größe in MB
  Serial.println(" MB");
}

void updateDisplayStatus() {
  if (!wifiConnected) {
    tft.fillRect(0, 0, tft.width(), 20, RED);    // Red for no WiFi
  } else if (wifiConnected && !timeSynced) {
    tft.fillRect(0, 0, tft.width(), 20, ORANGE); // Orange for no SNTP-Sync
  } else if (wifiConnected && timeSynced) {
    tft.fillRect(0, 0, tft.width(), 20, GREEN);  // Green for connected
  }
}


// Callback for SNTP sync
void timeSyncCallback() {

  Serial.println(" sync_callback");

  timeSynced = true;
  updateDisplayStatus();
}

// Callback for connected event
void wiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info){
  Serial.println("Connected to AP successfully!");
  wifiConnected = true;
  
  updateDisplayStatus();
}

// Callback for disconnected event
void wiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info){
  Serial.println("Disconnected from WiFi access point");
  Serial.print("WiFi lost connection. Reason: ");
  Serial.println(info.wifi_sta_disconnected.reason);

  wifiConnected = false;
  timeSynced = false;
  
  updateDisplayStatus();

  Serial.println("Trying to Reconnect");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
      vTaskDelay(pdMS_TO_TICKS(500));
      Serial.print(".");
  }
}

// Function to send multiple pulses with delays to LM293D
void sendPulses(uint16_t count) {
  Serial.printf("Pulses %d \n", count);

  static int level = 0;

  for (uint16_t i = 0; i < count; i++) {
    // direction of current
    digitalWrite(PULSE_GPIO_INPUT1, level);
    digitalWrite(PULSE_GPIO_INPUT2, !level);

    // Set the GPIO high to start the pulse
    digitalWrite(PULSE_GPIO_ENABLE, HIGH);
    vTaskDelay(pdMS_TO_TICKS(PULSE_WIDTH_MS));
    
    // Set the GPIO low to end the pulse
    digitalWrite(PULSE_GPIO_ENABLE, LOW);
    vTaskDelay(pdMS_TO_TICKS(PULSE_INTERVAL_MS));  

    level = !level;
  }
}

// Function to send one pulse
void sendPulse() {

  Serial.println("Move hand");

  sendPulses(1);
}

void setup(void) {
  Serial.begin(115200);
  while (!Serial); 

  printInfo();

  // Init pins
  pinMode(PULSE_GPIO_ENABLE, OUTPUT);
  pinMode(PULSE_GPIO_INPUT1, OUTPUT);
  pinMode(PULSE_GPIO_INPUT2, OUTPUT);

  // Init display
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);


  // Info text
  tft.drawString("Move the hands to", 0, 30);
  tft.drawString("12 o'clock position.", 0, 60);
  tft.drawString("Then press Start", 0, 90);

  
  Serial.println("Start Setup");
  buttons.setMoveCallback(sendPulse); // Callback for moving the handles
  buttons.start(); // Blocking loop to set the hands
  
  // Setting of hands is done. Clear screen and proceed
  tft.fillScreen(TFT_BLACK);


  // Init WIFI
  WiFi.onEvent(wiFiStationConnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
  WiFi.onEvent(wiFiStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  Serial.printf("Connecting to %s ", ssid);

  WiFi.setHostname(hostname);
  WiFi.disconnect();
  WiFi.begin(ssid, password);


  // SNTP init with Callback
  networkTime.init(timeSyncCallback);

  // Create task
  xTaskCreatePinnedToCore(displayTimeTask, "DisplayTime", 4096, NULL, 1, NULL, 1);

}

void loop() {
  if (timeSynced) {

    struct tm timeinfo;
    networkTime.getTime(timeinfo); // Get the current time

    static int16_t clock_minutes = 0; // Variable to track the clock's current minute position
    uint16_t current_minutes = (timeinfo.tm_hour % 12) * 60 + timeinfo.tm_min;  // Calculate current minutes on the 12-hour clock
    int16_t difference = current_minutes - clock_minutes; // Calculate the difference in minutes

    // Handle negative differences (e.g., crossing midnight or wrapping around the 12-hour format)
    if (difference < 0) {
        difference += 12 * 60; // Add 12 hours in minutes. Time always goes forward :-)
    }

    // Update clock_minutes to the current position after calculation
    if (difference != 0) {
      clock_minutes = current_minutes;
      
      // Move hands
      sendPulses(difference);
    }
  }

  vTaskDelay(pdMS_TO_TICKS(1000));
}


// Task: Show time on the display
void displayTimeTask(void *param) {
  struct tm timeinfo;

  while (true) {
    // Get the current time
    networkTime.getTime(timeinfo);

    // Time in format HH:MM:SS 
    char timeStr[9]; // Space for "HH:MM:SS"
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);

    // Show the time on the display
    tft.setTextDatum(MC_DATUM);
    tft.drawString(timeStr, tft.width() / 2, tft.height() / 2, 4);

    vTaskDelay(pdMS_TO_TICKS(1000)); // Wait a second
  }
}

