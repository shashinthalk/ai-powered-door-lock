#include <WiFi.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>

const char* ssid = "A1-5C26A0E1";
const char* password = "AwGlgkw3f1pBVn";

// --- UDP Discovery parameters ---
WiFiUDP udp;
const unsigned int UDP_PORT = 4210;
String receiverHost = ""; // will be auto-discovered

bool discoverReceiver(unsigned long timeout_ms = 1000) {
  receiverHost = "";
  udp.begin(UDP_PORT + 1); // use a different port locally
  udp.beginPacket("255.255.255.255", UDP_PORT);
  udp.print("DISCOVER_ESP32");
  udp.endPacket();

  unsigned long start = millis();
  while (millis() - start < timeout_ms) {
    int size = udp.parsePacket();
    if (size) {
      char reply[64];
      int len = udp.read(reply, sizeof(reply)-1);
      if (len > 0) reply[len] = 0;
      if (strncmp(reply, "ESP32_HERE:", 11) == 0) {
        receiverHost = String(reply + 11);
        Serial.print("Discovered receiver IP: ");
        Serial.println(receiverHost);
        udp.stop();
        return true;
      }
    }
    delay(10);
  }
  udp.stop();
  Serial.println("Receiver discovery failed.");
  return false;
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  if (!MDNS.begin("esp32cam")) {
    Serial.println("Error starting mDNS");
    while (1) delay(1000);
  }
  Serial.println("mDNS responder started as esp32cam.local");

  while (!discoverReceiver()) {
    Serial.println("Retrying UDP discovery in 2 sec...");
    delay(2000);
  }

  Serial.print("Current receiverHost: ");
  Serial.println(receiverHost);
  Serial.println("Ready for serial input...");
}

void loop() {
  if (receiverHost == "") {
    // If not discovered, try again
    if (!discoverReceiver()) {
      delay(2000);
      return;
    }
  }

  if (Serial.available()) {
    String msg = Serial.readStringUntil('\n');
    msg.trim();
    if (msg.length() == 0) return;

    // Re-discover if "find" is sent
    if (msg == "find") {
      if (discoverReceiver()) {
        Serial.print("Current receiverHost: ");
        Serial.println(receiverHost);
      }
      return;
    }

    WiFiClient client;
    HTTPClient http;
    String url = String("http://") + receiverHost + "/";
    http.begin(client, url);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    int httpCode = http.POST("msg=" + msg);

    if (httpCode > 0) {
      String response = http.getString();
      if (response == "success") {
        Serial.println("success");
      } else {
        Serial.println("fail");
      }
    } else {
      Serial.println("fail");
      // If failed, force re-discovery before next send
      receiverHost = "";
    }
    http.end();
  }
}
