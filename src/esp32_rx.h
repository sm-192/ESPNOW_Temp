#ifndef ESP32_RX_H
#define ESP32_RX_H

#include <Wire.h>
#include <RTClib.h>
#include <LittleFS.h>
#include <SD.h>
#include <SPI.h>

#ifndef QTDE_TX
#define QTDE_TX 3
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

#define LISTA_TX { "Garrafa1","Garrafa2","Garrafa3","Isopor1","Isopor2","Isopor3","Botuflex1","Botuflex2","Botuflex3","Extra" }

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

SensorData stationData[QTDE_TX];
StationState stationStates[QTDE_TX];
const char* expectedNames[10] = LISTA_TX;

unsigned long lastAmbientMillis = 0;
bool receivedStation[QTDE_TX] = { false };

String pad2(int value) { return (value < 10 ? "0" : "") + String(value); }

void writeLog(const String &entry) {
    Serial.println(entry);

    // LittleFS
    File logFile = LittleFS.open("/log.txt", "a");
    if (logFile) { logFile.println(entry); logFile.close(); }

    // SD Card
    if (SD.begin(SD_CS_PIN)) {
        File sdFile = SD.open("/log.txt", FILE_APPEND);
        if (sdFile) { sdFile.println(entry); sdFile.close(); }
    }

    // SSE
    events.send(entry.c_str(), "message", millis());
}

int getStationIndex(const char* nome) {
    for (int i = 0; i < QTDE_TX; i++) {
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
}

void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
    if (len != sizeof(SensorData)) return;

    SensorData data;
    memcpy(&data, incomingData, sizeof(SensorData));

    int idx = getStationIndex(data.nome_tx);
    if (idx != -1) {
        stationData[idx] = data;
        receivedStation[idx] = true;
        logStation(data.nome_tx, data.temp);
    }
}

void handleLog(AsyncWebServerRequest *request) {
    if (LittleFS.exists("/log.txt")) {
        request->send(LittleFS, "/log.txt", "text/plain", true);
        return;
    }
    if (SD.exists("/log.txt")) {
        request->send(SD, "/log.txt", "text/plain", true);
        return;
    }
    request->send(200, "text/plain", "Nenhum log encontrado.");
}

void setup() {
    Serial.begin(115200);
    pinMode(FLASH_BTN, INPUT_PULLUP);

    if (!rtc.begin()) { Serial.println("Erro: RTC DS1307 não encontrado!"); while (1); }
    if (!LittleFS.begin()) { Serial.println("Falha ao montar LittleFS"); while (1); }
    if (!SD.begin(SD_CS_PIN)) { Serial.println("Falha ao inicializar SD"); }

    sensors.begin();
    sensors.setResolution(12);

    for (int i = 0; i < QTDE_TX; i++) {
        strncpy(stationStates[i].nome, expectedNames[i], sizeof(stationStates[i].nome));
        stationStates[i].lowAlert = false;
        stationStates[i].highAlert = false;
    }

    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("RECEPTOR","12345678");

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        String html = R"rawliteral(
            <!DOCTYPE html><html><head><meta charset='UTF-8'><title>ESP32 Log</title></head>
            <body>
            <h1>Log em tempo real</h1>
            <div id='log'></div>
            <button onclick="window.location='/log'">Baixar log</button>
            <script>
                var source = new EventSource('/events');
                source.onmessage = function(e){ document.getElementById('log').innerHTML += e.data + '<br>'; };
            </script>
            </body></html>
        )rawliteral";
        request->send(200,"text/html",html);
    });
    server.on("/log", HTTP_GET, handleLog);
    server.addHandler(&events);
    server.begin();

    if (esp_now_init() != ESP_OK) { Serial.println("Erro ESP-NOW"); return; }
    esp_now_register_recv_cb(onDataRecv);

    if (LittleFS.exists("/log.txt")) {
    LittleFS.remove("/log.txt");
    Serial.println("Log apagado do LittleFS.");
    }
    if (SD.begin(SD_CS_PIN)) {
        if (SD.exists("/log.txt")) {
            SD.remove("/log.txt");
            Serial.println("Log apagado do SD.");
        }
    }

    Serial.println("Pronto para receber dados...");
}

void loop() {
    unsigned long now = millis();

    if (digitalRead(FLASH_BTN) == LOW) {
        Serial.println("Botão FLASH pressionado: log zerado.");
        if (LittleFS.exists("/log.txt")) LittleFS.remove("/log.txt");
        if (SD.exists("/log.txt")) SD.remove("/log.txt");
        delay(500);
    }

    if (now - lastAmbientMillis >= 60000) {
        logAmbient();
        lastAmbientMillis = now;
    }
}

#endif
