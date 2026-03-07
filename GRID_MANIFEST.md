# GRID OS Manifest

Essential settings and information for the MeshCore-Touch project.

## Hardware: Heltec V4 (ESP32-S3)
- **Display**: ST7796S (3.5" or similar, SPI)
    - **Pins**: SCK: 9, MOSI: 10, MISO: 11, CS: 3, DC: 4, RST: 5, BL: 6
    - **Resolution**: 320x480
    - **Driver**: TFT_eSPI (configured for `Rotation 0`)
- **Touch**: FT6336U (I2C)
    - **Pins**: SDA: 41, SCL: 42, INT: 7, RST: 1
    - **Mapping**: Direct raw mapping to 320x480 (No inversion needed in `Rotation 0`).

## Software Stack
- **Framework**: Arduino / PlatformIO
- **UI Library**: LVGL v9.5.0
- **Base**: MeshCore v1.14.0

## UI Architecture
- **Theme**: Amber/Dark (#FFB300 on #121212).
- **Navigation**:
    - **Global Back Button**: Persistent `lv_button` on `lv_layer_top()` for universal navigation.
    - **Status Bar**: Persistent `lv_obj` on `lv_layer_top()` for system metrics.
- **Applications**:
    - `src/grid/grid_chat_overview`: **Chat Home** with TabView (HASHTAGS | DMs) and real-time numeric badges.
    - `src/grid/grid_chat`: **Active Conversation View** with bubble messages, sender metadata, and hashtag linking.
    - `src/grid/grid_ui_common`: Centralized styles, **Float-Protected Keyboard**, and Unread Badge helper (`grid_create_badge`).
    - `src/grid/grid_launcher`: Multi-app grid with total unread badges on app icons.
---
## Progress Log (Current)
- [x] Boot stable with TFT_eSPI & FT6336U.
- [x] Fixed circular sync crash in keyboard preview.
- [x] Implemented global nav logic on top layer.
- [x] Implemented professional bubble chat with unread badges.
- [x] Implemented Hashtag Channels and Direct Messages (DM) flow.
- [x] Refactored Chat into Overview/Conversation architecture.

## Git Backup
- Remote: `https://github.com/Quark1980/MeshcoreGRID.git`
