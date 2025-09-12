#ifndef ESP8266_TX_H
#define ESP8266_TX_H

    #include <OneWire.h>
    #include <DallasTemperature.h>

    #define ONEWIRE_PIN D2   // pino do DS18B20
    #ifndef NODE_ID
    #define NODE_ID "caixa"   // valor padrão, caso não seja definido no build_flags
    #endif

    // Configura DS18B20
    OneWire oneWire(ONEWIRE_PIN);
    DallasTemperature sensors(&oneWire);

    // MAC do receptor (ajustar conforme sua rede)
    #ifndef MAC_RX
    #define MAC_RX {MAC_RX_0, MAC_RX_1, MAC_RX_2, MAC_RX_3, MAC_RX_4, MAC_RX_5}  // fallback
    #endif

    uint8_t mac_rx[6] = MAC_RX;

    // Conversões de tempo
    uint64_t secondsToUs(uint32_t s) { return (uint64_t)s * 1000000ULL; }
    uint64_t minutesToUs(uint32_t m) { return (uint64_t)m * 60ULL * 1000000ULL; }
    uint64_t hoursToUs(uint32_t h)   { return (uint64_t)h * 3600ULL * 1000000ULL; }

    #if INTERVALO == SEGUNDOS
    const uint64_t SEND_INTERVAL = secondsToUs(TEMPO);
    #elif INTERVALO == MINUTOS
    const uint64_t SEND_INTERVAL = minutesToUs(TEMPO);
    #elif INTERVALO == HORAS
    const uint64_t INTERVALO = hoursToUs(TEMPO);
    #else
    #error "INTERVALO inválido! Use SEGUNDOS, MINUTOS ou HORAS."
    #endif

    void setup() {
    Serial.begin(115200);

    WiFi.mode(WIFI_AP_STA);
    WiFi.disconnect();

    if (esp_now_init() != 0) {
        Serial.println("Erro ao iniciar ESP-NOW");
        return;
    }

    esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
    esp_now_add_peer(mac_rx, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);

    sensors.begin();
    sensors.setResolution(12); // máxima precisão

    // Faz a leitura do sensor
    sensors.requestTemperatures();
    float temp = sensors.getTempCByIndex(0);
    temp = round(temp * 100.0) / 100.0;

    SensorData data;
    data.nodeId = NODE_ID;
    data.temp = temp;

    esp_now_send(mac_rx, (uint8_t*)&data, sizeof(data));
    Serial.printf("Enviado: ID=%d Temp=%.2f°C\n", data.nodeId, data.temp);

    // Light sleep até próxima leitura
    Serial.printf("Dormindo por %.2f segundos...\n", (double)SEND_INTERVAL / 1e6);

    WiFi.forceSleepBegin(); // WiFi em sleep
    delay(1);
    system_deep_sleep(SEND_INTERVAL); // acorda pelo timer
    }

    void loop() {
    // nada, o ESP acorda sozinhoD
    }

#endif // TRANSMISSOR