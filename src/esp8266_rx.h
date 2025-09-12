#ifndef ESP8266_RX_H
#define ESP8266_RX_H

    #include <OneWire.h>
    #include <DallasTemperature.h>
    #include <Wire.h>
    #include <RTClib.h>
    #include <FS.h>             
    #include <ESP8266WebServer.h>

    #define LED_PIN 2 // LED interno
    #define ONEWIRE_PIN D2   // pino do DS18B20

    #ifndef QTDE_TX
    #define QTDE_TX 1   // valor padrão, caso não seja definido no build_flags
    #endif

    RTC_DS3231 rtc;
    ESP8266WebServer server(80);
    OneWire oneWire(ONEWIRE_PIN);
    DallasTemperature sensors(&oneWire);

    // SSID e senha do AP
    const char* ssid = "RECEPTOR";
    const char* password = "12345678";

    String pad2(int value) {
    return (value < 10 ? "0" : "") + String(value);
    }

    // Função callback quando recebe dados
    void onDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
    if(len != sizeof(SensorData)) return;

    SensorData data;
    memcpy(&data, incomingData, sizeof(data));

    // Pisca LED
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);

    // Pega horário atual
    DateTime now = rtc.now();

    // Cria a string de log
    String logEntry = pad2(now.day()) + "/" + pad2(now.month()) + "/" + String(now.year()) + " ";
    logEntry += pad2(now.hour()) + ":" + pad2(now.minute()) + ":" + pad2(now.second());
    logEntry += " - Cx: " + String(data.nodeId) + " | Temp: " + String(data.temp, 2) + " Graus\n";

    // Mostra na serial
    Serial.print(logEntry);

    // Salva no log.txt
    File logFile = SPIFFS.open("/log.txt", "a");
    if(logFile){
        logFile.print(logEntry);
        logFile.close();
    }
    else {
        Serial.println("Erro ao abrir log.txt");
    }
    }

    // Rota para exibir o log
    void handleLog() {
    if(SPIFFS.exists("/log.txt")) {
        File logFile = SPIFFS.open("/log.txt", "r");
        if(logFile){
        server.streamFile(logFile, "text/plain");
        logFile.close();
        }
    } else {
        server.send(200, "text/plain", "Nenhum log encontrado.");
    }
    }

    void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    // Inicia RTC
    if (!rtc.begin()) {
        Serial.println("Erro: RTC DS3231 não encontrado!");
        while (1);
    }

    // Inicia SPIFFS
    if(!SPIFFS.begin()){
        Serial.println("Falha ao montar SPIFFS");
        return;
    }

    // Inicia DS18B20
    sensors.begin();
    sensors.setResolution(12);

    // Configura como Access Point
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(ssid, password);
    WiFi.disconnect();
    Serial.print("AP iniciado. Conecte-se em: ");
    Serial.println(WiFi.softAPIP());

    // Inicia servidor web
    server.on("/", [](){
        server.send(200, "text/html", "<h1>Servidor ESP8266</h1><p><a href=\"/log\">Ver log</a></p>");
    });
    server.on("/log", handleLog);
    server.begin();

    // Inicializa ESP-NOW
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

    // Leitura da temperatura ambiente a cada 10 segundos
    static uint32_t lastRead = 0;
    if (millis() - lastRead > 10000) {  // 10 segundos
        lastRead = millis();

        sensors.requestTemperatures();
        float ambientTemp = sensors.getTempCByIndex(0);
        //Serial.printf("Temperatura ambiente: %.2f°C\n", ambientTemp);

        // Opcional: salvar no log.txt
        DateTime now = rtc.now();
        String logEntry = pad2(now.day()) + "/" + pad2(now.month()) + "/" + String(now.year()) + " ";
        logEntry += pad2(now.hour()) + ":" + pad2(now.minute()) + ":" + pad2(now.second());
        logEntry += " - Ambiente: " + String(ambientTemp, 2) + "°C\n";

        File logFile = SPIFFS.open("/log.txt", "a");
        if(logFile){
        logFile.print(logEntry);
        logFile.close();
        }
    }
    }

#endif // RECEPTOR