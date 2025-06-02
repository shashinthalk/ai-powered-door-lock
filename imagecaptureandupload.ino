#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_camera.h"

// WiFi credentials
const char* ssid = "A1-5C26A0E1";
const char* password = "AwGlgkw3f1pBVn";

// Server endpoint
const char* serverUrl = "https://renukacityhotel.lk/api/";

// Camera pin config for AI Thinker ESP32-CAM module
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

void setupCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 12;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_CIF;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  Serial.println("Initializing camera...");
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.println("Camera init failed. Error code: " + String(err));
    while (true) delay(1000);
  } else {
    Serial.println("Camera initialized");
  }
}

void uploadImage(camera_fb_t * fb) {
  if (!fb) {
    Serial.println("Camera capture failed (null frame)");
    return;
  }

  HTTPClient http;
  http.begin(serverUrl);
  http.addHeader("Content-Type", "image/jpeg");

  int httpResponseCode = http.POST(fb->buf, fb->len);
  if (httpResponseCode > 0) {
    Serial.println("Server response: " + http.getString());
  } else {
    Serial.println("POST failed: " + http.errorToString(httpResponseCode));
  }

  http.end();
  esp_camera_fb_return(fb);
}

void captureAndUpload() {
  Serial.println("Capturing image...");
  camera_fb_t * fb = esp_camera_fb_get();
  uploadImage(fb);
}

void setup() {
  Serial.begin(115200);
  Serial.println("Booting ESP32-CAM...");

  WiFi.begin(ssid, password);
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(500);
    Serial.println("Connecting to WiFi...");
    retry++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected");
    Serial.println("IP address: " + WiFi.localIP().toString());
    setupCamera();
  } else {
    Serial.println("Failed to connect to WiFi");
    while (true) delay(1000);
  }
}

void loop() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input == "y") {
      Serial.println("Capture command received");
      captureAndUpload();
    }
  }
}
