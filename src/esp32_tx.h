#ifndef ESP32_TX_H
#define ESP32_TX_H

// --------------------
// Configurações do sensor
// --------------------
OneWire oneWire(ONEWIRE_PIN);
DallasTemperature sensors(&oneWire);

// --------------------
// MAC do receptor
// --------------------
#ifndef MAC_RX
#define MAC_RX {MAC_RX_0, MAC_RX_1, MAC_RX_2, MAC_RX_3, MAC_RX_4, MAC_RX_5}  // fallback
#endif
uint8_t mac_rx[6] = MAC_RX;

// --------------------
// Intervalos de envio
// --------------------
uint64_t secondsToUs(uint32_t s) { return (uint64_t)s * 1000000ULL; }
uint64_t minutesToUs(uint32_t m) { return (uint64_t)m * 60ULL * 1000000ULL; }
uint64_t hoursToUs(uint32_t h)   { return (uint64_t)h * 3600ULL * 1000000ULL; }

#if INTERVALO == SEGUNDOS
const uint64_t SEND_INTERVAL = secondsToUs(TEMPO);
#elif INTERVALO == MINUTOS
const uint64_t SEND_INTERVAL = minutesToUs(TEMPO);
#elif INTERVALO == HORAS
const uint64_t SEND_INTERVAL = hoursToUs(TEMPO);
#else
#error "INTERVALO inválido! Use SEGUNDOS, MINUTOS ou HORAS."
#endif

// --------------------
// Setup
// --------------------
void setup() {
    Serial.begin(115200);

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (esp_now_init() != ESP_OK) {
        Serial.println("Erro ao iniciar ESP-NOW");
        return;
    }

    // Adiciona peer (receptor)
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, mac_rx, 6);
    peerInfo.channel = 0;  
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Falha ao adicionar peer");
        return;
    }

    sensors.begin();
    sensors.setResolution(12); // máxima precisão

    // Leitura do sensor
    SensorData data = {};
    strncpy(data.nome_tx, TX_ID, sizeof(data.nome_tx));
    data.nome_tx[sizeof(data.nome_tx)-1] = '\0';

    sensors.requestTemperatures();
    float temp = sensors.getTempCByIndex(0);
    temp = round(temp * 100.0) / 100.0;
    data.temp = temp;

    // Envia para o RX
    esp_err_t result = esp_now_send(mac_rx, (uint8_t*)&data, sizeof(data));
    if (result == ESP_OK) {
        Serial.printf("Enviado: ID=%s Temp=%.2f°C\n", data.nome_tx, data.temp);
    } else {
        Serial.println("Erro ao enviar dados");
    }

    // --------------------
    // Light sleep até próxima leitura
    // --------------------
    Serial.printf("Dormindo por %.2f segundos...\n", (double)SEND_INTERVAL / 1e6);

    esp_sleep_enable_timer_wakeup(SEND_INTERVAL);
    esp_deep_sleep_start();
}

// --------------------
// Loop vazio
// --------------------
void loop() {
    // nada, ESP32 acorda sozinho do deep sleep
}

#endif // ESP32_TX_H