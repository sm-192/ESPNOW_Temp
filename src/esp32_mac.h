#ifndef ESP32_MAC_H
#define ESP32_MAC_H

    #include <WiFi.h>

    void setup(){
        Serial.begin(115200);
        delay(1000);

        // Pega MAC do WiFi station (STA)
        String mac = WiFi.macAddress();
        Serial.println("MAC Address (STA): " + mac);

        // Pega MAC do WiFi access point (AP)
        String macAP = WiFi.softAPmacAddress();
        Serial.println("MAC Address (AP): " + macAP);
    }
    
    void loop(){
    }

#endif // ESP32_MAC