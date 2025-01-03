#include <TFT_eSPI.h>
#include <SPI.h>
#include <time.h>
#include <mutex>

#include "wifi/WifiSmartConfig.h"
#include "buttons/ButtonHandler.h"

#define TAG "SLAVECLOCK"

// Define the hour value for the clock. Either 12 or 24.
// If it is a 24-hour clock, the hour value is calculated modulo 24, 
// which in fact does not change anything. 
// For a 12-hour clock, the hours 12-23 are used to calculate the value 0-11
#define CLOCK_HOURS 24

// Define colors
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

#define PWM_CHANNEL 0    // PWM channel
#define PWM_FREQ 100     // 100 Hz
#define PWM_RESOLUTION 8 // 8 bits, 0-255 
#define PWM_DUTY 20      // Brightness


const char* ntpserver  = "pool.ntp.org";
const char* hostname   = "ESP32-Nebenuhr";
const char* aes_key    = "ESP32-AES-PHRASE"; 

std::mutex tftMutex;

bool timeSynced    = false; // Status of time-synchronisation
WifiSmartConfig::WifiConnectStatus wifiConnected = WifiSmartConfig::WifiConnectStatus::Disconnected; // Status of WiFi connection

// Prototype for tasks
void displayTimeTask(void *param);
void moveHandsTask(void *param);

// Prototype for callbacks
void connectionCallback(WifiSmartConfig::WifiConnectStatus status);
void timeSyncCallback(struct timeval *tv);


// Init objects
TFT_eSPI tft = TFT_eSPI();
ButtonHandler buttons(BUTTON_MOVE_PIN, BUTTON_START_PIN);
WifiSmartConfig wifi(aes_key, hostname, ntpserver, connectionCallback, timeSyncCallback);




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

  tftMutex.lock();

  if (wifiConnected == WifiSmartConfig::WifiConnectStatus::Disconnected) {
    tft.fillRect(0, 0, tft.width() / 2 - 1, 20, RED);    // Red for no WiFi
  } else if (wifiConnected == WifiSmartConfig::WifiConnectStatus::Smartconfig) {
    tft.fillRect(0, 0, tft.width() / 2 - 1, 20, ORANGE); // Orange for Smartconfig
  } else if (wifiConnected == WifiSmartConfig::WifiConnectStatus::Connected) {
    tft.fillRect(0, 0, tft.width() / 2 - 1, 20, GREEN);  // Green for connected
  }

  if (timeSynced) {
    tft.fillRect(tft.width() / 2 + 1, 0, tft.width(), 20, GREEN);
  } else {
    tft.fillRect(tft.width() / 2 + 1, 0, tft.width(), 20, RED);
  }

  tftMutex.unlock(); 
}


/// Get the current time
bool getTime(struct tm& timeInfo) {
  time_t now;
  time(&now);
  if (localtime_r(&now, &timeInfo) == nullptr) {
      ESP_LOGW(TAG, "Zeit konnte nicht abgerufen werden");
      return false;
  }
  ESP_LOGI(TAG, "Zeit ok");
  return true;
}

void connectionCallback(WifiSmartConfig::WifiConnectStatus status) {
  ESP_LOGI(TAG, "Connection status: %d", status);

  wifiConnected = status;
  updateDisplayStatus();
}

void timeSyncCallback(struct timeval *tv) {
   ESP_LOGI(TAG, "Zeit synchronisiert: %s", ctime(&tv->tv_sec));

  timeSynced = true;
  updateDisplayStatus();
}

// Function to send multiple pulses with delays to LM293D
void sendPulses(uint16_t count) {
  ESP_LOGI(TAG, "Pulses %d ", count);

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

  ESP_LOGI(TAG, "Move hand");

  sendPulses(1);
}

void setup(void) {

  setCpuFrequencyMhz(80);

  Serial.begin(115200);
  while (!Serial){
    delay(500);
  } 

  printInfo();

  // Init pins
  pinMode(PULSE_GPIO_ENABLE, OUTPUT);
  pinMode(PULSE_GPIO_INPUT1, OUTPUT);
  pinMode(PULSE_GPIO_INPUT2, OUTPUT);
  digitalWrite(PULSE_GPIO_ENABLE, LOW);


  // Init display
  tft.init();
  tft.setRotation(1);
  tft.setTextWrap(true, false);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);


  // Set display brightness very low to save energy
  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(TFT_BL, PWM_CHANNEL);
  ledcWrite(PWM_CHANNEL, PWM_DUTY);

  // Init status display
  updateDisplayStatus();

  // Start Wifi 
  tft.setCursor(0, 30);
  tft.print("Waiting for WiFi");

  if (wifi.init() == ESP_OK) {
     ESP_LOGI(TAG, "WiFi initialisiert");
  } else {
     ESP_LOGE(TAG, "WiFi Initialisierung fehlgeschlagen");
    return;
  }

  while (wifi.connect() != ESP_OK) {
     ESP_LOGE(TAG, "WiFi Verbindung fehlgeschlagen. Erneuter Versuch...");
  }

  // SNTP und Zeitzone initialisieren
  if (wifi.initSNTP() == ESP_OK) {
     ESP_LOGI(TAG, "SNTP initialisiert");
  } else {
     ESP_LOGE(TAG, "SNTP Initialisierung fehlgeschlagen");
  }

  if (wifi.initTimezone() == ESP_OK) {
     ESP_LOGI(TAG, "Zeitzone initialisiert");
  } else {
     ESP_LOGE(TAG, "Zeitzonen Initialisierung fehlgeschlagen");
  }


 

  // Info text
  tft.setCursor(0, 30);
  tft.print("Move the hands to 12 o'clock position. Then press Start");
  
   ESP_LOGI(TAG, "Start Setup");
  buttons.setMoveCallback(sendPulse); // Callback for moving the handles
  buttons.start(); // Blocking loop to set the hands
  
  // Create task
  tft.fillRect(0, 30, tft.width(), tft.height(), TFT_BLACK);    
  xTaskCreatePinnedToCore(moveHandsTask, "MoveHands", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(displayTimeTask, "DisplayTime", 4096, NULL, 1, NULL, 1);

}

// Task to move the hands
void moveHandsTask(void *param) {
  struct tm timeinfo;

  // Wait until we get the correct time
  do {
    vTaskDelay(pdMS_TO_TICKS(1000));
  } while (!timeSynced);

  // Now start movement of the hands
  while (true) {
    
    if (getTime(timeinfo)) { // Get the current time

      static int16_t clock_minutes = 0; // Variable to track the clock's current minute position
      uint16_t current_minutes = (timeinfo.tm_hour % CLOCK_HOURS) * 60 + timeinfo.tm_min;  // Calculate current minutes on the clock
      int16_t difference = current_minutes - clock_minutes; // Calculate the difference in minutes

      ESP_LOGI(TAG, "Difference: %d", difference);

      // Handle negative differences (e.g., crossing midnight or wrapping around the 12-hour format)
      if (difference < 0) {
          difference += CLOCK_HOURS * 60; // Add hours in minutes. Time always goes forward :-)
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

}

// Task: Show time on the display
void displayTimeTask(void *param) {
  struct tm timeinfo;

  while (true) {
    // Get the current time
    if (getTime(timeinfo)) {

      // Time in format HH:MM:SS 
      char timeStr[9]; // Space for "HH:MM:SS"
      strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);

      // Show the time on the display
      tft.setTextDatum(MC_DATUM);
      tft.drawString(timeStr, tft.width() / 2, tft.height() / 2, 4);
    }

    vTaskDelay(pdMS_TO_TICKS(1000)); // Wait a second
  }
}

// Not needed
void loop() {
}

