#ifndef ESP32_MAC_H
#define ESP32_MAC_H

    #include <WiFi.h>
    #include <esp_wifi.h>

    void printMac(wifi_interface_t interface, const char* name) {
        uint8_t mac[6];
        esp_err_t ret = esp_wifi_get_mac(interface, mac);
        if (ret == ESP_OK) {
            Serial.printf("[%s] MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                        name,
                        mac[0], mac[1], mac[2],
                        mac[3], mac[4], mac[5]);
        } else {
            Serial.printf("[%s] Falha ao ler o endereço MAC\n", name);
        }
    }

    void setup() {
        Serial.begin(115200);
        delay(1000);

        // Inicializa as duas interfaces (STA + AP)
        WiFi.mode(WIFI_AP_STA);

        // Lê o MAC de cada interface
        printMac(WIFI_IF_STA, "STA: ");
        printMac(WIFI_IF_AP,  "AP: ");
    }

    void loop() {
    }

#endif // ESP32_MAC