# atmosedge-proto-esp32

A lightweight ESP32-based environmental monitoring system that reads **CO₂, temperature, humidity, VOC, NOx, and atmospheric pressure** using and exposes them over an **HTTP REST endpoint (`/metrics`)** in JSON format.

---

## Features

- HTTP/REST endpoint (`GET /metrics`)
- Real-time sensor data acquisition
- Multi-sensor support via I²C:
  - **SCD41** → CO₂ (ppm), Temperature (°C), Humidity (%)
  - **SGP41** → VOC Index, NOx Index
  - **LPS22HB** → Pressure (hPa)
- Metrics sampling every **2 seconds**
- Wi-Fi STA mode
- Fully compatible with **ESP-IDF v5.4+**

---

## HTTP Endpoint

### GET `/metrics`

```json
{
  "co2_ppm": 812.5,
  "temperature_c": 23.41,
  "humidity_rh": 44.8,
  "voc_index": 18,
  "nox_index": 12,
  "pressure_hpa": 1013.24
}
```

### Error example (sensor lock)

```json
{
  "error": "metrics locked"
}
```

---

## Hardware Setup

| Device  | I²C Address | Function             |
| ------- | ----------- | -------------------- |
| SCD41   | `0x62`      | CO₂, Temp, Humidity  |
| SGP41   | `0x59`      | VOC/NOx              |
| LPS22HB | `0x5C`      | Atmospheric Pressure |

---

## Wiring Diagram (ESP32 ↔ Sensors)

```text
ESP32                   Sensor Module
---------------------------------------
3V3  ------------------> VCC
GND  ------------------> GND
GPIO 8 (I2C_SDA)   ----> SDA
GPIO 9 (I2C_SCL)   ----> SCL
```

> Use 4.7kΩ pull-ups on SDA/SCL if not provided on the sensor breakout.

---

## Setup Instructions

### Clone Repository

```bash
git clone https://github.com/yourname/atmosedge-proto-esp32.git
cd atmosedge-proto-esp32
```

### Configure Wi-Fi

Edit `main.c`:

```c
.ssid = "YOUR_SSID",
.password = "YOUR_PASS",
```

### Build & Flash

```bash
idf.py set-target esp32
idf.py build
idf.py flash monitor
```

---

## Example Log Output

```
air-sensors: Sensors initialized
air-sensors: CO2: 805.7 ppm, Temp: 23.42 C, RH: 44.8%, VOC: 19, NOx: 8, Pressure: 1013.18 hPa
atmosedge-proto: HTTP server started, GET /metrics for data
```

---

## Architecture Overview

```text
 ┌────────────┐      ┌───────────────┐
 │  Sensors   │----->│ air_sensors.c │
 └────────────┘ I2C  └───────────────┘
                         ↓
                ┌──────────────────┐
                │ metrics_task     │
                └──────────────────┘
                         ↓
                ┌──────────────────┐
                │ HTTP Server (/metrics)
                └──────────────────┘
                         ↓
                    Client / API
```

---

## Future Enhancements

- MQTT publish (e.g. `mosquitto-pub`)
- Prometheus/OpenMetrics format
- OTA support
- Wi-Fi manager + captive portal
- Overpressure and VOC alert system

---

## Troubleshooting

| Issue                      | Solution                                    |
| -------------------------- | ------------------------------------------- |
| `No readings from sensors` | Check SDA/SCL wiring & pull-ups             |
| `I2C init failed`          | Make sure no other task takes I2C           |
| `metrics locked`           | Increase mutex timeout                      |
| `0 ppm CO₂`                | Sensor warm-up: wait ~5 sec before sampling |

---
