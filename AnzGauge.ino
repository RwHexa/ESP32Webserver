//          AnzGauge.ino   07.03.2026
/*********
 * von RandomNerd, weiterentw. Rw + Ki"claudeAi"
  WebServer: Access Point Modus + Schaltausgang + Slider-Wert
*********/
#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "LittleFS.h"
#include <Arduino_JSON.h>
#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>

/* wichtiger Hinweis zu ESPAsyncWebServer 
hatte zuerst compilier Fehler da Biblio neue ESP32-Core nicht unterstuetzt
in Zukunft Bibliothek von "mathieucarbou" nehmen !
*/

// Access Point credentials
const char* ssid     = "ESP32-Sensor";
const char* password = "12345678";

IPAddress apIP(192, 168, 4, 1);

// --- Ausgaenge ---
#define PIN_OUTPUT  32   // GPIO fuer Schaltausgang (Relais oder LED)
bool outputState = false;
int  sliderValue = 0;

AsyncWebServer server(80);
AsyncEventSource events("/events");
JSONVar readings;

unsigned long lastTime = 0;
unsigned long timerDelay = 4150;

Adafruit_BME280 bme;

void initBME(){
  if (!bme.begin(0x76)) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }
}

String getSensorReadings(){
  readings["temperature"] = String(bme.readTemperature());
  readings["humidity"]    = String(bme.readHumidity());
  return JSON.stringify(readings);
}

void initLittleFS() {
  if (!LittleFS.begin()) {
    Serial.println("An error has occurred while mounting LittleFS");
  }
  Serial.println("LittleFS mounted successfully");
}

void initWiFi() {
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(ssid, password);
  Serial.println("Access Point gestartet");
  Serial.print("AP SSID: "); Serial.println(ssid);
  Serial.print("AP IP:   "); Serial.println(WiFi.softAPIP());
}

void setup() {
  Serial.begin(115200);

  // Ausgang initialisieren
  pinMode(PIN_OUTPUT, OUTPUT);
  digitalWrite(PIN_OUTPUT, LOW);

  initBME();
  initWiFi();
  initLittleFS();

  // Startseite
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/index.html", "text/html");
  });
  server.serveStatic("/", LittleFS, "/");

  // Sensor-Messwerte
  server.on("/readings", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = getSensorReadings();
    request->send(200, "application/json", json);
  });

  // Ausgang toggeln  ->  GET /toggle
  server.on("/toggle", HTTP_GET, [](AsyncWebServerRequest *request){
    outputState = !outputState;
    digitalWrite(PIN_OUTPUT, outputState ? HIGH : LOW);
    Serial.printf("Ausgang: %s\n", outputState ? "EIN" : "AUS");
    String json = "{\"output\":" + String(outputState ? "true" : "false") + "}";
    request->send(200, "application/json", json);
  });

  // Slider-Wert empfangen  ->  GET /slider?value=42
  server.on("/slider", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("value")) {
      sliderValue = request->getParam("value")->value().toInt();
      sliderValue = constrain(sliderValue, 0, 100);
      Serial.printf("Slider-Wert: %d\n", sliderValue);
      // Beispiel PWM: analogWrite(PIN_PWM, map(sliderValue, 0, 100, 0, 255));
    }
    String json = "{\"slider\":" + String(sliderValue) + "}";
    request->send(200, "application/json", json);
  });

  // Aktuellen Status beim Seitenaufruf laden  ->  GET /status
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{\"output\":" + String(outputState ? "true" : "false") +
                  ",\"slider\":"  + String(sliderValue) + "}";
    request->send(200, "application/json", json);
  });

  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID: %u\n", client->lastId());
    }
    client->send("hello!", NULL, millis(), 10000);
  });
  server.addHandler(&events);

  server.begin();
}

void loop() {
  if ((millis() - lastTime) > timerDelay) {
    events.send("ping", NULL, millis());
    events.send(getSensorReadings().c_str(), "new_readings", millis());
    lastTime = millis();
  }
}

