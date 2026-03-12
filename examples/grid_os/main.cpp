#include <Arduino.h>
#include <SPIFFS.h>
#include <TFT_eSPI.h>
#include <Wire.h>
#include <cstring>
#include <lvgl.h>

#include <Mesh.h>
#ifdef BLE_PIN_CODE
#include <helpers/esp32/SerialBLEInterface.h>
#else
#include <helpers/ArduinoSerialInterface.h>
#endif
#include <helpers/SimpleMeshTables.h>

#include "../companion_radio/MyMesh.h"
#include "../companion_radio/DataStore.h"

#include "grid/MeshBridge.h"
#include "grid/WindowManager.h"
#include "grid/GridApps.h"
#include "grid/RadioTelemetryStore.h"
#include "grid/GridRuntimeSettings.h"

// ESP32 companion-radio compatible globals
DataStore store(SPIFFS, rtc_clock);
#ifdef BLE_PIN_CODE
SerialBLEInterface serial_interface;
#else
ArduinoSerialInterface serial_interface;
#endif
StdRNG fast_rng;
SimpleMeshTables tables;
MyMesh the_mesh(radio_driver, fast_rng, rtc_clock, tables, store, nullptr);

namespace {
TFT_eSPI tft = TFT_eSPI();
std::vector<MeshBridge::ContactSummary> gContactSnapshot;

constexpr uint8_t TOUCH_SDA = 5;
constexpr uint8_t TOUCH_SCL = 6;
// Heltec V4 shares GPIO7 with LoRa FEM power control. Do not use a touch IRQ
// pin here or we can disrupt LoRa RX/TX when display power state changes.
constexpr int8_t TOUCH_INT = -1;
constexpr uint8_t TOUCH_RST = 41;
constexpr uint8_t FT6336U_ADDR = 0x38;
constexpr uint8_t kBacklightOnLevel = 200;
constexpr const char* kScreenTimeoutFile = "/grid_screen_timeout.txt";
constexpr const char* kGridVersion = "v0.6.0";
constexpr const char* kGridAuthor = "M.Seijkens";

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
bool gBleEnabled = false;
volatile bool touchIrqPending = false;
bool gDisplaySleeping = false;
uint32_t gLastInteractionMs = 0;
bool gUserBtnPrevPressed = false;
uint32_t gScreenTimeoutSec = 30;
uint32_t gLastPacketCountObserved = 0;
uint32_t gRxWhileScreenOffCount = 0;
uint32_t gRxDebugHideAtMs = 0;
lv_obj_t* gRxDebugLabel = nullptr;

void ensureRxDebugLabel() {
  if (gRxDebugLabel != nullptr) {
    return;
  }

  lv_obj_t* layer = lv_layer_top();
  gRxDebugLabel = lv_label_create(layer);
  lv_obj_set_style_bg_opa(gRxDebugLabel, LV_OPA_80, 0);
  lv_obj_set_style_bg_color(gRxDebugLabel, lv_color_hex(0x111A24), 0);
  lv_obj_set_style_text_color(gRxDebugLabel, lv_color_hex(0xDCE7F5), 0);
  lv_obj_set_style_text_font(gRxDebugLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_pad_left(gRxDebugLabel, 8, 0);
  lv_obj_set_style_pad_right(gRxDebugLabel, 8, 0);
  lv_obj_set_style_pad_top(gRxDebugLabel, 5, 0);
  lv_obj_set_style_pad_bottom(gRxDebugLabel, 5, 0);
  lv_obj_set_style_radius(gRxDebugLabel, 8, 0);
  lv_label_set_text(gRxDebugLabel, "");
  lv_obj_align(gRxDebugLabel, LV_ALIGN_TOP_MID, 0, 34);
  lv_obj_add_flag(gRxDebugLabel, LV_OBJ_FLAG_HIDDEN);
}

void showRxDebugOverlay(uint32_t count) {
  if (count == 0 || gRxDebugLabel == nullptr) {
    return;
  }

  char text[64];
  snprintf(text, sizeof(text), "RX while screen off: %lu", static_cast<unsigned long>(count));
  lv_label_set_text(gRxDebugLabel, text);
  lv_obj_align(gRxDebugLabel, LV_ALIGN_TOP_MID, 0, 34);
  lv_obj_clear_flag(gRxDebugLabel, LV_OBJ_FLAG_HIDDEN);
  gRxDebugHideAtMs = millis() + 4000;
}

uint32_t getScreenTimeoutSecInternal() {
  return gScreenTimeoutSec;
}

void setScreenTimeoutSecInternal(uint32_t sec) {
  gScreenTimeoutSec = sec;
}

void loadScreenTimeoutSecInternal() {
  File f = SPIFFS.open(kScreenTimeoutFile, "r");
  if (!f) {
    return;
  }

  char buf[20] = {0};
  size_t n = f.readBytes(buf, sizeof(buf) - 1);
  f.close();
  if (n == 0) {
    return;
  }

  uint32_t v = static_cast<uint32_t>(strtoul(buf, nullptr, 10));
  gScreenTimeoutSec = v;
}

void saveScreenTimeoutSecInternal() {
  File f = SPIFFS.open(kScreenTimeoutFile, "w");
  if (!f) {
    return;
  }
  f.printf("%lu\n", static_cast<unsigned long>(gScreenTimeoutSec));
  f.close();
}

void reinitTouchController();

void IRAM_ATTR onTouchIrq() {
  touchIrqPending = true;
}

void setPeripheralRailEnabled(bool enabled) {
#ifdef PIN_VEXT_EN
#ifdef PIN_VEXT_EN_ACTIVE
  digitalWrite(PIN_VEXT_EN, enabled ? PIN_VEXT_EN_ACTIVE : !PIN_VEXT_EN_ACTIVE);
#else
  digitalWrite(PIN_VEXT_EN, enabled ? HIGH : LOW);
#endif
#else
  (void)enabled;
#endif
}

void setBacklightEnabled(bool enabled) {
#ifdef PIN_TFT_LEDA_CTL
#ifdef PIN_TFT_LEDA_CTL_ACTIVE
  ledcWrite(0, enabled ? (PIN_TFT_LEDA_CTL_ACTIVE ? kBacklightOnLevel : 0) : (PIN_TFT_LEDA_CTL_ACTIVE ? 0 : kBacklightOnLevel));
#else
  ledcWrite(0, enabled ? kBacklightOnLevel : 0);
#endif
#else
  (void)enabled;
#endif
}

void initTftPanel() {
  tft.init();
  tft.setRotation(0);
#ifdef TFT_INVERSION_ON
  tft.invertDisplay(true);
#endif
  tft.fillScreen(TFT_BLACK);
}

void wakeDisplay() {
  if (!gDisplaySleeping) {
    return;
  }

  gDisplaySleeping = false;
  setBacklightEnabled(true);
  lv_obj_invalidate(lv_scr_act());
  lv_refr_now(nullptr);

  if (gRxWhileScreenOffCount > 0) {
    showRxDebugOverlay(gRxWhileScreenOffCount);
    gRxWhileScreenOffCount = 0;
  }
}

void sleepDisplay() {
  if (gDisplaySleeping) {
    return;
  }

  setBacklightEnabled(false);
  gDisplaySleeping = true;
}

void noteInteraction() {
  gLastInteractionMs = millis();
  if (gDisplaySleeping) {
    wakeDisplay();
  }
}

bool sendQueuedOutboxItem(const MeshBridge::OutboxItem& item) {
  if (item.isPrivate) {
    const int total = the_mesh.getNumContacts();
    for (int i = 0; i < total; ++i) {
      ContactInfo contact;
      if (!the_mesh.getContactByIdx(i, contact)) {
        continue;
      }

      uint32_t contactId = 0;
      memcpy(&contactId, contact.id.pub_key, sizeof(contactId));
      if (contactId != item.threadId) {
        continue;
      }

      uint32_t expectedAck = 0;
      uint32_t estTimeout = 0;
      return the_mesh.sendMessage(contact,
                                  item.timestamp,
                                  0,
                                  item.text,
                                  expectedAck,
                                  estTimeout) != MSG_SEND_FAILED;
    }
    return false;
  }

  ChannelDetails channel;
  if (!the_mesh.getChannel(static_cast<int>(item.threadId & 0xFFu), channel)) {
    return false;
  }

  return the_mesh.sendGroupMessage(item.timestamp,
                                   channel.channel,
                                   the_mesh.getNodePrefs()->node_name,
                                   item.text,
                                   static_cast<int>(strnlen(item.text, sizeof(item.text))));
}

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

  if (TOUCH_INT >= 0) {
    if (!touchIrqPending && digitalRead(TOUCH_INT) != LOW) {
      return false;
    }
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
    if (digitalRead(TOUCH_INT) == HIGH) {
      touchIrqPending = false;
    }
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

  if (digitalRead(TOUCH_INT) == HIGH) {
    touchIrqPending = false;
  }

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

void reinitTouchController() {
  pinMode(TOUCH_RST, OUTPUT);
  digitalWrite(TOUCH_RST, LOW);
  delay(8);
  digitalWrite(TOUCH_RST, HIGH);
  delay(50);
  if (TOUCH_INT >= 0) {
    pinMode(TOUCH_INT, INPUT);
  }
  touchIrqPending = false;
  touchReady = probeTouchOnBus(TOUCH_SDA, TOUCH_SCL) || probeTouchOnBus(4, 3);
  if (TOUCH_INT >= 0) {
    if (touchReady) {
      attachInterrupt(digitalPinToInterrupt(TOUCH_INT), onTouchIrq, FALLING);
    } else {
      detachInterrupt(digitalPinToInterrupt(TOUCH_INT));
    }
  }
}

void lvglTouchRead(lv_indev_drv_t* drv, lv_indev_data_t* data) {
  (void)drv;
  int16_t x = 0;
  int16_t y = 0;
  if (readTouchPoint(&x, &y)) {
    if (gDisplaySleeping) {
      noteInteraction();
      data->state = LV_INDEV_STATE_REL;
      data->continue_reading = false;
      return;
    }

    noteInteraction();
    data->state = LV_INDEV_STATE_PR;
    data->point.x = x;
    data->point.y = y;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }

  data->continue_reading = false;
}

void meshTask(void*) {
  // Legacy placeholder: GRID now runs mesh+UI cooperatively in loop() for radio stability.
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void uiTask(void*) {
  // Legacy placeholder: GRID now runs mesh+UI cooperatively in loop() for radio stability.
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void showSplash() {
  lv_obj_t* scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x080C12), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

  lv_obj_t* title = lv_label_create(scr);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(title, lv_color_hex(0xF4FBFF), 0);
  lv_label_set_text(title, "GRID");
  lv_obj_align(title, LV_ALIGN_CENTER, 0, -44);

  lv_obj_t* version = lv_label_create(scr);
  lv_obj_set_style_text_color(version, lv_color_hex(0x79D6FF), 0);
  lv_obj_set_style_text_font(version, &lv_font_montserrat_20, 0);
  char versionText[40];
  snprintf(versionText, sizeof(versionText), "Version %s", kGridVersion);
  lv_label_set_text(version, versionText);
  lv_obj_align(version, LV_ALIGN_CENTER, 0, 8);

  lv_obj_t* author = lv_label_create(scr);
  lv_obj_set_style_text_color(author, lv_color_hex(0xDCE7F5), 0);
  lv_obj_set_style_text_font(author, &lv_font_montserrat_16, 0);
  char authorText[64];
  snprintf(authorText, sizeof(authorText), "Author: %s", kGridAuthor);
  lv_label_set_text(author, authorText);
  lv_obj_align(author, LV_ALIGN_CENTER, 0, 34);

  lv_obj_t* sub = lv_label_create(scr);
  lv_obj_set_style_text_color(sub, lv_color_hex(0x8EA0B4), 0);
  lv_label_set_text(sub, "powered by MeshCore 1.14");
  lv_obj_align(sub, LV_ALIGN_CENTER, 0, 60);

  uint32_t t0 = millis();
  while (millis() - t0 < 300) {
    lv_tick_inc(10);
    lv_timer_handler();
    delay(10);
  }
}
}

namespace grid::runtime {

uint32_t getScreenTimeoutSec() {
  return getScreenTimeoutSecInternal();
}

void setScreenTimeoutSec(uint32_t sec) {
  setScreenTimeoutSecInternal(sec);
}

void loadScreenTimeoutSec() {
  loadScreenTimeoutSecInternal();
}

void saveScreenTimeoutSec() {
  saveScreenTimeoutSecInternal();
}

}  // namespace grid::runtime

void setup() {
  Serial.begin(115200);

  pinMode(PIN_VEXT_EN, OUTPUT);
  setPeripheralRailEnabled(true);

  board.begin();

  setPeripheralRailEnabled(true);
  delay(80);

  if (!radio_init()) {
    while (true) {
      delay(1000);
    }
  }

  fast_rng.begin(radio_get_rng_seed());
  SPIFFS.begin(true);
  grid::runtime::loadScreenTimeoutSec();
  store.begin();
  the_mesh.begin(false);

#ifdef BLE_PIN_CODE
  serial_interface.begin(BLE_NAME_PREFIX, the_mesh.getNodePrefs()->node_name, the_mesh.getBLEPin());
  the_mesh.bindInterface(serial_interface);
  the_mesh.stopInterface();
  gBleEnabled = false;
#else
  serial_interface.begin(Serial);
  the_mesh.startInterface(serial_interface);
  gBleEnabled = false;
#endif

  // TFT + LVGL init (UI on Core 1)
#ifdef PIN_TFT_LEDA_CTL
  ledcSetup(0, 5000, 8);
  ledcAttachPin(PIN_TFT_LEDA_CTL, 0);
  setBacklightEnabled(true);
#endif

  initTftPanel();

  reinitTouchController();
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
  gLastInteractionMs = millis();

  MeshBridge& bridge = MeshBridge::instance();
  bridge.begin(&the_mesh, nullptr, nullptr, nullptr, nullptr);
  bridge.setRadioMetricsProvider([](int16_t& rssi, int8_t& snr) {
    rssi = static_cast<int16_t>(radio_driver.getLastRSSI());
    snr = static_cast<int8_t>(radio_driver.getLastSNR());
    return true;
  });
  bridge.setBleControl([]() {
#ifdef BLE_PIN_CODE
    return gBleEnabled;
#else
    return false;
#endif
  }, [](bool enabled) {
#ifdef BLE_PIN_CODE
    if (enabled == gBleEnabled) {
      return true;
    }

    if (enabled) {
      the_mesh.startInterface(serial_interface);
      gBleEnabled = true;
      return true;
    }

    the_mesh.stopInterface();
    gBleEnabled = false;
    return true;
  #else
    (void)enabled;
    return false;
  #endif
  });
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
      char pubHex[PUB_KEY_SIZE * 2 + 1] = {0};
      for (size_t j = 0; j < PUB_KEY_SIZE; ++j) {
        snprintf(&pubHex[j * 2], 3, "%02X", contact.id.pub_key[j]);
      }
      item.publicKeyHex = pubHex;
      item.type = contact.type;
      item.flags = contact.flags;
      item.outPathLen = contact.out_path_len;
      item.lastAdvertTimestamp = contact.last_advert_timestamp;
      item.lastSeen = contact.lastmod;
      item.heardRecently = now >= contact.lastmod && (now - contact.lastmod) <= 15 * 60;
      item.gpsLat = contact.gps_lat;
      item.gpsLon = contact.gps_lon;
      item.syncSince = contact.sync_since;
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
#ifdef BLE_PIN_CODE
  registerBleApp(wm);
#endif
  registerSettingsApp(wm);
  registerPowerApp(wm);
  wm.openApp("home", false);

  ensureRxDebugLabel();
  gLastPacketCountObserved = grid::radio_telemetry::packetCount();

  sensors.begin();

}

void loop() {
  static uint32_t lastRadioMetricsMs = 0;
  MeshBridge& bridge = MeshBridge::instance();

  the_mesh.loop();
  sensors.loop();
  rtc_clock.tick();

  MeshBridge::OutboxItem outItem{};
  while (bridge.dequeueOutboxText(outItem, 0)) {
    sendQueuedOutboxItem(outItem);
  }

  if (!gDisplaySleeping) {
    lv_tick_inc(5);
    lv_timer_handler();
    WindowManager::instance().tick();
  }

  const uint32_t now = millis();

  const uint32_t packetCountNow = grid::radio_telemetry::packetCount();
  if (packetCountNow > gLastPacketCountObserved) {
    const uint32_t delta = packetCountNow - gLastPacketCountObserved;
    if (gDisplaySleeping) {
      gRxWhileScreenOffCount += delta;
    }
    gLastPacketCountObserved = packetCountNow;
  }

  if (!gDisplaySleeping && gRxDebugLabel != nullptr && gRxDebugHideAtMs != 0 &&
      static_cast<int32_t>(now - gRxDebugHideAtMs) >= 0) {
    lv_obj_add_flag(gRxDebugLabel, LV_OBJ_FLAG_HIDDEN);
    gRxDebugHideAtMs = 0;
  }

  const uint32_t timeoutSec = grid::runtime::getScreenTimeoutSec();
#ifdef PIN_USER_BTN
  const bool btnPressed = (digitalRead(PIN_USER_BTN) == LOW);
  if (btnPressed && !gUserBtnPrevPressed) {
    if (timeoutSec == 0) {
      if (gDisplaySleeping) {
        noteInteraction();
      } else {
        sleepDisplay();
      }
    } else if (gDisplaySleeping) {
      noteInteraction();
    }
  }
  gUserBtnPrevPressed = btnPressed;
#endif

  if (timeoutSec > 0) {
    const uint32_t timeoutMs = timeoutSec * 1000UL;
    if (!gDisplaySleeping && (now - gLastInteractionMs) >= timeoutMs) {
      sleepDisplay();
    }
  }

  if (now - lastRadioMetricsMs >= 1000) {
    lastRadioMetricsMs = now;
    const int16_t rssi = static_cast<int16_t>(radio_driver.getLastRSSI());
    const int8_t snr = static_cast<int8_t>(radio_driver.getLastSNR());
    grid::radio_telemetry::updateMetrics(rssi, snr, now);
    if (rssi < 0 && rssi > -200) {
      bridge.publishEvent(GRID_EVT_RADIO_STATS, nullptr, "radio", rssi, snr, now);
    }
  }

  delay(5);
}
