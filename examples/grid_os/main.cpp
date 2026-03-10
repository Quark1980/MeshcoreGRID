#include <Arduino.h>
#include <SPIFFS.h>
#include <TFT_eSPI.h>
#include <Wire.h>
#include <cstring>
#include <lvgl.h>

#include <Mesh.h>
#include <helpers/esp32/SerialBLEInterface.h>
#include <helpers/SimpleMeshTables.h>

#include "../companion_radio/MyMesh.h"
#include "../companion_radio/DataStore.h"

#include "grid/MeshBridge.h"
#include "grid/WindowManager.h"
#include "grid/GridApps.h"

// ESP32 companion-radio compatible globals
DataStore store(SPIFFS, rtc_clock);
SerialBLEInterface serial_interface;
StdRNG fast_rng;
SimpleMeshTables tables;
MyMesh the_mesh(radio_driver, fast_rng, rtc_clock, tables, store, nullptr);

namespace {
TFT_eSPI tft = TFT_eSPI();
std::vector<MeshBridge::ContactSummary> gContactSnapshot;

constexpr uint8_t TOUCH_SDA = 5;
constexpr uint8_t TOUCH_SCL = 6;
constexpr uint8_t TOUCH_INT = 7;
constexpr uint8_t TOUCH_RST = 41;
constexpr uint8_t FT6336U_ADDR = 0x38;

// Touch calibration profile for Heltec V4 (rotation 0, 320x480 UI)
constexpr bool TOUCH_SWAP_XY = false;
constexpr bool TOUCH_INVERT_X = false;
constexpr bool TOUCH_INVERT_Y = false;
constexpr int16_t TOUCH_RAW_X_MIN = 0;
constexpr int16_t TOUCH_RAW_X_MAX = 319;
constexpr int16_t TOUCH_RAW_Y_MIN = 0;
constexpr int16_t TOUCH_RAW_Y_MAX = 479;

lv_disp_draw_buf_t drawBuf;
lv_color_t* buf1 = nullptr;
lv_indev_t* touchIndev = nullptr;
TwoWire touchWire(1);
bool touchReady = false;
uint8_t touchAddr = FT6336U_ADDR;
uint8_t touchSda = TOUCH_SDA;
uint8_t touchScl = TOUCH_SCL;

void lvglFlush(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p) {
  const uint32_t w = static_cast<uint32_t>(area->x2 - area->x1 + 1);
  const uint32_t h = static_cast<uint32_t>(area->y2 - area->y1 + 1);
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors(reinterpret_cast<uint16_t*>(color_p), w * h, true);
  tft.endWrite();
  lv_disp_flush_ready(disp);
}

bool readTouchPoint(int16_t* x, int16_t* y) {
  if (!touchReady) {
    return false;
  }

  static uint32_t touchErrLogGate = 0;

  auto logTouchError = [&](const char* reason) {
    const uint32_t now = millis();
    if (now - touchErrLogGate > 2000) {
      touchErrLogGate = now;
      Serial.printf("TOUCH_ERR %s addr=0x%02X sda=%u scl=%u\n", reason, touchAddr, touchSda, touchScl);
    }
  };

  touchWire.beginTransmission(touchAddr);
  touchWire.write(0x02);
  if (touchWire.endTransmission() != 0) {
    logTouchError("reg-write");
    return false;
  }

  touchWire.requestFrom((uint16_t)touchAddr, (uint8_t)5, true);
  if (touchWire.available() < 5) {
    logTouchError("short-read");
    return false;
  }

  uint8_t touches = touchWire.read() & 0x0F;
  uint8_t x_high = touchWire.read();
  uint8_t x_low = touchWire.read();
  uint8_t y_high = touchWire.read();
  uint8_t y_low = touchWire.read();

  if (touches == 0 || x == nullptr || y == nullptr) {
    return false;
  }

  int16_t rawX = ((x_high & 0x0F) << 8) | x_low;
  int16_t rawY = ((y_high & 0x0F) << 8) | y_low;

  int16_t tx = TOUCH_SWAP_XY ? rawY : rawX;
  int16_t ty = TOUCH_SWAP_XY ? rawX : rawY;

  tx = map(tx, TOUCH_RAW_X_MIN, TOUCH_RAW_X_MAX, 0, 319);
  ty = map(ty, TOUCH_RAW_Y_MIN, TOUCH_RAW_Y_MAX, 0, 479);

  if (TOUCH_INVERT_X) {
    tx = 319 - tx;
  }
  if (TOUCH_INVERT_Y) {
    ty = 479 - ty;
  }

  tx = constrain(tx, 0, 319);
  ty = constrain(ty, 0, 479);

  *x = tx;
  *y = ty;
  return true;
}

bool probeTouchOnBus(uint8_t sda, uint8_t scl) {
  touchWire.begin(sda, scl, 400000);

  const uint8_t candidateAddrs[] = {0x38, 0x15, 0x14};
  for (uint8_t i = 0; i < sizeof(candidateAddrs); ++i) {
    touchWire.beginTransmission(candidateAddrs[i]);
    if (touchWire.endTransmission() == 0) {
      touchAddr = candidateAddrs[i];
      touchSda = sda;
      touchScl = scl;
      return true;
    }
  }
  return false;
}

void lvglTouchRead(lv_indev_drv_t* drv, lv_indev_data_t* data) {
  (void)drv;
  int16_t x = 0;
  int16_t y = 0;
  if (readTouchPoint(&x, &y)) {
    data->state = LV_INDEV_STATE_PR;
    data->point.x = x;
    data->point.y = y;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }

  data->continue_reading = false;
}

void meshTask(void*) {
  for (;;) {
    the_mesh.loop();
    sensors.loop();
    rtc_clock.tick();
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

void uiTask(void*) {
  for (;;) {
    lv_tick_inc(8);
    lv_timer_handler();
    WindowManager::instance().tick();
    vTaskDelay(pdMS_TO_TICKS(8));
  }
}

void showSplash() {
  lv_obj_t* scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x0E1117), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

  lv_obj_t* title = lv_label_create(scr);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
  lv_label_set_text(title, "GRID");
  lv_obj_align(title, LV_ALIGN_CENTER, 0, -12);

  lv_obj_t* sub = lv_label_create(scr);
  lv_obj_set_style_text_color(sub, lv_color_hex(0xB8C2CF), 0);
  lv_label_set_text(sub, "GRID, powered by meshcore 1.14");
  lv_obj_align(sub, LV_ALIGN_CENTER, 0, 18);

  uint32_t t0 = millis();
  while (millis() - t0 < 300) {
    lv_tick_inc(10);
    lv_timer_handler();
    delay(10);
  }
}
}

void setup() {
  Serial.begin(115200);

  board.begin();

  pinMode(PIN_VEXT_EN, OUTPUT);
#ifdef PIN_VEXT_EN_ACTIVE
  digitalWrite(PIN_VEXT_EN, PIN_VEXT_EN_ACTIVE);
#else
  digitalWrite(PIN_VEXT_EN, HIGH);
#endif
  delay(80);

  if (!radio_init()) {
    while (true) {
      delay(1000);
    }
  }

  fast_rng.begin(radio_get_rng_seed());
  SPIFFS.begin(true);
  store.begin();
  the_mesh.begin(false);
  serial_interface.begin(BLE_NAME_PREFIX, the_mesh.getNodePrefs()->node_name, the_mesh.getBLEPin());
  the_mesh.startInterface(serial_interface);

  // TFT + LVGL init (UI on Core 1)
#ifdef PIN_TFT_LEDA_CTL
  ledcSetup(0, 5000, 8);
  ledcAttachPin(PIN_TFT_LEDA_CTL, 0);
#ifdef PIN_TFT_LEDA_CTL_ACTIVE
  ledcWrite(0, PIN_TFT_LEDA_CTL_ACTIVE ? 200 : 0);
#else
  ledcWrite(0, 200);
#endif
#endif

  tft.init();
  tft.setRotation(0);
#ifdef TFT_INVERSION_ON
  tft.invertDisplay(true);
#endif
  tft.fillScreen(TFT_BLACK);

  pinMode(TOUCH_RST, OUTPUT);
  digitalWrite(TOUCH_RST, LOW);
  delay(8);
  digitalWrite(TOUCH_RST, HIGH);
  delay(50);
  pinMode(TOUCH_INT, INPUT);
  touchReady = probeTouchOnBus(TOUCH_SDA, TOUCH_SCL) || probeTouchOnBus(4, 3);
  if (touchReady) {
    Serial.printf("Touch detected at 0x%02X on SDA=%u SCL=%u\n", touchAddr, touchSda, touchScl);
  } else {
    Serial.println("WARN: No supported touch controller detected");
  }

  lv_init();

  const uint32_t horRes = 320;
  const uint32_t lineCount = 40;
  buf1 = static_cast<lv_color_t*>(heap_caps_malloc(horRes * lineCount * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  lv_disp_draw_buf_init(&drawBuf, buf1, nullptr, horRes * lineCount);

  static lv_disp_drv_t dispDrv;
  lv_disp_drv_init(&dispDrv);
  dispDrv.hor_res = 320;
  dispDrv.ver_res = 480;
  dispDrv.flush_cb = lvglFlush;
  dispDrv.draw_buf = &drawBuf;
  lv_disp_drv_register(&dispDrv);

  static lv_indev_drv_t indevDrv;
  lv_indev_drv_init(&indevDrv);
  indevDrv.type = LV_INDEV_TYPE_POINTER;
  indevDrv.read_cb = lvglTouchRead;
  touchIndev = lv_indev_drv_register(&indevDrv);

  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);
  lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);
  lv_refr_now(nullptr);

  showSplash();

  MeshBridge& bridge = MeshBridge::instance();
  bridge.begin(&the_mesh, nullptr, nullptr, nullptr, nullptr);
  bridge.setChannelProvider([](std::vector<MeshBridge::ChannelSummary>& out) {
    out.clear();
    out.reserve(24);
#ifdef MAX_GROUP_CHANNELS
    const int maxChannels = MAX_GROUP_CHANNELS;
#else
    const int maxChannels = 40;
#endif
    constexpr int kProviderChannelCap = 24;
    int emitted = 0;
    for (int i = 0; i < maxChannels; ++i) {
      ChannelDetails channel;
      if (!the_mesh.getChannel(i, channel)) {
        continue;
      }

      MeshBridge::ChannelSummary item{};
      item.id = static_cast<uint8_t>(i);
      if (channel.name[0]) {
        const size_t nameLen = strnlen(channel.name, sizeof(channel.name));
        item.name.assign(channel.name, nameLen);
      } else {
        item.name = "Channel";
      }
      out.push_back(item);

      emitted++;
      if (emitted >= kProviderChannelCap) {
        break;
      }
    }
  });
  bridge.setContactProvider([](std::vector<MeshBridge::ContactSummary>& out) {
    out = gContactSnapshot;
  });

  gContactSnapshot.clear();
  gContactSnapshot.reserve(64);
  {
    const uint32_t now = the_mesh.getRTCClock()->getCurrentTime();
    const int total = the_mesh.getNumContacts();
    constexpr int kProviderContactCap = 64;
    int emitted = 0;

    for (int i = 0; i < total; ++i) {
      ContactInfo contact;
      if (!the_mesh.getContactByIdx(i, contact)) {
        continue;
      }

      uint32_t contactId = 0;
      memcpy(&contactId, contact.id.pub_key, sizeof(contactId));

      char nameBuf[16] = {0};
      if (!contact.name[0]) {
        snprintf(nameBuf, sizeof(nameBuf), "%02X%02X%02X%02X", contact.id.pub_key[0], contact.id.pub_key[1], contact.id.pub_key[2], contact.id.pub_key[3]);
      }

      MeshBridge::ContactSummary item{};
      item.id = contactId;
      if (contact.name[0]) {
        const size_t nameLen = strnlen(contact.name, sizeof(contact.name));
        item.name.assign(contact.name, nameLen);
      } else {
        item.name = nameBuf;
      }
      item.lastSeen = contact.lastmod;
      item.heardRecently = now >= contact.lastmod && (now - contact.lastmod) <= 15 * 60;
      gContactSnapshot.push_back(item);

      emitted++;
      if (emitted >= kProviderContactCap) {
        break;
      }
    }
  }

  WindowManager& wm = WindowManager::instance();
  wm.begin(bridge);
  registerHomeApp(wm);
  registerChatApp(wm, bridge);
  registerNodesApp(wm, bridge);
  registerRadioApp(wm);
  registerBleApp(wm);
  registerSettingsApp(wm);
  registerPowerApp(wm);
  wm.openApp("home", false);

  xTaskCreatePinnedToCore(meshTask, "grid_mesh_core0", 6144, nullptr, 4, nullptr, 0);
  xTaskCreatePinnedToCore(uiTask, "grid_ui_core1", 6144, nullptr, 3, nullptr, 1);
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(100));
}
