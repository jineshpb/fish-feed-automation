#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include "RTClib.h"
#include "esp_camera.h"

// ====== CONFIG ======
const char* WIFI_SSID     = "YOUR_SSID";
const char* WIFI_PASSWORD = "YOUR_PASSWORD";

const int SERVO_PIN = 2;       // GPIO 2 (PWM-capable)
const int SERVO_FEED_START = 0;    // resting angle
const int SERVO_FEED_END   = 180;  // 180° feed rotation
const int SERVO_FEED_DELAY_MS = 800; // time to reach angle

// ====== GLOBALS ======
WebServer server(80);
Servo feederServo;
RTC_DS3231 rtc;  // real-time clock (DS3231)

// Feed schedule: morning and evening at 09:00 and 21:00
const int FEED_MORNING_HOUR = 9;
const int FEED_MORNING_MIN  = 0;
const int FEED_EVENING_HOUR = 21;
const int FEED_EVENING_MIN  = 0;

// Track last day we fed (so we don't double-feed)
int lastFeedDayMorning = -1;
int lastFeedDayEvening = -1;

// Last feed activity capture (numeric only)
int lastMotionScore     = -1;
int feedSequence        = 0;   // increments every time a feed happens

// Camera pin config for XIAO ESP32S3 Sense
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

// ====== FEEDING / ACTIVITY CAPTURE ======
int computeMotionScore(camera_fb_t* a, camera_fb_t* b) {
  if (!a || !b) return -1;
  // v0 heuristic: difference in JPEG size
  return abs((int)a->len - (int)b->len);
}

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

void performFeedWithCapture() {
  Serial.println("[FEED] Capture before/after (ephemeral)");

  camera_fb_t* before = esp_camera_fb_get();
  if (!before) {
    Serial.println("[FEED] Failed to get BEFORE frame");
    performFeed();
    return;
  }

  performFeed();

  camera_fb_t* after = esp_camera_fb_get();
  if (!after) {
    Serial.println("[FEED] Failed to get AFTER frame");
    esp_camera_fb_return(before);
    return;
  }

  lastMotionScore = computeMotionScore(before, after);
  feedSequence++;

  esp_camera_fb_return(before);
  esp_camera_fb_return(after);

  Serial.print("[FEED] Motion score: ");
  Serial.println(lastMotionScore);
  Serial.print("[FEED] Sequence: ");
  Serial.println(feedSequence);
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

  config.frame_size   = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location  = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count     = 1;

  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      config.jpeg_quality = 10;
      config.fb_count     = 2;
      config.grab_mode    = CAMERA_GRAB_LATEST;
    } else {
      config.frame_size   = FRAMESIZE_SVGA;
      config.fb_location  = CAMERA_FB_IN_DRAM;
    }
  } else {
    config.frame_size = FRAMESIZE_240X240;
  #if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
  #endif
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[CAM] Init failed with error 0x%x\n", err);
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
    <p><a href="/status">Status JSON</a></p>
  </body>
</html>
)HTML";
  server.send(200, "text/html", html);
}

void handleFeedNow() {
  performFeedWithCapture();
  server.send(200, "text/plain", "Feeding (with capture) triggered\n");
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

void handleStatus() {
  DateTime now = rtc.now();
  String json = "{";

  json += "\"time\":{";
  json += "\"year\":" + String(now.year()) + ",";
  json += "\"month\":" + String(now.month()) + ",";
  json += "\"day\":" + String(now.day()) + ",";
  json += "\"hour\":" + String(now.hour()) + ",";
  json += "\"minute\":" + String(now.minute()) + ",";
  json += "\"second\":" + String(now.second()) + "},";

  json += "\"morning\":{";
  json += "\"hour\":" + String(FEED_MORNING_HOUR) + ",";
  json += "\"minute\":" + String(FEED_MORNING_MIN) + ",";
  json += "\"fedToday\":" + String(lastFeedDayMorning == now.day() ? "true" : "false") + "},";

  json += "\"evening\":{";
  json += "\"hour\":" + String(FEED_EVENING_HOUR) + ",";
  json += "\"minute\":" + String(FEED_EVENING_MIN) + ",";
  json += "\"fedToday\":" + String(lastFeedDayEvening == now.day() ? "true" : "false") + "},";

  json += "\"lastFeed\":{";
  json += "\"motionScore\":" + String(lastMotionScore) + ",";
  json += "\"seq\":" + String(feedSequence);
  json += "}}";

  server.send(200, "application/json", json);
}

void handleSetTime() {
  if (!server.hasArg("epoch")) {
    server.send(400, "text/plain", "Missing epoch param\n");
    return;
  }
  unsigned long epoch = server.arg("epoch").toInt();
  if (epoch == 0) {
    server.send(400, "text/plain", "Invalid epoch\n");
    return;
  }
  rtc.adjust(DateTime(epoch));
  server.send(200, "text/plain", "RTC updated\n");
}

// ====== SETUP / LOOP ======
void setup() {
  Serial.begin(115200);
  delay(1000);

  // Servo
  feederServo.attach(SERVO_PIN);
  feederServo.write(SERVO_FEED_START);

  // RTC
  if (!rtc.begin()) {
    Serial.println("[RTC] Failed to initialize RTC (DS3231)");
  } else {
    if (rtc.lostPower()) {
      Serial.println("[RTC] RTC lost power, set the time via /set-time once.");
    }
  }

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
  server.on("/status", handleStatus);
  server.on("/set-time", handleSetTime);

  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();

  // Simple RTC-based scheduler: feed at 09:00 and 21:00 once per day
  DateTime now = rtc.now();

  // Morning feed
  if (now.hour() == FEED_MORNING_HOUR && now.minute() == FEED_MORNING_MIN) {
    if (lastFeedDayMorning != now.day()) {
      Serial.println("[SCHED] Morning feed triggered");
      performFeedWithCapture();
      lastFeedDayMorning = now.day();
    }
  }

  // Evening feed
  if (now.hour() == FEED_EVENING_HOUR && now.minute() == FEED_EVENING_MIN) {
    if (lastFeedDayEvening != now.day()) {
      Serial.println("[SCHED] Evening feed triggered");
      performFeedWithCapture();
      lastFeedDayEvening = now.day();
    }
  }
}
