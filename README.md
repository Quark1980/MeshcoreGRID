# Meshcore-Touch

![MeshCore Touch](logo/touch.png)

MeshCore is a lightweight, portable C++ library that enables multi-hop packet routing for embedded projects. This specialized **"TOUCH" Edition** is optimized for the Heltec v4 (ESP32-S3), featuring a premium dark-mode UI and full touch interactivity.

> [!WARNING]
> **BETA VERSION**: This project is in active development. Features and UI are subject to change.

## 📱 Premium Tactical Touch UI

![GUI Example](logo/gui_example.png)

> [!NOTE]
> **GUI Disclaimer**: The image above is a **digital mockup** intended to demonstrate the general layout and aesthetic goals of the interface. The actual on-device graphics, icons, and layout details may differ in the current build.

The "TOUCH" edition features a custom-built, high-performance UI designed for a deep tactical aesthetic and mission-critical reliability.

### Key interface features:
- **Tactical Aesthetic**: Deep Tactical Black backgrounds with High-Visibility Tactical Amber/Orange accents.
- **Fluent Navigation**: intuitive 3-icon carousel for quick access to core functions.
- **Ergonomic Design**: Enlarged touch targets and full QWERTY keyboard for field use.
- **Real-time Intel**: Tactical status bar showing unread messages (✉), node counts (⛫), and battery vitals.

---

## ⚡ Quick Start (Flashing)

If you just want to get up and running quickly with the latest stable build:

1. **Download**: [Heltec_v4_2.4inch_touchUI_v1.13.0_v1.2.3.bin](./bin/Heltec_v4_2.4inch_touchUI_v1.13.0_v1.2.3.bin)
   - *MeshCore v1.13.0 + Touch v1.2.3*
2. **Flash**: Use the [MeshCore Flasher](https://flasher.meshcore.co.uk) (select Custom File) or use `esptool`:
   ```bash
   esptool.py --chip esp32s3 write_flash 0x10000 bin/Heltec_v4_2.4inch_touchUI_v1.13.0_v1.2.3.bin
   ```
3. **Enjoy**: The device will boot directly into the touch interface!

---

## 🛠️ Building from Source

For developers who want to customize or contribute to the project:

1. **Prerequisites**: [Visual Studio Code](https://code.visualstudio.com/) + [PlatformIO IDE](https://platformio.org/).
2. **Clone & Build**:
   ```bash
   git clone https://github.com/Quark1980/Meshcore-Touch.git
   pio run -e heltec_v4_tft_touch_companion_radio_ble
   ```
3. **Upload**:
   ```bash
   pio run -e heltec_v4_tft_touch_companion_radio_ble --target upload
   ```

---

## 🔌 Hardware Connection Guide

### Wiring Diagram (Heltec v4 to ST7789 + XPT2046)

| Component | Function | Heltec v4 Pin |
|-----------|----------|---------------|
| **TFT**   | SCLK     | GPIO 17       |
| **TFT**   | MOSI/SDA | GPIO 33       |
| **TFT**   | CS       | GPIO 15       |
| **TFT**   | DC       | GPIO 16       |
| **TFT**   | RESET    | GPIO 18       |
| **Touch** | T_CLK    | GPIO 17       |
| **Touch** | T_DIN    | GPIO 33       |
| **Touch** | T_DO     | GPIO 42       |
| **Touch** | T_CS     | GPIO 3        |
| **Touch** | T_IRQ    | GPIO 4        |

### Connection Tips:
- **Shared SPI Bus**: `SCLK` (GPIO 17) and `MOSI` (GPIO 33) are shared. Wire both components to these same physical pins.
- **Dedicated CS**: TFT uses `GPIO 15`, Touch uses `GPIO 3`.
- **Power**: Connect `VCC` to `3.3V` or `VEXT`. Ensure the external power rail is enabled if using `VEXT`.

---

## 🚀 Technical Features

- **Custom Icon Pack**: High-resolution 64x64 RGB565 icons.
- **Multi-Module Navigation**:
  - **CLOCK**: Large time/date display.
  - **CHAT**: Encrypted messaging with unread indicators.
  - **NODE**: Dynamic discovery and hop-count tracking.
  - **RADIO/CFG**: Full hardware and device configuration.
  - **LOG/BLE/PWR**: System diagnostics and power management.
- **Propagation Tracking**: Real-time `Me:X` counter for mesh propagation status.
- **App Sync**: BLE Interception keeps device history in sync with MeshCore Mobile.
- **Optimization**: Intelligent backlight and redraw management for power efficiency.

> [!NOTE]
> **Experimental Features**: 'Repeats Heard' and updated BLE logic are currently in testing. Please report any edge cases.

---
*Created by Quark1980 & The MeshCore Community*
