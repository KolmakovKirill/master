#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

// ==== Настройки WiFi ====
const char* ssid = "test";
const char* password = "test12345";

// ==== Пины ====
const int LDR_PIN = A0;
const int PIR_PIN = D5;
const int LED_PIN = D1;

// ==== Константы ====
const int LIGHT_THRESHOLD = 600;
const unsigned long MOTION_DURATION = 5000;

// ==== Состояния ====
bool motionDetected = false;
bool ledState = false;
bool manualMode = false;
unsigned long motionStartTime = 0;

int lastLightLevel = -1;
bool lastMotionState = false;

// ==== NTP-клиент ====
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3 * 3600, 60000);

// ==== Веб-сервер ====
ESP8266WebServer server(80);

// ==== Функции ====

void logEvent(String message) {
  Serial.println("[" + timeClient.getFormattedTime() + "] " + message);
}

void turnLedOn() {
  if (!ledState) {
    digitalWrite(LED_PIN, HIGH);
    ledState = true;
    logEvent("LED turned ON");
  }
}

void turnLedOff() {
  if (ledState) {
    digitalWrite(LED_PIN, LOW);
    ledState = false;
    logEvent("LED turned OFF");
  }
}

// ==== Обработчики Web ====

void handleRoot() {
  String html = "<html><head><meta charset='UTF-8'><title>Управление светом</title></head><body>";
  html += "<h2>Режим: " + String(manualMode ? "Ручной" : "Авто") + "</h2>";
  html += "<form action='/toggle_mode'><button type='submit'>Переключить режим</button></form><br>";
  html += "<form action='/on'><button type='submit'>Включить свет</button></form>";
  html += "<form action='/off'><button type='submit'>Выключить свет</button></form>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleTurnOn() {
  if (manualMode) turnLedOn();
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

void handleTurnOff() {
  if (manualMode) turnLedOff();
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

void handleToggleMode() {
  manualMode = !manualMode;
  logEvent(String("Mode switched to ") + (manualMode ? "MANUAL" : "AUTO"));
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/on", handleTurnOn);
  server.on("/off", handleTurnOff);
  server.on("/toggle_mode", handleToggleMode);
  server.begin();
  Serial.println("Web server started.");
}

// ==== SETUP ====

void setup() {
  Serial.begin(115200);
  pinMode(PIR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  timeClient.begin();
  timeClient.update();

  setupWebServer();
}

// ==== LOOP ====

void loop() {
  server.handleClient();
  timeClient.update();
  unsigned long now = millis();

  if (!manualMode) {
    int lightLevel = analogRead(LDR_PIN);
    bool motion = (digitalRead(PIR_PIN) == LOW);  // Обратная логика: LOW = движение

    // Логирование изменений
    if (abs(lightLevel - lastLightLevel) > 50) {
      logEvent("Light changed: " + String(lightLevel));
      lastLightLevel = lightLevel;
    }

    if (motion != lastMotionState) {
      logEvent(String("Motion ") + (motion ? "detected" : "stopped"));
      lastMotionState = motion;
    }

    // Реагируем на движение только если темно
    if (motion && lightLevel > LIGHT_THRESHOLD) {
      motionDetected = true;
      motionStartTime = now;
      turnLedOn();
    } else if(lightLevel > LIGHT_THRESHOLD){
      turnLedOn();
    }

    // Гасим по истечении MOTION_DURATION
    if (motionDetected && (now - motionStartTime >= MOTION_DURATION)) {
      motionDetected = false;
      turnLedOff();
    } else if(lightLevel <= LIGHT_THRESHOLD){
      turnLedOff();
    }
  }

  delay(100);
}
