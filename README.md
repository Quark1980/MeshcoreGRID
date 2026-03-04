# Meshcore-Touch

![MeshCore Touch](logo/touch.png)

MeshCore is a lightweight, portable C++ library that enables multi-hop packet routing for embedded projects. This specialized **"TOUCH" Edition** is optimized for the Heltec v4 (ESP32-S3), featuring a premium dark-mode UI and full touch interactivity.

> [!WARNING]
> **BETA VERSION**: This project is in active development. Features and UI are subject to change.

## 💎 Featured Build: Heltec v4 "TOUCH"

Optimized for the ESP32-S3 with a 320x240 ST7789 TFT and XPT2046 touch controller.

### ⚡ HOWTO: Quick Start (Flashing)
If you just want to get up and running quickly:
1. **Download**: [heltec_v4_meshcore_touch.bin](./bin/heltec_v4_meshcore_touch.bin)
2. **Flash**: Use the [MeshCore Flasher](https://flasher.meshcore.co.uk) (select Custom File) or use `esptool`:
   ```bash
   esptool.py --chip esp32s3 write_flash 0x10000 bin/heltec_v4_meshcore_touch.bin
   ```
3. **Enjoy**: The device will boot into the premium touch interface!

### 🛠️ HOWTO: Building from Source
For developers who want to customize the project:
1. **Prerequisites**: Install [Visual Studio Code](https://code.visualstudio.com/) and the [PlatformIO IDE](https://platformio.org/) extension.
2. **Clone**: 
   ```bash
   git clone https://github.com/Quark1980/Meshcore-Touch.git
   ```
3. **Build**:
   ```bash
   pio run -e heltec_v4_tft_touch_companion_radio_ble
   ```
4. **Upload**:
   ```bash
   pio run -e heltec_v4_tft_touch_companion_radio_ble --target upload
   ```

#### 🔌 Wiring Diagram (Heltec v4 to ST7789 + XPT2046)

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

### 🔧 Physical Connection Guide
To get your Heltec V4 working with a 2.4" or 2.8" SPI TFT (ST7789) and Touch (XPT2046):

1. **Shared SPI Bus**: Note that `SCLK` (GPIO 17) and `MOSI` (GPIO 33) are shared between both the display and the touch controller. You must wire both components to these same physical pins on the Heltec.
2. **Dedicated Chip Selects**: Each component has its own `CS` pin (`GPIO 15` for TFT, `GPIO 3` for Touch) to allow the ESP32-S3 to talk to them individually.
3. **Power (3.3V)**: Connect the `VCC` of both the TFT and Touch to the Heltec's `3.3V` or `VEXT` pin. If using `VEXT`, ensure your code or configuration enables the external power rail.
4. **Backlight**: The code expects the backlight to be hardwired or managed. For Heltec V4, ensure your ground connections are solid to avoid flicker.

## 📱 Premium Tactical Touch UI
The "TOUCH" edition features a high-performance, low-flicker UI designed for mission-critical reliability:
- **3-Icon Carousel Navigation**: Fluent, touch-friendly carousel with large PREV/NEXT buttons for easy selection.
- **Premium Aesthetics**: Rugged square app icons with 10px rounded borders and high-contrast "Neon Cyan" accents.
- **Tactical Status Bar**: 
  - **✉ Unread Messages**: Real-time counter for incoming channel and direct messages.
  - **⛫ Node Discovery**: Dynamic counter showing active nodes heard in the mesh.
  - **⚡ Power Intel**: Accurate battery percentage and voltage (V:x.xx) display.
- **Instant Responsiveness**: Fixed touch-hang issues and optimized redraw logic for an "always-ready" interface.
- **Full QWERTY Keyboard**: Compose and send encrypted messages directly on-screen.
- **Auto-Dismiss Splash**: Startup splash screen automatically transitions to the launcher after 5 seconds.
- **Power Efficiency**: Intelligent backlight management and optional manual-wake notifications to maximize battery life.
- **Node Persistence**: Secure, on-board storage for node names and mesh preferences.

---
*Created by Quark1980 & The MeshCore Community*

