// =======================================================
// Definição de papel via build_flags
// =======================================================
#if defined(ESP8266_TX)
  #define ROLE "TRANSMISSOR"
#elif defined(ESP8266_RX)
  #define ROLE "RECEPTOR"
#elif defined(ESP8266_MAC)
  #define ROLE "ESP8266_MAC"
#elif defined(ESP32_MAC)
  #define ROLE "ESP32_MAC"
#else
  #error "Nenhum papel definido! Use -DTRANSMISSOR ou -DRECEPTOR."
#endif

// =======================================================
// Includes comuns
// =======================================================
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <espnow.h>

// Estrutura de dados comum
struct SensorData {
  uint8_t nodeId;
  float temp;
};

// =======================================================
// ===============  BLOCO TRANSMISSOR ====================
// =======================================================
#if defined(ESP8266_TX)
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


// =======================================================
// ===============  BLOCO RECEPTOR =======================
// =======================================================
#if defined(ESP8266_RX)

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
const char* ssid = "ESP_LOG_AP";
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

// =======================================================
// ===============  BLOCO MAC ESP8266 ====================
// =======================================================
#if defined(ESP8266_MAC)

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

// =======================================================
// ================  BLOCO MAC ESP32 =====================
// =======================================================
#if defined(ESP32_MAC)
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