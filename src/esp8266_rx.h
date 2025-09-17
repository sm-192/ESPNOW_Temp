#ifndef ESP8266_RX_H
#define ESP8266_RX_H

    #include <OneWire.h>
    #include <DallasTemperature.h>
    #include <Wire.h>
    #include <RTClib.h>
    #include <LittleFS.h>             
    #include <ESP8266WebServer.h>
    #include <Arduino.h>

    #ifndef TEMP_MIN
    #define TEMP_MIN 5.0
    #endif
    #ifndef TEMP_MAX
    #define TEMP_MAX 10.0
    #endif

    // --------------------
    // Hardware e objetos
    // --------------------
    #define LED_PIN 2
    #define ONEWIRE_PIN D2
    #define FLASH_BTN 0  // GPIO0 (botão FLASH)
    #define TIMEOUT_MS 20000

    unsigned long lastRecvTime = 0;

    RTC_DS3231 rtc;
    ESP8266WebServer server(80);
    OneWire oneWire(ONEWIRE_PIN);
    DallasTemperature sensors(&oneWire);

    const char* ssid = "RECEPTOR";
    const char* password = "12345678";

    // --------------------
    // Funções auxiliares
    // --------------------
    String pad2(int value) {
        return (value < 10 ? "0" : "") + String(value);
    }

    // Contagem de estações e estado de transição
    int receivedCount = 0;
    bool stateLow[6] = {false};
    bool stateHigh[6] = {false};

    // --------------------
    // Função de log (sensores e ambiente)
    // --------------------
    void logEntryWithAlert(const String &entry, uint8_t nodeId = 255, float temp = NAN) {
        String logEntry = entry;

        if (!isnan(temp) && nodeId < 6) { 
            if (temp < TEMP_MIN && !stateLow[nodeId]) {
                logEntry += " <<< ALERTA: abaixo de " + String(TEMP_MIN) + "°C!";
                stateLow[nodeId] = true;
                stateHigh[nodeId] = false;
            } else if (temp > TEMP_MAX && !stateHigh[nodeId]) {
                logEntry += " <<< ALERTA: acima de " + String(TEMP_MAX) + "°C!";
                stateHigh[nodeId] = true;
                stateLow[nodeId] = false;
            } else if (temp >= TEMP_MIN && temp <= TEMP_MAX) {
                if (stateLow[nodeId] || stateHigh[nodeId]) {
                    logEntry += " <<< NORMALIZADO";
                }
                stateLow[nodeId] = false;
                stateHigh[nodeId] = false;
            }
        }

        Serial.println(logEntry);

        // Salva no log.txt
        File logFile = LittleFS.open("/log.txt", "a");
        if(logFile){
            logFile.println(logEntry);
            logFile.close();
        }

        // Contagem de estações
        if (!isnan(temp) && nodeId < 6) {
            receivedCount++;
            if (receivedCount >= QTDE_TX) {
                Serial.println("--------------------------------------");
                receivedCount = 0;
            }
        }
    }

    // --------------------
    // Callback ESP-NOW
    // --------------------
    void onDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
        if(len != sizeof(SensorData)) return;

        SensorData data;
        memcpy(&data, incomingData, sizeof(data));

        lastRecvTime = millis();

        // Pisca LED
        digitalWrite(LED_PIN, HIGH);
        delay(100);
        digitalWrite(LED_PIN, LOW);

        DateTime now = rtc.now();
        String entry = pad2(now.day()) + "/" + pad2(now.month()) + "/" + String(now.year()) + " ";
        entry += pad2(now.hour()) + ":" + pad2(now.minute()) + ":" + pad2(now.second());
        entry += " - Cx: " + String(data.nodeId) + " | Temp: " + String(data.temp, 2) + " °C";

        logEntryWithAlert(entry, data.nodeId, data.temp);
    }

    // --------------------
    // Rota web /log
    // --------------------
    void handleLog() {
        if(LittleFS.exists("/log.txt")) {
            File logFile = LittleFS.open("/log.txt", "r");
            if(logFile){
                server.streamFile(logFile, "text/plain");
                logFile.close();
            }
        } else {
            server.send(200, "text/plain", "Nenhum log encontrado.");
        }
    }

    // --------------------
    // Setup e loop
    // --------------------
    void setup() {
        Serial.begin(115200);
        pinMode(LED_PIN, OUTPUT);
        digitalWrite(LED_PIN, LOW);

        pinMode(FLASH_BTN, INPUT_PULLUP);

        if (!rtc.begin()) {
            Serial.println("Erro: RTC DS3231 não encontrado!");
            while (1);
        }

        if(!LittleFS.begin()){
            Serial.println("Falha ao montar LittleFS");
            return;
        }

        sensors.begin();
        sensors.setResolution(12);

        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP(ssid, password);
        WiFi.disconnect();
        Serial.print("AP iniciado. Conecte-se em: ");
        Serial.println(WiFi.softAPIP());

        server.on("/", [](){
            server.send(200, "text/html", "<h1>Servidor ESP8266</h1><p><a href=\"/log\">Ver log</a></p>");
        });
        server.on("/log", handleLog);
        server.begin();

        if (esp_now_init() != 0) {
            Serial.println("Erro ESP-NOW");
            return;
        }

        esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
        esp_now_register_recv_cb(onDataRecv);

        Serial.println("Pronto para receber dados...");
    }

    void loop() {
        server.handleClient();

        // Botão FLASH para zerar log
        if (digitalRead(FLASH_BTN) == LOW) {
            Serial.println("Botão FLASH pressionado: log zerado.");
            if(LittleFS.exists("/log.txt")) LittleFS.remove("/log.txt");
            delay(500); // debounce simples
        }

        // Timeout das estações
        if (receivedCount > 0 && millis() - lastRecvTime > TIMEOUT_MS) {
            Serial.println("-------------------------------------- (timeout)");
            receivedCount = 0;
            lastRecvTime = millis(); // reseta o timer
        }

        // --------------------
        // Leitura temperatura ambiente **antes** das estações
        // --------------------
        static bool ambientLogged = false;
        if (!ambientLogged && receivedCount == 0) {
            sensors.requestTemperatures();
            float ambientTemp = sensors.getTempCByIndex(0);

            DateTime now = rtc.now();
            String entry = pad2(now.day()) + "/" + pad2(now.month()) + "/" + String(now.year()) + " ";
            entry += pad2(now.hour()) + ":" + pad2(now.minute()) + ":" + pad2(now.second());
            entry += " - Ambiente: " + String(ambientTemp, 2) + " °C";

            logEntryWithAlert(entry, 255, ambientTemp); // 255 indica que não é uma estação específica
            ambientLogged = true; // só registra uma vez antes das estações
        }

        // Reseta flag quando todas as estações tiverem enviado
        if (receivedCount == 0) {
            ambientLogged = false;
        }
    }

#endif // ESP8266_RX_H
