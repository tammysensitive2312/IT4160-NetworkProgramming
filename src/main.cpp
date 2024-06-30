#include<Arduino.h>
#include<WiFi.h>
#include<SPIFFS.h>
#include<ESPAsyncWebServer.h>
#include <ArduinoJson.h>

#define LED1 0
#define HTTP_PORT 80

const char* SSID = "Hoang";
const char* PASSWORD = "hust1234";

struct Led
{
  uint8_t pin;
  bool on;

  void update() {
      digitalWrite(pin, on ? HIGH : LOW);
  }
};

Led led = { LED1, false };
Led onboard_led = { LED_BUILTIN, false };

AsyncWebServer server(HTTP_PORT);
AsyncWebSocket ws("/ws");

void initSPIFFS() {
  if (!SPIFFS.begin()) {
    Serial.println("Cannot mount SPIFFS volume...");
    while (1) {
        onboard_led.on = millis() % 200 < 50;
        onboard_led.update();
    }
  }
}

void notifyClients() {
  DynamicJsonDocument json(1024);
  json["status"] = led.on ? "on" : "off";

  String jsonString;
  serializeJson(json, jsonString);
  ws.textAll(jsonString);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
      DynamicJsonDocument json(1024);
      DeserializationError err = deserializeJson(json, data);

      if (err) {
            Serial.print(F("deserializeJson() failed with code "));
            Serial.println(err.c_str());
            return;
        }

        const char *action = json["action"];
        if (strcmp(action, "toggle") == 0) {
            led.on = !led.on;
            notifyClients();
      }
    }
}

String processor(const String &var) {
    return String(var == "STATE" && led.on ? "on" : "off");
}

void onRootRequest(AsyncWebServerRequest *request) {
  request->send(SPIFFS, "/index.html", "text/html", false, processor);
}

void initWebServer() {
    server.on("/", onRootRequest);
    server.serveStatic("/", SPIFFS, "/");
    server.begin();
}

void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
  Serial.printf("Trying to connect [%s] ", WiFi.macAddress().c_str());
  while (WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
      delay(500);
  }
  Serial.printf(" %s\n", WiFi.localIP().toString().c_str());
}

void onEvent(AsyncWebSocket       *server,  
             AsyncWebSocketClient *client,  
             AwsEventType          type,    
             void                 *arg,     
             uint8_t              *data,
             size_t                len) {

      switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket client #%u disconnected\n", client->id());
            break;
        case WS_EVT_DATA:
            handleWebSocketMessage(arg, data, len);
            break;
        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;
    }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void setup() {

  pinMode(led.pin, OUTPUT);
  pinMode(onboard_led.pin, OUTPUT);

  Serial.begin(115200); delay(500);
  initSPIFFS();
  initWiFi();
  initWebSocket();
  initWebServer();
}

void loop() {
  ws.cleanupClients();

  onboard_led.on = millis() % 1000 < 50;
  led.update();
  onboard_led.update();
}
