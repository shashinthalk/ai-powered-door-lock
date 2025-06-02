#pragma once
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>

extern WiFiUDP udp;
extern String receiverHost;
extern const unsigned int UDP_PORT;
extern char udpCmdBuf[64];

inline void connectWiFi() {
  const char* ssid = "A1-5C26A0E1";
  const char* password = "AwGlgkw3f1pBVn";
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
}

inline bool startMDNS(const char* hostname) {
  if (!MDNS.begin(hostname)) {
    Serial.println("Error starting mDNS");
    return false;
  }
  Serial.printf("mDNS responder started as %s.local\n", hostname);
  return true;
}

inline String readUDPCommand() {
  int len = udp.read(udpCmdBuf, sizeof(udpCmdBuf) - 1);
  if (len > 0) udpCmdBuf[len] = 0;
  String cmd = String(udpCmdBuf);
  cmd.trim();
  return cmd;
}

inline bool discoverReceiver(unsigned long timeout_ms = 1000) {
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

// Global variable definitions
WiFiUDP udp;
String receiverHost = "";
const unsigned int UDP_PORT = 4210;
char udpCmdBuf[64];
