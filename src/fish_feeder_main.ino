#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include "esp_camera.h"

// ====== CONFIG ======
const char* WIFI_SSID     = "YOUR_SSID";
const char* WIFI_PASSWORD = "YOUR_PASSWORD";

const int SERVO_PIN = D2;      // adjust if needed
const int SERVO_FEED_START = 0;    // resting angle
const int SERVO_FEED_END   = 180;  // 180° feed rotation
const int SERVO_FEED_DELAY_MS = 800; // time to reach angle

// ====== GLOBALS ======
WebServer server(80);
Servo feederServo;

// Camera pin config for XIAO ESP32S3 Sense (may need tweaking to match Seeed example)
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     10
#define SIOD_GPIO_NUM     40
#define SIOC_GPIO_NUM     39

#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       11
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       16
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       17
#define Y2_GPIO_NUM       15
#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     47
#define PCLK_GPIO_NUM     13

// ====== FEEDING ======
void performFeed() {
  Serial.println("[FEED] Feeding started");

  // rotate to dump
  feederServo.write(SERVO_FEED_END);
  delay(SERVO_FEED_DELAY_MS);

  // rotate back
  feederServo.write(SERVO_FEED_START);
  delay(SERVO_FEED_DELAY_MS);

  Serial.println("[FEED] Feeding done");
}

// ====== CAMERA ======
bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  config.frame_size   = FRAMESIZE_QVGA;
  config.jpeg_quality = 12;
  config.fb_count     = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[CAM] Init failed 0x%x\n", err);
    return false;
  }
  Serial.println("[CAM] Init OK");
  return true;
}

// ====== HTTP HANDLERS ======
void handleRoot() {
  String html = R"HTML(
<!DOCTYPE html>
<html>
  <head><meta charset="utf-8"><title>Fish Feeder</title></head>
  <body>
    <h1>Fish Feeder</h1>
    <p><a href="/feed-now"><button>Feed Now</button></a></p>
    <p><img src="/snapshot" style="max-width: 100%; height: auto;"></p>
  </body>
</html>
)HTML";
  server.send(200, "text/html", html);
}

void handleFeedNow() {
  performFeed();
  server.send(200, "text/plain", "Feeding triggered\n");
}

void handleSnapshot() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    server.send(500, "text/plain", "Camera capture failed");
    return;
  }
  server.setContentLength(fb->len);
  server.send(200, "image/jpeg", "");
  WiFiClient client = server.client();
  client.write(fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

// ====== SETUP / LOOP ======
void setup() {
  Serial.begin(115200);
  delay(1000);

  // Servo
  feederServo.attach(SERVO_PIN);
  feederServo.write(SERVO_FEED_START);

  // Camera
  if (!initCamera()) {
    Serial.println("Camera init failed, rebooting...");
    delay(3000);
    ESP.restart();
  }

  // Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("Connecting to %s", WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Web server routes
  server.on("/", handleRoot);
  server.on("/feed-now", handleFeedNow);
  server.on("/snapshot", handleSnapshot);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
  // later: add scheduler here for automatic feeds
}
