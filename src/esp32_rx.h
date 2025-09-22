#ifndef ESP32_RX_H
#define ESP32_RX_H

#include <Wire.h>
#include <RTClib.h>
#include <LittleFS.h>
#include <SD.h>
#include <SPI.h>
#include <ESPAsyncWebServer.h>

#ifndef QTDE_TX
    #define QTDE_TX 1
#endif
#ifndef TEMP_MIN
    #define TEMP_MIN 5.0
#endif
#ifndef TEMP_MAX
    #define TEMP_MAX 10.0
#endif

#define LED_PIN 2
#define FLASH_BTN 0  // GPIO0 (botão FLASH)
#define SD_CS_PIN 33 // GPIO15 (pino CS do SD)
#define ONEWIRE_PIN 32 // Exemplo, ajuste conforme seu hardware

// Lista de nomes a partir do PlatformIO.ini
#define LISTA_TX {NOME_TX1,NOME_TX2,NOME_TX3,NOME_TX4,NOME_TX5,NOME_TX6,NOME_TX7,NOME_TX8,NOME_TX9,NOME_TX10}

RTC_DS1307 rtc;
AsyncWebServer server(80);
OneWire oneWire(ONEWIRE_PIN);
DallasTemperature sensors(&oneWire);

// --------------------
// Estruturas de controle
// --------------------
struct SensorData {
    char nome_tx[16];
    float temp;
};

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
    File logFile = LittleFS.open("/log.txt", "a");
    if (logFile) {
        logFile.println(entry);
        logFile.close();
    }

    File sdFile = SD.open("/log.txt", FILE_WRITE);
    if (sdFile) {
        sdFile.println(entry);
        sdFile.close();
    }
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
void onDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
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

    digitalWrite(LED_PIN, HIGH);
    delay(50);
    digitalWrite(LED_PIN, LOW);
}

// --------------------
// Rota web /log
// --------------------
void handleLog(AsyncWebServerRequest *request) {
    if (LittleFS.exists("/log.txt")) {
        File logFile = LittleFS.open("/log.txt", "r");
        if (logFile) {
            request->send(logFile, "text/plain; charset=UTF-8");
            logFile.close();
            return;
        }
    }
    request->send(200, "text/plain", "Nenhum log encontrado.");
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
        Serial.println("Erro: RTC DS1307 não encontrado!");
        while (1);
    }

    if (!LittleFS.begin()) {
        Serial.println("Falha ao montar LittleFS");
        return;
    }

    if (!SD.begin(SD_CS_PIN)) {
        Serial.println("Falha ao inicializar o cartão SD.");
    } else {
        Serial.println("Cartão SD pronto.");
    }

    sensors.begin();
    sensors.setResolution(12);

    for (int i = 0; i < QTDE_TX; i++) {
        strncpy(stationStates[i].nome, expectedNames[i], sizeof(stationStates[i].nome));
        stationStates[i].lowAlert = false;
        stationStates[i].highAlert = false;
    }

    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("RECEPTOR", "12345678");
    WiFi.disconnect();
    Serial.print("AP iniciado. Conecte-se em: ");
    Serial.println(WiFi.softAPIP());

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        String html = "<h1>Servidor ESP32</h1>";
        html += "<p><a href=\"/log\" target=\"_blank\">Ver log</a></p>";
        html += "<p><a href=\"/log\" download=\"log.txt\">Baixar log</a></p>";
        request->send(200, "text/html; charset=utf-8", html);
    });

    server.on("/log", HTTP_GET, handleLog);
    server.begin();

    if (esp_now_init() != ESP_OK) {
        Serial.println("Erro ESP-NOW");
        return;
    }
    esp_now_register_recv_cb(onDataRecv);

    Serial.println("Pronto para receber dados...");
}

void loop() {
    // Botão FLASH para zerar log
    if (digitalRead(FLASH_BTN) == LOW) {
        Serial.println("Botão FLASH pressionado: log zerado.");
        if (LittleFS.exists("/log.txt")) LittleFS.remove("/log.txt");
        if (SD.exists("/log.txt")) SD.remove("/log.txt");
        delay(500);
    }

    if (!waitingBlock) {
        logAmbient();
        lastRecvTime = millis();
    }

    if (waitingBlock) {
        if (receivedCount >= QTDE_TX || millis() - lastRecvTime > TIMEOUT_MS) {
            closeBlock();
        }
    }
}

#endif // ESP32_RX_H