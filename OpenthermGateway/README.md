---
title: HomeMaster OpenTherm Gateway
date-published: 2025-08-19
type: relay
standard: global
board: esp32
project-url: https://github.com/isystemsautomation/HOMEMASTER/tree/main/OpenthermGateway
made-for-esphome: True
difficulty: 1
---

## HomeMaster OpenTherm Gateway

![Device](./opentherm.png)

## Description

The HomeMaster OpenTherm Gateway is an ESP32-based DIN-rail device designed to interface with OpenTherm-compatible boilers.

The device provides a hardware OpenTherm interface together with relay outputs, a digital input, and 1-Wire temperature sensor support. It is designed for local operation using ESPHome and integrates directly with Home Assistant.

This page provides a minimal working ESPHome configuration. The full configuration used for the shipped device and vendor OTA updates is available in the project repository.

- Maker: https://www.home-master.eu/
- Product page: https://www.home-master.eu/shop/esp32-opentherm-gateway-59
- Repository: https://github.com/isystemsautomation/HOMEMASTER/tree/main/OpenthermGateway

## Features

- ESP32-WROOM-32U
- OpenTherm interface
- Relay output
- Digital input
- Two 1-Wire buses
- USB Type-C
- Wi-Fi and Bluetooth
- ESPHome pre-installed
- OTA updates (ESPHome + HTTP)
- Improv provisioning
- DIN-rail mounting

## Pinout

![Pinout](./pinout.png)

## Getting Started

The device supports two setup methods:

- **Improv Wi-Fi (recommended)**
- **Fallback Access Point (HomeMaster OT Fallback)**

### Improv Wi-Fi Setup (Recommended)

1. Power on the device
2. Open https://improv-wifi.com
3. Connect via USB or Bluetooth
4. Enter Wi-Fi credentials
5. Wait for connection

After connection, the device will appear automatically in:

- ESPHome Dashboard
- Home Assistant

Click **Take Control** to import the full configuration.

### Fallback Access Point (HomeMaster OT Fallback)

If the device cannot connect to Wi-Fi, it starts a fallback Access Point.

**SSID:** `HomeMaster OT Fallback`

#### Steps

1. Power on the device and wait approximately 60 seconds
2. Connect to: HomeMaster OT Fallback
3. Open a browser and navigate to: http://192.168.4.1
4. Enter your Wi-Fi credentials and save

The device will restart and connect to your network.

### Notes

- The captive portal page may open automatically. If it does not, open `http://192.168.4.1` manually.
- Mobile devices may continue using mobile data; disable it if the page does not load.
- The fallback Access Point is only active when the device cannot connect to Wi-Fi.
- Improv Wi-Fi is the preferred setup method.

## Firmware Updates

The device supports two firmware update methods:

### ESPHome Updates (User-controlled)

After taking control in ESPHome Dashboard, firmware can be updated manually:

- Build new firmware from ESPHome
- Upload via OTA or USB
- Full control over configuration

### Managed Updates (HTTP)

The device also supports vendor-provided firmware updates.

A firmware update entity is exposed in Home Assistant, allowing the device to check for new firmware versions and install updates directly.

This mechanism uses:

- `update.http_request`
- a hosted firmware manifest
- OTA firmware downloads over HTTPS

If a newer firmware version is available, it can be installed directly from Home Assistant.

## Example Entities

The example configuration below exposes:

- Button
- Relay
- Status LED
- Boiler Water Temperature
- Boiler Flame On
- Boiler Fault Indication
- 1-Wire Bus 1 Temperature
- 1-Wire Bus 2 Temperature
- Firmware Update

Additional OpenTherm entities are available in the full configuration.

## Basic ESPHome Configuration

```yaml
esphome:
  name: homemaster-opentherm
  friendly_name: HomeMaster OpenTherm Gateway
  project:
    name: homemaster.opentherm_gateway
    version: "1.0.4"

esp32:
  board: esp32dev
  framework:
    type: esp-idf

logger:

api:

wifi:
  ap:
    ssid: "HomeMaster OT Fallback"

captive_portal:

esp32_improv:
  authorizer: none

improv_serial:

dashboard_import:
  package_import_url: github://isystemsautomation/HOMEMASTER/OpenthermGateway/Firmware/opentherm.yaml@main
  import_full_config: true

http_request:

ota:
  - platform: esphome
  - platform: http_request

update:
  - platform: http_request
    id: firmware_update
    name: "Firmware Update"
    source: https://isystemsautomation.github.io/HOMEMASTER/OpenthermGateway/Firmware/manifest.json

opentherm:
  id: ot_bus
  in_pin: GPIO21
  out_pin: GPIO26
  ch_enable: true
  dhw_enable: true

binary_sensor:
  - platform: gpio
    id: button_1
    name: "Button"
    pin:
      number: GPIO35
      inverted: true
      mode:
        input: true

  - platform: opentherm
    fault_indication:
      id: ot_fault_indication
      name: "Boiler Fault Indication"
      entity_category: diagnostic
    flame_on:
      id: ot_flame_on
      name: "Boiler Flame On"

one_wire:
  - platform: gpio
    id: ow_bus_1
    pin: GPIO4

  - platform: gpio
    id: ow_bus_2
    pin: GPIO5

sensor:
  - platform: opentherm
    t_boiler:
      id: ot_t_boiler
      name: "Boiler Water Temperature"
      unit_of_measurement: "°C"

  - platform: dallas_temp
    id: ow_bus_1_temperature
    one_wire_id: ow_bus_1
    name: "1-Wire Bus 1 Temperature"
    unit_of_measurement: "°C"

  - platform: dallas_temp
    id: ow_bus_2_temperature
    one_wire_id: ow_bus_2
    name: "1-Wire Bus 2 Temperature"
    unit_of_measurement: "°C"

switch:
  - platform: gpio
    id: relay_1
    name: "Relay"
    pin: GPIO32

status_led:
  pin:
    number: GPIO33
    inverted: true
```
