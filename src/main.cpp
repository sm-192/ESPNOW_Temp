#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#if defined(ESP8266_TX) || defined(ESP8266_RX) || defined(ESP8266_MAC)
  #include <ESP8266WiFi.h>
  #include <espnow.h>
#elif defined(ESP32_TX) || defined(ESP32_RX) || defined(ESP32_MAC)
  #include <WiFi.h>
  #include <esp_now.h>
#endif

#define ONEWIRE_PIN D2   // pino do DS18B20

// Estrutura de dados comum
struct SensorData {
  char nome_tx[16];
  float temp;
};

#if defined(ESP8266_TX)
  #include "esp8266_tx.h"
#elif defined(ESP8266_RX)
  #include "esp8266_rx.h"
#elif defined(ESP8266_MAC)
  #include "esp8266_mac.h"
#elif defined(ESP8266_RTC)
  #include "esp8266_rtc.h"
#elif defined(ESP32_MAC)
  #include "esp32_mac.h"
#else
  #error "Nenhum papel definido! Use -DTRANSMISSOR ou -DRECEPTOR."
#endif