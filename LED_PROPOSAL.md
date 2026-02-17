# RGB LED Support — 6x WS2812B (USB-Powered)

## Summary

6 individually addressable WS2812B LEDs connected to the ESP32S3 for feeding indicators, ambient lighting, and status feedback. Powered directly from the ESP32's USB 5V rail — no separate PSU needed.

---

## Hardware

### Components

| Item | Qty | Notes |
|------|-----|-------|
| WS2812B LED (individual, through-hole or PCB) | 6 | ~60mA each at full white, 360mA max total |
| 330Ω resistor | 1 | Data line protection (GPIO1 → first LED DIN) |
| 100µF capacitor | 1 | Across 5V/GND near LEDs (optional but recommended) |
| Hookup wire | — | Chain DIN→DOUT between LEDs |

### Why USB Power Is Fine

- 6 LEDs × 60mA = **360mA max** (full white, full brightness)
- Typical use at 30-50% brightness: **~100-180mA**
- USB 5V from ESP32 easily handles this (USB spec: 500mA+)
- No external PSU, no level shifter needed for short runs

### Wiring

```
ESP32S3 XIAO
  GPIO1 (D0) ──[330Ω]──→ LED1 DIN
  5V ─────────────────→ LED1-6 VCC (daisy-chain)
  GND ────────────────→ LED1-6 GND (daisy-chain)

LED chain: LED1 DOUT → LED2 DIN → ... → LED6 DIN
```

### Pin Choice

- **GPIO1 (D0)**: Free, PWM-capable, on exposed header
- GPIO2 already used by servo

---

## Firmware

### Library

**Adafruit NeoPixel** — lightweight, well-supported on ESP32S3.

### API Endpoint

```
GET /led?mode=<mode>[&r=R&g=G&b=B&brightness=BR]
```

| Mode | Description | Parameters |
|------|-------------|------------|
| `off` | All LEDs off | — |
| `ambient` | Time-of-day adaptive colors (sunrise/day/sunset/moonlight) | — |
| `feed` | Green pulse (3 flashes) | — |
| `solid` | All LEDs set to custom color | `r`, `g`, `b`, `brightness` (0-255) |

### LED Modes

| Mode | Behavior |
|------|----------|
| **Feeding indicator** | 3× green pulse, auto-triggers on every feed event |
| **Ambient** | Sunrise orange (7-10h) → Cool white (10-17h) → Sunset (17-21h) → Moonlight blue (21+) |
| **Solid color** | Any RGB via API — party mode, viewing light, etc. |
| **Status: WiFi error** | Red on LED #1 |
| **Status: RTC error** | Yellow blink on LED #1 |

### Status JSON

The `/status` endpoint now includes LED state:

```json
{
  "led": {
    "mode": "ambient",
    "count": 6,
    "brightness": 60
  }
}
```

---

## n8n Integration

Same HTTP pattern as existing endpoints:

```
# Feeding + LED coordination
Cron (09:00) → GET /feed-now → Wait 5s → GET /led?mode=ambient

# Scheduled lighting
Cron (07:00) → GET /led?mode=ambient
Cron (22:00) → GET /led?mode=solid&r=0&g=0&b=30&brightness=10
Cron (00:00) → GET /led?mode=off

# Manual / on-demand
Webhook → GET /led?mode=solid&r=255&g=100&b=0&brightness=80
```

---

## Bill of Materials

| Item | ~Cost (INR) |
|------|-------------|
| 6x WS2812B LEDs (individual or breakout) | ₹60–120 |
| 330Ω resistor | ₹2 |
| 100µF capacitor | ₹5 |
| Hookup wire | ₹20 |
| **Total** | **~₹90–150** |

---

## Future Ideas

- **WLED on a second ESP32-C3** for fancy animations if needed
- **SK6812 RGBW** for better white light
- **LDR sensor** for auto-brightness based on room light
- **Per-LED control** endpoint for individual LED colors
