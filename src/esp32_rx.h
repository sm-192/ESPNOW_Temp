#ifndef ESP32_RX_H
#define ESP32_RX_H

    #include <Wire.h>
    #include <RTClib.h>
    #include <LittleFS.h>
    #include <SD.h>
    #include <SPI.h>

    #ifndef QTDE_TX
        #define QTDE_TX 1
    #endif
    #ifndef TEMP_MIN
        #define TEMP_MIN 5.0
    #endif
    #ifndef TEMP_MAX
        #define TEMP_MAX 10.0
    #endif

    #define FLASH_BTN 0
    #define SD_CS_PIN 33
    #define ONEWIRE_PIN 32

    // Lista de nomes a partir do PlatformIO.ini
    #define LISTA_TX {NOME_TX1,NOME_TX2,NOME_TX3,NOME_TX4,NOME_TX5,NOME_TX6,NOME_TX7,NOME_TX8,NOME_TX9,NOME_TX10}

    RTC_DS1307 rtc;
    AsyncWebServer server(80);
    AsyncEventSource events("/events");
    OneWire oneWire(ONEWIRE_PIN);
    DallasTemperature sensors(&oneWire);

    struct StationState {
        char nome[16];
        bool lowAlert;
        bool highAlert;
    };

    SensorData stationData[QTDE_TX];          // Últimos dados recebidos
    StationState stationStates[QTDE_TX];      // Estados de alerta por estação
    bool receivedStation[QTDE_TX] = {false};  // Marca se cada estação já enviou
    const char *expectedNames[10] = LISTA_TX;
    unsigned long lastRecvTime = 0;
    int receivedCount = 0;
    bool waitingBlock = false; // indica se já iniciamos um bloco

    // --------------------
    // Funções auxiliares
    // --------------------
    String pad2(int value) {
        return (value < 10 ? "0" : "") + String(value);
    }


    void writeLog(const String &entry) {
        Serial.println(entry);

        // ---------------- LittleFS ----------------
        File logFile = LittleFS.open("/log.txt", "a");
        if (logFile) {
            logFile.println(entry);
            logFile.flush();  // força escrita imediata
            logFile.close();
        } else {
            Serial.println("Erro ao abrir /log.txt no LittleFS");
        }

        // ---------------- SD Card ----------------
        if (SD.begin(SD_CS_PIN)) {  // garante inicialização do SD
            File sdFile = SD.open("/log.txt", FILE_APPEND); // append explícito
            if (sdFile) {
                sdFile.println(entry);
                sdFile.flush();  // força escrita imediata
                sdFile.close();
            } else {
                Serial.println("Erro ao abrir /log.txt no SD");
            }
        } else {
            Serial.println("SD não inicializado ao escrever log");
        }

        // ---------------- SSE ----------------
        events.send(entry.c_str(), "message", millis());
    }

    int getStationIndex(const char* nome) {
        for (int i = 0; i < QTDE_TX; i++) {
            if (strlen(stationStates[i].nome) == 0) continue;
            if (strcmp(stationStates[i].nome, nome) == 0) return i;
        }
        return -1;
    }

    void logStation(const char* nome_tx, float temp) {
        int idx = getStationIndex(nome_tx);
        if (idx == -1) return;

        String entry;
        DateTime now = rtc.now();
        entry += pad2(now.day()) + "/" + pad2(now.month()) + "/" + String(now.year()) + " ";
        entry += pad2(now.hour()) + ":" + pad2(now.minute()) + ":" + pad2(now.second());
        entry += " - Est: " + String(nome_tx) + " | Temp: " + String(temp, 2) + " \u00B0C";

        if (temp < TEMP_MIN && !stationStates[idx].lowAlert) {
            entry += " <<< ALERTA: abaixo de " + String(TEMP_MIN) + " \u00B0C!";
            stationStates[idx].lowAlert = true;
            stationStates[idx].highAlert = false;
        } else if (temp > TEMP_MAX && !stationStates[idx].highAlert) {
            entry += " <<< ALERTA: acima de " + String(TEMP_MAX) + " \u00B0C";
            stationStates[idx].highAlert = true;
            stationStates[idx].lowAlert = false;
        } else if (temp >= TEMP_MIN && temp <= TEMP_MAX) {
            if (stationStates[idx].lowAlert || stationStates[idx].highAlert)
                entry += " <<< NORMALIZADO";
            stationStates[idx].lowAlert = false;
            stationStates[idx].highAlert = false;
        }

        writeLog(entry);
    }

    void logAmbient() {
        sensors.requestTemperatures();
        float ambientTemp = sensors.getTempCByIndex(0);

        DateTime now = rtc.now();
        String entry = pad2(now.day()) + "/" + pad2(now.month()) + "/" + String(now.year()) + " ";
        entry += pad2(now.hour()) + ":" + pad2(now.minute()) + ":" + pad2(now.second());
        entry += " - Ambiente: " + String(ambientTemp, 2) + " \u00B0C";

        writeLog(entry);

        waitingBlock = true;
        receivedCount = 0;
        for (int i = 0; i < QTDE_TX; i++) receivedStation[i] = false;
    }

    void closeBlock() {
        for (int i = 0; i < QTDE_TX; i++) {
            if (!receivedStation[i] && strlen(expectedNames[i]) > 0) {
                writeLog(String("Estacao faltante: ") + expectedNames[i]);
            }
        }
        writeLog("--------------------------------------");

        waitingBlock = false;
        receivedCount = 0;
    }

    // --------------------
    // Callback ESP-NOW
    // --------------------
    void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
        if (len != sizeof(SensorData)) return;
        SensorData data;
        memcpy(&data, incomingData, sizeof(SensorData));

        lastRecvTime = millis();

        int idx = getStationIndex(data.nome_tx);
        if (idx != -1 && !receivedStation[idx]) {
            stationData[idx] = data;
            receivedStation[idx] = true;
            receivedCount++;
            logStation(data.nome_tx, data.temp);
        }
    }

    // --------------------
    // Rota web /log
    // --------------------
    void handleLog(AsyncWebServerRequest *request) {
    // Primeiro tenta no LittleFS
        if (LittleFS.exists("/log.txt")) {
            request->send(LittleFS, "/log.txt", "text/plain", true); // true habilita download
            return;
        }

        // Depois tenta no SD
        if (SD.exists("/log.txt")) {
            request->send(SD, "/log.txt", "text/plain", true); // true habilita download
            return;
        }

        // Se não existir em nenhum dos dois, retorna mensagem
        request->send(200, "text/plain", "Nenhum log encontrado.");
    }

    // --------------------
    // Setup e loop
    // --------------------
    void setup() {
        Serial.begin(115200);
        pinMode(FLASH_BTN, INPUT_PULLUP);

        // --------------------
        // RTC
        // --------------------
        if (!rtc.begin()) {
            Serial.println("Erro: RTC DS1307 não encontrado!");
            while (1);
        }

        // --------------------
        // LittleFS
        // --------------------
        if (!LittleFS.begin()) {
            Serial.println("Falha ao montar LittleFS, tentando formatar...");
            if (!LittleFS.begin(true)) {
                Serial.println("Erro crítico: não foi possível montar LittleFS");
                while (1);
            } else {
                Serial.println("LittleFS formatado e montado com sucesso.");
            }
        } else {
            Serial.println("LittleFS montado com sucesso.");
        }

        // --------------------
        // SD Card
        // --------------------
        if (!SD.begin(SD_CS_PIN)) {
            Serial.println("Falha ao inicializar o cartão SD.");
        } else {
            Serial.println("Cartão SD pronto.");
        }

        // --------------------
        // Sensor DS18B20
        // --------------------
        sensors.begin();
        sensors.setResolution(12);  // 12 bits, conversão ~750ms

        // --------------------
        // Inicializa estados das estações
        // --------------------
        for (int i = 0; i < QTDE_TX; i++) {
            strncpy(stationStates[i].nome, expectedNames[i], sizeof(stationStates[i].nome));
            stationStates[i].lowAlert = false;
            stationStates[i].highAlert = false;
        }

        // --------------------
        // Wi-Fi AP+STA
        // --------------------
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP("RECEPTOR", "12345678");
        Serial.print("AP iniciado. Conecte-se em: ");
        Serial.println(WiFi.softAPIP());

        // --------------------
        // Webserver e SSE
        // --------------------
        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
            String html = R"rawliteral(
                <!DOCTYPE html>
                <html>
                <head>
                    <meta charset="UTF-8">
                    <title>Servidor ESP32 - Log</title>
                    <style>
                        body { font-family: monospace; background: #f5f5f5; padding: 20px; }
                        #log { border: 1px solid #ccc; background: #fff; padding: 10px; height: 400px; overflow-y: scroll; }
                        button { margin-top: 10px; padding: 10px 15px; }
                    </style>
                </head>
                <body>
                    <h1>Servidor ESP32 - Log em tempo real</h1>
                    <div id="log"></div>
                    <button onclick="window.location='/log'">Baixar log</button>

                    <script>
                        var logDiv = document.getElementById('log');
                        var source = new EventSource('/events');

                        source.onmessage = function(event) {
                            logDiv.innerHTML += event.data + '<br>';
                            logDiv.scrollTop = logDiv.scrollHeight;
                        };

                        source.onerror = function(err) {
                            console.error("SSE error:", err);
                        };
                    </script>
                </body>
                </html>
            )rawliteral";

            request->send(200, "text/html; charset=utf-8", html);
        });

        server.addHandler(&events);
        server.on("/log", HTTP_GET, handleLog);
        server.begin();

        // --------------------
        // ESP-NOW
        // --------------------
        if (esp_now_init() != ESP_OK) {
            Serial.println("Erro ESP-NOW");
            return;
        }
        esp_now_register_recv_cb(onDataRecv);

        Serial.println("Pronto para receber dados...");
    }

    // --------------------
    // Loop principal
    // --------------------
    unsigned long lastTempRead = 0;

    void loop() {
        unsigned long now = millis();

        // Botão FLASH para zerar log
        if (digitalRead(FLASH_BTN) == LOW) {
            Serial.println("Botão FLASH pressionado: log zerado.");
            if (LittleFS.exists("/log.txt")) LittleFS.remove("/log.txt");
            if (SD.exists("/log.txt")) SD.remove("/log.txt");
            delay(500);
        }

        // Log do ambiente (DS18B20) a cada intervalo definido
        if (!waitingBlock && (now - lastTempRead > TIMEOUT_MS)) {
            logAmbient();
            lastTempRead = now;
            lastRecvTime = now;
        }

        // Fechamento do bloco quando todas estações enviaram ou timeout
        if (waitingBlock) {
            if (receivedCount >= QTDE_TX || now - lastRecvTime > TIMEOUT_MS) {
                closeBlock();
            }
        }
    }

#endif // ESP32_RX_H