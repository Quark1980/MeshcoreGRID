#pragma once

#include "DisplayDriver.h"
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#if defined(HELTEC_LORA_V4_TFT)
#include <Adafruit_ST7796S.h>
#else
#include <Adafruit_ST7789.h>
#endif
#include <helpers/RefCountedDigitalPin.h>

class ST7789LCDDisplay : public DisplayDriver {
  #if defined(LILYGO_TDECK) || defined(HELTEC_LORA_V4_TFT)
    SPIClass displaySPI;
  #endif
#if defined(HELTEC_LORA_V4_TFT)
  Adafruit_ST7796S display;
#else
  Adafruit_ST7789 display;
#endif
  bool _isOn;
  bool _touchReady;
  uint16_t _color;
  RefCountedDigitalPin* _peripher_power;

#if defined(HELTEC_LORA_V4_TFT) && defined(ESP32)
  TwoWire _touchWire;
#endif

  bool i2c_probe(TwoWire& wire, uint8_t addr);
public:
#ifdef USE_PIN_TFT
  ST7789LCDDisplay(RefCountedDigitalPin* peripher_power=NULL) : DisplayDriver(128, 64), 
      display(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_SDA, PIN_TFT_SCL, PIN_TFT_RST),
      _peripher_power(peripher_power)
  {
    _isOn = false;
    _touchReady = false;
  }
#elif defined(LILYGO_TDECK) || defined(HELTEC_LORA_V4_TFT)
  ST7789LCDDisplay(RefCountedDigitalPin* peripher_power=NULL) : DisplayDriver(128, 64),
      displaySPI(HSPI),
      display(&displaySPI, PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST),
#if defined(HELTEC_LORA_V4_TFT) && defined(ESP32)
      _touchWire(1),
#endif
      _peripher_power(peripher_power)
  {
    _isOn = false;
    _touchReady = false;
  }
#else
  ST7789LCDDisplay(RefCountedDigitalPin* peripher_power=NULL) : DisplayDriver(128, 64), 
      display(&SPI, PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST),
      _peripher_power(peripher_power)
  {
    _isOn = false;
    _touchReady = false;
  }
#endif
  bool begin();

  bool isOn() override { return _isOn; }
  void turnOn() override;
  void turnOff() override;
  void clear() override;
  void startFrame(Color bkg = DARK) override;
  void setTextSize(int sz) override;
  void setColor(Color c) override;
  void setCursor(int x, int y) override;
  void print(const char* str) override;
  void fillRect(int x, int y, int w, int h) override;
  void drawRect(int x, int y, int w, int h) override;
  void drawXbm(int x, int y, const uint8_t* bits, int w, int h) override;
  uint16_t getTextWidth(const char* str) override;
  bool getTouch(int* x, int* y) override;
  void endFrame() override;
};
