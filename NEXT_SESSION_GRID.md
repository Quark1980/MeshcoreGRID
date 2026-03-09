# MeshcoreGRID Next Session Handoff

## Current status
- Display path for Heltec V4 TFT is working with ST7796S in the normal companion firmware.
- Touch hardware has been validated on-device with a touch circle test sketch.
- Main firmware was flashed as BLE client variant: `heltec_v4_tft_companion_radio_ble`.
- Touch-ready plumbing exists in the active display driver (`getTouch` API + FT6336 read path on dedicated I2C bus).

## Verified touch hardware
- Controller: FT6336
- Address: `0x38`
- Pins: SDA=5, SCL=6, INT=7, RST=41

## Naming direction
- Project name: **MeshcoreGRID**
- Touch GUI name: **GRID**

## Next phase objective
Build the first touch GUI flow for **GRID** in companion radio UI:
1. Define touch event model (tap, hold, swipe).
2. Add touch polling and coordinate mapping in UI task loop.
3. Implement first touch navigation screen for GRID.
4. Keep BLE companion flow as the default runtime target.
