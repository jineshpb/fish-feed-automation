# Fish Feed Automation

Single-brain automated fish feeder + camera using Seeed XIAO ESP32S3 Sense.

## Features (v0)
- Servo-based drum feeder (one feed = 180° rotation and back).
- XIAO ESP32S3 Sense hosts a tiny web UI:
  - `GET /` → "Feed Now" button + latest snapshot.
  - `GET /feed-now` → triggers a feed event.
  - `GET /snapshot` → returns a JPEG from the camera.

## Hardware
- Seeed Studio XIAO ESP32S3 Sense
- 1x small servo for feeder drum (connected to a PWM-capable pin, default `D2` in sketch)

## Setup
1. Open `src/fish_feeder_main.ino` in Arduino IDE.
2. Set your Wi-Fi credentials in `WIFI_SSID` and `WIFI_PASSWORD`.
3. Select the XIAO ESP32S3 board in Arduino.
4. Flash the sketch.
5. Open Serial Monitor to see the assigned IP address.
6. Visit `http://<ip>/` in a browser on the same network to use the web UI.

Camera pin mapping currently follows Seeed XIAO ESP32S3 Sense examples and may need tweaking if Seeed updates their reference design.
