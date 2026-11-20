# AquaOracle: ESP32 firmware

[![Build](https://github.com/Vuduykhang2306/Esp32_AquaOracle_Programm/actions/workflows/build.yml/badge.svg)](https://github.com/Vuduykhang2306/Esp32_AquaOracle_Programm/actions/workflows/build.yml)

Firmware for the AquaOracle sensor node. It measures the water in an
aquaculture pond and pushes the readings to Supabase.

The mobile app that reads this data:
[aquaoracle-app](https://github.com/Vuduykhang2306/aquaoracle-app)

## Hardware

| Part | Model |
|---|---|
| Microcontroller | ESP32-WROOM |
| Temperature | DS18B20 (1-Wire) |
| pH | Analog pH Meter Kit V2 |
| TDS | analog TDS sensor |
| Turbidity | analog turbidity sensor |
| Power | 12V battery, can be paired with a solar panel |

Everything sits in a waterproof plastic box. Pond water runs through a
measuring chamber in PVC pipe.

## How it works

1. On boot: set up the four sensors and connect to Wi-Fi.
2. Every 2 minutes (`SEND_INTERVAL`): read each analog sensor several times and
   average the readings to cut noise.
3. Compare against safe thresholds and raise an alert if one is crossed.
4. Pack the values into JSON, POST them to the Supabase REST API over HTTPS,
   and check that the write went through.

Each record carries an `esp_id`, so the app can tell several devices apart on
one account.

`hieu_chinh_pH.ino` holds the two-point pH calibration (pH 7.0 and pH 4.0
buffers). The default voltages in it are placeholders and have to be measured
again on the probe you actually use.

## Setup

```bash
cp secrets.example.h secrets.h   # then fill in Wi-Fi SSID, password and Supabase key
```

`secrets.h` is in `.gitignore`.

Open `Esp32_Program_AquaOracle.ino` in the Arduino IDE, pick the ESP32 Dev
Module board and upload.

CI compiles the sketch on every push with `secrets.example.h` standing in for
`secrets.h`, so a broken build shows up without real credentials.

## Security note

Earlier versions of this repo had the Wi-Fi SSID, the Wi-Fi password and the
Supabase anon key hardcoded in the `.ino` file, and that was pushed to a public
repo. I rewrote the git history with `git filter-repo` to remove them, changed
the Wi-Fi password and deleted the old Supabase project. Config now lives on
its own in `secrets.h`.

## Status

The contest is over. The code stays up for reference. To run it again you need
a new Supabase project.

Tiếng Việt: [README.vi.md](README.vi.md)
