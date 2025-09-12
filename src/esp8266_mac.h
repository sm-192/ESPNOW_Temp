#ifndef ESP8266_MAC_H
#define ESP8266_MAC_H

    #include <ESP8266WiFi.h>

    void setup(){
        Serial.begin(115200);
        Serial.println();
        Serial.print("ESP Board MAC Address:  ");
        Serial.println(WiFi.macAddress());
    }
    
    void loop(){
    }

#endif // ESP8266_MAC