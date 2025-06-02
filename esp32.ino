#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFiUdp.h>
#include <math.h>

// OLED parameters
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define OLED_ADDRESS  0x3C
#define OLED_SDA      21
#define OLED_SCL      22

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// WiFi parameters
const char* ssid = "A1-5C26A0E1";
const char* password = "AwGlgkw3f1pBVn";

WebServer server(80);

// Status and animation variables
String statusLine1 = "Starting...";
String statusLine2 = " ";
String lastResponse = "No response";
int battery_level = 77; // percent (mocked)
int spinner_frame = 0;
unsigned long last_anim = 0;
const int anim_speed = 80; // ms

// Log buffer for blue area (6 lines)
#define MAX_LOG_LINES 6
String logLines[MAX_LOG_LINES];

// UDP Discovery
WiFiUDP udp;
const unsigned int UDP_PORT = 4210;

// Helper to add a line to the log (blue area)
void addLog(const String& log) {
  for (int i = 0; i < MAX_LOG_LINES - 1; i++) {
    logLines[i] = logLines[i + 1];
  }
  logLines[MAX_LOG_LINES - 1] = log;
  drawDisplay();
  Serial.println(log);
}

// Draws the OLED screen with status, icons, spinner, and log area.
void drawDisplay() {
  display.clearDisplay();

  // --- STATUS AREA (YELLOW, lines 0-15) ---
  display.fillRect(0, 0, SCREEN_WIDTH, 16, SSD1306_WHITE); // Simulate yellow
  display.setTextSize(1);
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(0, 0);
  display.println(statusLine1);
  display.setCursor(0, 8);
  display.println(statusLine2);

  // Battery icon (top right)
  int batt_x = 120, batt_y = 2, batt_w = 10, batt_h = 6;
  display.drawRect(batt_x, batt_y, batt_w, batt_h, SSD1306_BLACK);
  display.fillRect(batt_x + 1, batt_y + 1, (batt_w-2) * battery_level / 100, batt_h-2, SSD1306_BLACK);
  display.drawRect(batt_x + batt_w, batt_y + 2, 2, 2, SSD1306_BLACK);

  // Spinner animation (arc)
  int cx = 105, cy = 8, r = 4;
  float angle = spinner_frame * 0.4;
  display.fillCircle(cx + cos(angle)*r, cy + sin(angle)*r, 2, SSD1306_BLACK);

  // --- LOG AREA (BLUE, start y=16) ---
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  for (int i = 0; i < MAX_LOG_LINES; i++) {
    display.setCursor(0, 16 + i * 8); // 8px per line
    display.println(logLines[i]);
  }

  // --- Last response (from POST) at bottom
  display.setCursor(0, 16 + MAX_LOG_LINES * 8 + 2);
  display.println("response:");
  display.setCursor(0, 16 + MAX_LOG_LINES * 8 + 10);
  display.println(lastResponse);

  display.display();
}

// Handle incoming HTTP POST from CAM
void handleRoot() {
  String msg = server.arg("msg");
  if (msg.length() > 0) {
    lastResponse = msg;
    statusLine1 = "Received!";
    statusLine2 = "esp32.local";
    addLog("POST: " + msg);
    server.send(200, "text/plain", "success");
  } else {
    statusLine1 = "Received!";
    statusLine2 = "Fail";
    addLog("POST fail (empty)");
    server.send(200, "text/plain", "fail");
  }
  drawDisplay();
}

// Handle UDP discovery requests
void handleUDPDiscovery() {
  int packetSize = udp.parsePacket();
  if (packetSize) {
    char packet[64];
    int len = udp.read(packet, sizeof(packet)-1);
    if (len > 0) packet[len] = 0;
    if (strcmp(packet, "DISCOVER_ESP32") == 0) {
      IPAddress remoteIp = udp.remoteIP();
      udp.beginPacket(remoteIp, udp.remotePort());
      udp.print("ESP32_HERE:");
      udp.print(WiFi.localIP());
      udp.endPacket();
      addLog("UDP Discovery: replied to " + remoteIp.toString());
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);

  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (true);
  }
  display.clearDisplay();
  display.display();

  statusLine1 = "Connecting WiFi";
  statusLine2 = ssid;
  addLog("Booting...");
  addLog("Starting WiFi...");
  drawDisplay();

  WiFi.begin(ssid, password);
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 20) {
    delay(500);
    retries++;
    statusLine2 = String("Wait: ") + (millis()/1000) + "s";
    addLog("WiFi connecting...");
    drawDisplay();
  }

  if (WiFi.status() == WL_CONNECTED) {
    String ip = WiFi.localIP().toString();
    statusLine1 = "WiFi Connected";
    statusLine2 = "esp32.local"; // Always show mDNS name, NOT ip
    addLog("WiFi OK");
    addLog("IP: " + ip);        // Log IP, do not display as status
    drawDisplay();
  } else {
    statusLine1 = "WiFi Failed";
    statusLine2 = "Check credentials";
    addLog("WiFi Failed");
    addLog("Check credentials");
    drawDisplay();
  }

  if (!MDNS.begin("esp32")) {
    statusLine1 = "mDNS failed!";
    statusLine2 = " ";
    addLog("mDNS failed!");
    drawDisplay();
    while (1) delay(1000);
  }
  statusLine1 = "Ready!";
  statusLine2 = "esp32.local";
  addLog("mDNS Ready as esp32.local");
  drawDisplay();

  server.on("/", HTTP_POST, handleRoot);
  server.begin();
  statusLine1 = "Server Ready";
  statusLine2 = "esp32.local";
  addLog("Web server ready");
  drawDisplay();

  // OTA setup
  ArduinoOTA.onStart([]() {
    statusLine1 = "OTA Update";
    statusLine2 = "esp32.local";
    addLog("OTA Start...");
    drawDisplay();
  });
  ArduinoOTA.onEnd([]() {
    statusLine1 = "OTA Done!";
    statusLine2 = "Rebooting...";
    addLog("OTA End. Rebooting...");
    drawDisplay();
    delay(1200);
    statusLine1 = "Ready to use";
    statusLine2 = "esp32.local";
    addLog("WiFi OK");
    addLog("IP: " + WiFi.localIP().toString());
    drawDisplay();
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    char buf[32];
    snprintf(buf, sizeof(buf), "OTA: %u%%", (progress * 100) / total);
    statusLine1 = "OTA Update";
    statusLine2 = "esp32.local";
    addLog(buf);
    drawDisplay();
  });
  ArduinoOTA.onError([](ota_error_t error) {
    statusLine1 = "OTA Error";
    statusLine2 = "esp32.local";
    char buf[32];
    snprintf(buf, sizeof(buf), "OTA Err: %u", error);
    addLog(buf);
    drawDisplay();
  });
  ArduinoOTA.begin();

  // Start UDP discovery service
  udp.begin(UDP_PORT);
  addLog("UDP Discovery ready");
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();
  handleUDPDiscovery();

  // Animate spinner
  unsigned long now = millis();
  if (now - last_anim > anim_speed) {
    spinner_frame = (spinner_frame + 1) % 16;
    drawDisplay();
    last_anim = now;
  }
}
