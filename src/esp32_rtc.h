#ifndef ESP32_RTC_H
#define ESP32_RTC_H

#include <Wire.h>
#include <RTClib.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <TimeLib.h>  

RTC_DS1307 rtc;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "br.pool.ntp.org", -10800, 60000); // GMT-3, atualiza a cada 60s

char t[32];
byte last_second, second_, minute_, hour_, day_, month_;
int year_;

void setup() {
    Serial.begin(115200);
    Wire.begin();  // I2C ESP32 padrão
    rtc.begin();

    // Conecta ao WiFi
    char* ssid = NOME_REDE;
    char* password = SENHA;
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConectado ao WiFi");

    // Inicializa NTP
    timeClient.begin();
    timeClient.update();
    unsigned long unix_epoch = timeClient.getEpochTime();
    rtc.adjust(DateTime(unix_epoch)); // Sincroniza RTC

    DateTime now = rtc.now();
    last_second = now.second();
}

void loop() {
    timeClient.update();
    unsigned long unix_epoch = timeClient.getEpochTime();

    second_ = second(unix_epoch);
    if (last_second != second_) {
        minute_ = minute(unix_epoch);
        hour_   = hour(unix_epoch);
        day_    = day(unix_epoch);
        month_  = month(unix_epoch);
        year_   = year(unix_epoch);

        // Mostra hora NTP
        sprintf(t, "NTP Time: %02d:%02d:%02d %02d/%02d/%02d", hour_, minute_, second_, day_, month_, year_);
        Serial.println(t);

        // Mostra hora RTC
        DateTime rtcTime = rtc.now();
        sprintf(t, "RTC Time: %02d:%02d:%02d %02d/%02d/%02d", rtcTime.hour(), rtcTime.minute(), rtcTime.second(), rtcTime.day(), rtcTime.month(), rtcTime.year());
        Serial.println(t);

        // Comparação
        if (rtcTime == DateTime(year_, month_, day_, hour_, minute_, second_)) {
            Serial.println("Hora e data sincronizados!");
        } else {
            Serial.println("Hora e data não sincronizados!");
        }

        last_second = second_;
    }

    delay(1000);
}

#endif // ESP32_RTC_H