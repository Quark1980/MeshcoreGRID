#include "GridApps.h"

#include "MyMesh.h"
#include "Packet.h"
#include "RadioTelemetryStore.h"
#include "GridRuntimeSettings.h"
#include "WindowManager.h"
#include "target.h"

#include <string>
#include <vector>

void registerMessengerStubApp(WindowManager& wm, MeshBridge& bridge);

namespace {

class NodesApp : public MeshApp {
public:
  explicit NodesApp(MeshBridge* bridge)
      : _bridge(bridge), _list(nullptr), _countLabel(nullptr), _showingEmptyState(false) {}
  void release() override { this->~NodesApp(); heap_caps_free(this); }
  void onStart(lv_obj_t* layout) override {
    lv_obj_t* title = lv_label_create(layout);
    lv_label_set_text(title, "Nodes Heard");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 14, 12);

    _countLabel = lv_label_create(layout);
    lv_obj_set_style_text_font(_countLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(_countLabel, lv_color_hex(0x9FAABB), 0);
    lv_obj_align(_countLabel, LV_ALIGN_TOP_RIGHT, -14, 16);

    _list = lv_list_create(layout);
    lv_obj_set_size(_list, LV_PCT(100), LV_PCT(86));
    lv_obj_align(_list, LV_ALIGN_BOTTOM_MID, 0, 0);

    populateBootAdverts();

    if (_bridge) {
      _bridge->subscribe(GRID_EVT_NODE_ADVERT, [this](const MeshMessage& msg) { onMessageReceived(msg); });
    }
  }
  void onLoop() override {}
  void onClose() override {
    if (_bridge) _bridge->clearSubscribers(GRID_EVT_NODE_ADVERT);
    _list = nullptr;
    _countLabel = nullptr;
    _showingEmptyState = false;
  }
  void onMessageReceived(MeshMessage msg) override {
    if (_list == nullptr || msg.packetType != GRID_EVT_NODE_ADVERT) return;

    if (_showingEmptyState) {
      lv_obj_clean(_list);
      _showingEmptyState = false;
    }

    const char* label = msg.text.empty() ? "(unnamed advert)" : msg.text.c_str();
    lv_list_add_text(_list, label);
    updateCountLabel();
  }
private:
  void populateBootAdverts() {
    if (_list == nullptr || _bridge == nullptr) {
      return;
    }

    const auto adverts = _bridge->getBootNodeAdverts();
    if (adverts.empty()) {
      lv_list_add_text(_list, "No adverts heard this boot");
      _showingEmptyState = true;
      updateCountLabel();
      return;
    }

    for (const auto& advert : adverts) {
      const char* label = advert.text.empty() ? "(unnamed advert)" : advert.text.c_str();
      lv_list_add_text(_list, label);
    }
    _showingEmptyState = false;
    updateCountLabel();
  }

  void updateCountLabel() {
    if (_countLabel == nullptr || _bridge == nullptr) {
      return;
    }

    char buf[32];
    snprintf(buf,
             sizeof(buf),
             "%u adverts",
             static_cast<unsigned>(_bridge->getBootNodeAdverts().size()));
    lv_label_set_text(_countLabel, buf);
  }

  MeshBridge* _bridge;
  lv_obj_t* _list;
  lv_obj_t* _countLabel;
  bool _showingEmptyState;
};

class SimpleCardApp : public MeshApp {
public:
  SimpleCardApp(const char* title, const char* subtitle) : _title(title), _subtitle(subtitle) {}
  void release() override { this->~SimpleCardApp(); heap_caps_free(this); }
  void onStart(lv_obj_t* layout) override {
    lv_obj_t* t = lv_label_create(layout);
    lv_label_set_text(t, _title);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_20, 0);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 14, 12);

    lv_obj_t* card = lv_obj_create(layout);
    lv_obj_set_size(card, LV_PCT(92), 112);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 56);
    lv_obj_set_style_radius(card, 14, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1A1F27), 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x2F3947), 0);

    lv_obj_t* s = lv_label_create(card);
    lv_label_set_text(s, _subtitle);
    lv_obj_set_style_text_font(s, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s, lv_color_hex(0xD6E0EC), 0);
    lv_obj_center(s);
  }
  void onLoop() override {}
  void onClose() override {}
  void onMessageReceived(MeshMessage) override {}
private:
  const char* _title;
  const char* _subtitle;
};

class RadioApp : public MeshApp {
public:
  RadioApp()
      : _tabview(nullptr),
        _metricsText(nullptr),
        _advertStatus(nullptr),
        _lastRefreshMs(0) {}
  void release() override { this->~RadioApp(); heap_caps_free(this); }
  void onStart(lv_obj_t* layout) override {
    lv_obj_t* t = lv_label_create(layout);
    lv_label_set_text(t, "Radio");
    lv_obj_set_style_text_font(t, &lv_font_montserrat_20, 0);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 14, 12);

    _tabview = lv_tabview_create(layout, LV_DIR_TOP, 36);
    lv_obj_set_size(_tabview, LV_PCT(96), LV_PCT(84));
    lv_obj_align(_tabview, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_obj_t* advertTab = lv_tabview_add_tab(_tabview, "Advert");
    lv_obj_t* metricsTab = lv_tabview_add_tab(_tabview, "Metrics");

    lv_obj_clear_flag(advertTab, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(advertTab, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(metricsTab, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(metricsTab, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(metricsTab, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_bg_opa(metricsTab, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(metricsTab, lv_color_hex(0x121821), 0);
    lv_obj_set_style_border_width(metricsTab, 0, 0);

    lv_obj_t* advertHint = lv_label_create(advertTab);
    lv_label_set_text(advertHint, "Send this node advertisement as zero-hop or flood.");
    lv_obj_set_style_text_color(advertHint, lv_color_hex(0x9FAABB), 0);
    lv_obj_set_style_text_font(advertHint, &lv_font_montserrat_14, 0);
    lv_obj_align(advertHint, LV_ALIGN_TOP_LEFT, 8, 18);

    lv_obj_t* zeroHopBtn = lv_btn_create(advertTab);
    lv_obj_set_size(zeroHopBtn, LV_PCT(84), 42);
    lv_obj_align(zeroHopBtn, LV_ALIGN_TOP_MID, 0, 62);
    lv_obj_set_style_bg_color(zeroHopBtn, lv_color_hex(0x1B2530), 0);
    lv_obj_set_style_bg_color(zeroHopBtn, lv_color_hex(0xFFB300), LV_STATE_PRESSED);
    lv_obj_add_event_cb(zeroHopBtn, [](lv_event_t* e) {
      auto* self = static_cast<RadioApp*>(lv_event_get_user_data(e));
      if (self == nullptr) {
        return;
      }
      self->setAdvertStatus(the_mesh.advert(), "Zero-hop advert sent", "Advert send failed");
    }, LV_EVENT_CLICKED, this);
    lv_obj_t* zeroHopLabel = lv_label_create(zeroHopBtn);
    lv_label_set_text(zeroHopLabel, LV_SYMBOL_WIFI " Advert Zero-Hop");
    lv_obj_center(zeroHopLabel);

    lv_obj_t* floodBtn = lv_btn_create(advertTab);
    lv_obj_set_size(floodBtn, LV_PCT(84), 42);
    lv_obj_align(floodBtn, LV_ALIGN_TOP_MID, 0, 118);
    lv_obj_set_style_bg_color(floodBtn, lv_color_hex(0x1B2530), 0);
    lv_obj_set_style_bg_color(floodBtn, lv_color_hex(0xFFB300), LV_STATE_PRESSED);
    lv_obj_add_event_cb(floodBtn, [](lv_event_t* e) {
      auto* self = static_cast<RadioApp*>(lv_event_get_user_data(e));
      if (self == nullptr) {
        return;
      }
      self->setAdvertStatus(the_mesh.advertFlood(), "Flood advert sent", "Advert send failed");
    }, LV_EVENT_CLICKED, this);
    lv_obj_t* floodLabel = lv_label_create(floodBtn);
    lv_label_set_text(floodLabel, LV_SYMBOL_WIFI " Advert Flood");
    lv_obj_center(floodLabel);

    _advertStatus = lv_label_create(advertTab);
    lv_obj_set_style_text_font(_advertStatus, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(_advertStatus, lv_color_hex(0x9FAABB), 0);
    lv_obj_align(_advertStatus, LV_ALIGN_TOP_LEFT, 8, 176);
    lv_label_set_text(_advertStatus, "");

    lv_obj_set_style_pad_top(metricsTab, 10, 0);
    lv_obj_set_style_pad_bottom(metricsTab, 14, 0);
    lv_obj_set_style_pad_left(metricsTab, 10, 0);
    lv_obj_set_style_pad_right(metricsTab, 10, 0);

    _metricsText = lv_label_create(metricsTab);
    lv_obj_set_width(_metricsText, LV_PCT(98));
    lv_obj_set_style_text_color(_metricsText, lv_color_hex(0xC7D1DE), 0);
    lv_obj_set_style_text_font(_metricsText, &lv_font_montserrat_14, 0);
    lv_obj_align(_metricsText, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_label_set_long_mode(_metricsText, LV_LABEL_LONG_WRAP);
    lv_label_set_text(_metricsText, "Loading...");

    refreshMetrics();
    lv_tabview_set_act(_tabview, 0, LV_ANIM_OFF);
  }
  void onLoop() override {
    const uint32_t now = millis();
    if (now - _lastRefreshMs < 250) {
      return;
    }
    _lastRefreshMs = now;

    refreshMetrics();
  }
  void onClose() override {
    _tabview = nullptr;
    _metricsText = nullptr;
    _advertStatus = nullptr;
  }
  void onMessageReceived(MeshMessage) override {}

private:
  void refreshMetrics() {
    if (_metricsText == nullptr) {
      return;
    }

    NodePrefs* prefs = the_mesh.getNodePrefs();
    const float freq = prefs ? prefs->freq : static_cast<float>(LORA_FREQ);
    const float bw = prefs ? prefs->bw : static_cast<float>(LORA_BW);
    const int sf = prefs ? static_cast<int>(prefs->sf) : static_cast<int>(LORA_SF);
    const int cr = prefs ? static_cast<int>(prefs->cr) : static_cast<int>(LORA_CR);
    const int tx = prefs ? static_cast<int>(prefs->tx_power_dbm) : static_cast<int>(LORA_TX_POWER);
    const float af = prefs ? prefs->airtime_factor : 1.0f;
    const float duty = 100.0f / (af + 1.0f);

    int16_t rssi = 0;
    int8_t snr = 0;
    uint32_t ts = 0;
    if (!grid::radio_telemetry::getLatestMetrics(rssi, snr, ts)) {
      char waitingBuf[320];
      snprintf(waitingBuf,
               sizeof(waitingBuf),
               "Freq: %.3f MHz\nBW: %.1f kHz\nSF: %d\nCR: %d\nTX: %d dBm\nAirtime factor: %.1f\nDuty limit: %.1f%%\n\nSignal: waiting\nPacket count: %lu\nLast raw packet: %lu\nRX calls: %lu\nDispatcher raw hits: %lu\nRecv flood: %lu\nRecv direct: %lu",
               (double)freq,
               (double)bw,
               sf,
               cr,
               tx,
               (double)af,
               (double)duty,
               (unsigned long)grid::radio_telemetry::packetCount(),
               (unsigned long)grid::radio_telemetry::lastRawPacketTimestamp(),
               (unsigned long)grid::radio_telemetry::getRxCallHits(),
               (unsigned long)grid::radio_telemetry::getDispatcherRxRawHits(),
               (unsigned long)the_mesh.getNumRecvFlood(),
               (unsigned long)the_mesh.getNumRecvDirect());
      lv_label_set_text(_metricsText, waitingBuf);
      return;
    }

    char metricsBuf[384];
    snprintf(metricsBuf,
             sizeof(metricsBuf),
             "Freq: %.3f MHz\nBW: %.1f kHz\nSF: %d\nCR: %d\nTX: %d dBm\nAirtime factor: %.1f\nDuty limit: %.1f%%\n\nRSSI: %d dBm\nSNR: %d dB\nPacket count: %lu\nLast raw packet: %lu\nRX calls: %lu\nDispatcher raw hits: %lu\nRecv flood: %lu\nRecv direct: %lu\nUpdated: %lu",
             (double)freq,
             (double)bw,
             sf,
             cr,
             tx,
             (double)af,
             (double)duty,
             (int)rssi,
             (int)snr,
             (unsigned long)grid::radio_telemetry::packetCount(),
             (unsigned long)grid::radio_telemetry::lastRawPacketTimestamp(),
             (unsigned long)grid::radio_telemetry::getRxCallHits(),
             (unsigned long)grid::radio_telemetry::getDispatcherRxRawHits(),
             (unsigned long)the_mesh.getNumRecvFlood(),
             (unsigned long)the_mesh.getNumRecvDirect(),
             (unsigned long)ts);
    lv_label_set_text(_metricsText, metricsBuf);
  }

  void setAdvertStatus(bool ok, const char* successText, const char* errorText) {
    if (_advertStatus == nullptr) {
      return;
    }

    lv_obj_set_style_text_color(_advertStatus,
                                lv_color_hex(ok ? 0x59D37A : 0xFF7A59),
                                0);
    lv_label_set_text(_advertStatus, ok ? successText : errorText);
  }

  lv_obj_t* _tabview;
  lv_obj_t* _metricsText;
  lv_obj_t* _advertStatus;
  uint32_t _lastRefreshMs;
};

class BleApp : public MeshApp {
public:
  void release() override { this->~BleApp(); heap_caps_free(this); }
  void onStart(lv_obj_t* layout) override {
    lv_obj_t* t = lv_label_create(layout);
    lv_label_set_text(t, "BLE");
    lv_obj_set_style_text_font(t, &lv_font_montserrat_20, 0);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 14, 12);

    lv_obj_t* card = lv_obj_create(layout);
    lv_obj_set_size(card, LV_PCT(92), 212);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 56);
    lv_obj_set_style_radius(card, 14, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1A1F27), 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x2F3947), 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    _state = lv_label_create(card);
    lv_obj_set_width(_state, LV_PCT(100));
    lv_label_set_long_mode(_state, LV_LABEL_LONG_WRAP);
    lv_obj_align(_state, LV_ALIGN_TOP_LEFT, 12, 14);

    _pin = lv_label_create(card);
    lv_obj_set_style_text_font(_pin, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(_pin, lv_color_hex(0xE2E8F0), 0);
    lv_obj_set_width(_pin, LV_PCT(100));
    lv_label_set_long_mode(_pin, LV_LABEL_LONG_WRAP);
    lv_obj_align(_pin, LV_ALIGN_TOP_LEFT, 12, 62);

    _connection = lv_label_create(card);
    lv_obj_set_style_text_color(_connection, lv_color_hex(0x9FAABB), 0);
    lv_obj_set_width(_connection, LV_PCT(100));
    lv_label_set_long_mode(_connection, LV_LABEL_LONG_WRAP);
    lv_obj_align(_connection, LV_ALIGN_TOP_LEFT, 12, 108);

    _toggleBtn = lv_btn_create(card);
    lv_obj_set_size(_toggleBtn, 156, 40);
    lv_obj_align(_toggleBtn, LV_ALIGN_BOTTOM_LEFT, 12, -16);
    lv_obj_set_style_bg_color(_toggleBtn, lv_color_hex(0xFFB300), LV_STATE_PRESSED);
    _toggleText = lv_label_create(_toggleBtn);
    lv_obj_center(_toggleText);
    lv_obj_add_event_cb(_toggleBtn, [](lv_event_t* e) {
      auto* self = static_cast<BleApp*>(lv_event_get_user_data(e));
      if (self) {
        const bool currentlyEnabled = MeshBridge::instance().isBleEnabled();
        MeshBridge::instance().setBleEnabled(!currentlyEnabled);
        self->refreshUi();
      }
    }, LV_EVENT_CLICKED, this);

    refreshUi();
  }
  void onLoop() override { refreshUi(); }
  void onClose() override {
    _state = nullptr;
    _pin = nullptr;
    _connection = nullptr;
    _toggleBtn = nullptr;
    _toggleText = nullptr;
  }
  void onMessageReceived(MeshMessage) override {}

private:
  void refreshUi() {
    if (_state == nullptr || _toggleText == nullptr || _pin == nullptr || _connection == nullptr) {
      return;
    }

    const bool enabled = MeshBridge::instance().isBleEnabled();
    const bool connected = MeshBridge::instance().isBleConnected();

    lv_label_set_text(_state, enabled ? "Companion BLE: enabled" : "Companion BLE: disabled");
    char pinText[64];
    NodePrefs* prefs = the_mesh.getNodePrefs();
    uint32_t pin = prefs ? prefs->ble_pin : 0;
#ifdef BLE_PIN_CODE
    if (pin == 0) {
      pin = static_cast<uint32_t>(BLE_PIN_CODE);
    }
#endif
    if (pin > 0) {
      snprintf(pinText, sizeof(pinText), "PIN: %lu", (unsigned long)pin);
    } else {
      snprintf(pinText, sizeof(pinText), "PIN: n/a");
    }
    lv_label_set_text(_pin, pinText);
    lv_label_set_text(_connection, connected ? "Status: connected" : "Status: disconnected");
    lv_label_set_text(_toggleText, enabled ? "Turn BLE Off" : "Turn BLE On");
  }

  lv_obj_t* _state = nullptr;
  lv_obj_t* _pin = nullptr;
  lv_obj_t* _connection = nullptr;
  lv_obj_t* _toggleBtn = nullptr;
  lv_obj_t* _toggleText = nullptr;
};

class SettingsApp : public MeshApp {
public:
  enum FieldId {
    FIELD_NODE_NAME = 0,
    FIELD_FREQ,
    FIELD_BW,
    FIELD_SF,
    FIELD_CR,
    FIELD_TX,
    FIELD_BLE_PIN,
    FIELD_SCREEN_TIMEOUT,
  };

  struct RowBinding {
    lv_obj_t* row;
    FieldId field;
  };

  void release() override { this->~SettingsApp(); heap_caps_free(this); }

  void onStart(lv_obj_t* layout) override {
    _layout = layout;

    lv_obj_t* t = lv_label_create(layout);
    lv_label_set_text(t, "Settings");
    lv_obj_set_style_text_font(t, &lv_font_montserrat_20, 0);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 14, 12);

    _hint = lv_label_create(layout);
    lv_label_set_text(_hint, "Tap a setting to edit with keyboard");
    lv_obj_set_style_text_color(_hint, lv_color_hex(0x9FAABB), 0);
    lv_obj_align(_hint, LV_ALIGN_TOP_LEFT, 16, 42);

    _list = lv_list_create(layout);
    lv_obj_set_size(_list, LV_PCT(100), LV_PCT(82));
    lv_obj_align(_list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_border_width(_list, 0, 0);
    lv_obj_set_style_bg_color(_list, lv_color_hex(0x101820), 0);

    _syncClockBtn = lv_btn_create(layout);
    lv_obj_set_size(_syncClockBtn, 118, 30);
    lv_obj_align(_syncClockBtn, LV_ALIGN_TOP_RIGHT, -14, 10);
    lv_obj_set_style_bg_color(_syncClockBtn, lv_color_hex(0x2F6DF6), 0);
    lv_obj_add_event_cb(_syncClockBtn, onSyncClockClicked, LV_EVENT_CLICKED, this);
    lv_obj_t* syncLbl = lv_label_create(_syncClockBtn);
    lv_label_set_text(syncLbl, "Sync Clock");
    lv_obj_center(syncLbl);

    refreshRows();
  }

  void onLoop() override {}

  void onClose() override {
    closeEditor();
    _bindings.clear();
    _layout = nullptr;
    _list = nullptr;
    _hint = nullptr;
    _syncClockBtn = nullptr;
  }

  void onMessageReceived(MeshMessage) override {}

private:
  static void onRowClicked(lv_event_t* e) {
    auto* self = static_cast<SettingsApp*>(lv_event_get_user_data(e));
    if (!self) {
      return;
    }
    lv_obj_t* row = lv_event_get_target(e);
    for (const auto& binding : self->_bindings) {
      if (binding.row == row) {
        self->openEditor(binding.field);
        return;
      }
    }
  }

  static void onSaveClicked(lv_event_t* e) {
    auto* self = static_cast<SettingsApp*>(lv_event_get_user_data(e));
    if (!self || self->_editorInput == nullptr) {
      return;
    }

    const char* txt = lv_textarea_get_text(self->_editorInput);
    if (self->applyField(self->_editingField, txt ? txt : "")) {
      lv_label_set_text(self->_hint, "Saved");
      self->closeEditor();
      self->refreshRows();
    } else {
      lv_label_set_text(self->_hint, "Invalid value");
    }
  }

  static void onCancelClicked(lv_event_t* e) {
    auto* self = static_cast<SettingsApp*>(lv_event_get_user_data(e));
    if (!self) {
      return;
    }
    self->closeEditor();
    lv_label_set_text(self->_hint, "Tap a setting to edit with keyboard");
  }

  static void onSyncClockClicked(lv_event_t* e) {
    auto* self = static_cast<SettingsApp*>(lv_event_get_user_data(e));
    if (!self || self->_hint == nullptr) {
      return;
    }

    uint32_t estimated = 0;
    uint8_t samples = 0;
    const bool applied = the_mesh.syncRtcFromHeardAdverts(estimated, samples);

    char msg[96];
    if (samples == 0) {
      snprintf(msg, sizeof(msg), "No usable advert timestamps yet");
    } else if (applied) {
      snprintf(msg, sizeof(msg), "Clock synced from %u adverts", static_cast<unsigned>(samples));
    } else {
      snprintf(msg, sizeof(msg), "Estimate ready (%u refs), clock not moved", static_cast<unsigned>(samples));
    }
    lv_label_set_text(self->_hint, msg);
  }

  static void onKeyboardCancel(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CANCEL) {
      return;
    }
    auto* self = static_cast<SettingsApp*>(lv_event_get_user_data(e));
    if (self) {
      self->closeEditor();
      lv_label_set_text(self->_hint, "Tap a setting to edit with keyboard");
    }
  }

  void refreshRows() {
    if (_list == nullptr) {
      return;
    }

    _bindings.clear();
    lv_obj_clean(_list);

    addRow("Node Name", FIELD_NODE_NAME);
    addRow("Frequency (MHz)", FIELD_FREQ);
    addRow("Bandwidth (kHz)", FIELD_BW);
    addRow("Spreading Factor", FIELD_SF);
    addRow("Coding Rate", FIELD_CR);
    addRow("TX Power (dBm)", FIELD_TX);
    addRow("BLE Pin", FIELD_BLE_PIN);
    addRow("Screen Timeout (sec, 0=manual)", FIELD_SCREEN_TIMEOUT);
  }

  void addRow(const char* title, FieldId field) {
    char rowText[128];
    char val[64];
    currentValue(field, val, sizeof(val));
    snprintf(rowText, sizeof(rowText), "%s: %s", title, val);

    lv_obj_t* row = lv_list_add_btn(_list, LV_SYMBOL_EDIT, rowText);
    lv_obj_set_style_bg_color(row, lv_color_hex(0x1A2532), 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_text_color(row, lv_color_hex(0xEDF4FF), 0);
    lv_obj_set_style_text_font(row, &lv_font_montserrat_14, 0);
    lv_obj_add_event_cb(row, onRowClicked, LV_EVENT_CLICKED, this);
    _bindings.push_back({row, field});
  }

  void currentValue(FieldId field, char* out, size_t outLen) {
    NodePrefs* prefs = the_mesh.getNodePrefs();
    if (!prefs) {
      snprintf(out, outLen, "n/a");
      return;
    }

    switch (field) {
      case FIELD_NODE_NAME:
        snprintf(out, outLen, "%s", prefs->node_name[0] ? prefs->node_name : "node");
        break;
      case FIELD_FREQ:
        snprintf(out, outLen, "%.3f", (double)prefs->freq);
        break;
      case FIELD_BW:
        snprintf(out, outLen, "%.1f", (double)prefs->bw);
        break;
      case FIELD_SF:
        snprintf(out, outLen, "%u", static_cast<unsigned>(prefs->sf));
        break;
      case FIELD_CR:
        snprintf(out, outLen, "%u", static_cast<unsigned>(prefs->cr));
        break;
      case FIELD_TX:
        snprintf(out, outLen, "%d", static_cast<int>(prefs->tx_power_dbm));
        break;
      case FIELD_BLE_PIN:
        snprintf(out, outLen, "%lu", static_cast<unsigned long>(prefs->ble_pin));
        break;
      case FIELD_SCREEN_TIMEOUT:
        snprintf(out, outLen, "%lu", static_cast<unsigned long>(grid::runtime::getScreenTimeoutSec()));
        break;
      default:
        snprintf(out, outLen, "?");
        break;
    }
  }

  void openEditor(FieldId field) {
    closeEditor();
    _editingField = field;

    _editorPanel = lv_obj_create(lv_layer_top());
    lv_obj_set_size(_editorPanel, LV_PCT(94), 170);
    lv_obj_align(_editorPanel, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_set_style_bg_color(_editorPanel, lv_color_hex(0x0F141B), 0);
    lv_obj_set_style_border_color(_editorPanel, lv_color_hex(0x2D3A4A), 0);
    lv_obj_set_style_radius(_editorPanel, 12, 0);

    _editorTitle = lv_label_create(_editorPanel);
    lv_obj_set_style_text_color(_editorTitle, lv_color_hex(0xE7F0FB), 0);
    lv_obj_set_style_text_font(_editorTitle, &lv_font_montserrat_14, 0);
    lv_obj_align(_editorTitle, LV_ALIGN_TOP_LEFT, 10, 8);
    lv_label_set_text(_editorTitle, "Edit value");

    _editorInput = lv_textarea_create(_editorPanel);
    lv_obj_set_size(_editorInput, LV_PCT(96), 50);
    lv_obj_align(_editorInput, LV_ALIGN_TOP_MID, 0, 34);
    lv_textarea_set_one_line(_editorInput, true);
    lv_obj_set_style_text_font(_editorInput, &lv_font_montserrat_16, 0);

    char val[64];
    currentValue(field, val, sizeof(val));
    lv_textarea_set_text(_editorInput, val);

    lv_obj_t* saveBtn = lv_btn_create(_editorPanel);
    lv_obj_set_size(saveBtn, 92, 34);
    lv_obj_align(saveBtn, LV_ALIGN_BOTTOM_LEFT, 12, -10);
    lv_obj_add_event_cb(saveBtn, onSaveClicked, LV_EVENT_CLICKED, this);
    lv_obj_t* saveLbl = lv_label_create(saveBtn);
    lv_label_set_text(saveLbl, "Save");
    lv_obj_center(saveLbl);

    lv_obj_t* cancelBtn = lv_btn_create(_editorPanel);
    lv_obj_set_size(cancelBtn, 92, 34);
    lv_obj_align(cancelBtn, LV_ALIGN_BOTTOM_RIGHT, -12, -10);
    lv_obj_add_event_cb(cancelBtn, onCancelClicked, LV_EVENT_CLICKED, this);
    lv_obj_t* cancelLbl = lv_label_create(cancelBtn);
    lv_label_set_text(cancelLbl, "Cancel");
    lv_obj_center(cancelLbl);

    _keyboard = lv_keyboard_create(lv_layer_top());
    lv_obj_set_size(_keyboard, LV_PCT(100), 180);
    lv_obj_align(_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(_keyboard, _editorInput);
    lv_obj_add_event_cb(_keyboard, onKeyboardCancel, LV_EVENT_CANCEL, this);
    lv_keyboard_set_mode(_keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
  }

  void closeEditor() {
    if (_keyboard) {
      lv_obj_del(_keyboard);
      _keyboard = nullptr;
    }
    if (_editorPanel) {
      lv_obj_del(_editorPanel);
      _editorPanel = nullptr;
    }
    _editorInput = nullptr;
    _editorTitle = nullptr;
  }

  bool applyField(FieldId field, const char* txt) {
    NodePrefs* prefs = the_mesh.getNodePrefs();
    if (!prefs || txt == nullptr || txt[0] == '\0') {
      return false;
    }

    bool radioDirty = false;

    switch (field) {
      case FIELD_NODE_NAME: {
        strncpy(prefs->node_name, txt, sizeof(prefs->node_name) - 1);
        prefs->node_name[sizeof(prefs->node_name) - 1] = '\0';
        break;
      }
      case FIELD_FREQ: {
        float v = strtof(txt, nullptr);
        if (v < 100.0f || v > 1000.0f) return false;
        prefs->freq = v;
        radioDirty = true;
        break;
      }
      case FIELD_BW: {
        float v = strtof(txt, nullptr);
        if (v < 20.0f || v > 500.0f) return false;
        prefs->bw = v;
        radioDirty = true;
        break;
      }
      case FIELD_SF: {
        long v = strtol(txt, nullptr, 10);
        if (v < 5 || v > 12) return false;
        prefs->sf = static_cast<uint8_t>(v);
        radioDirty = true;
        break;
      }
      case FIELD_CR: {
        long v = strtol(txt, nullptr, 10);
        if (v < 5 || v > 8) return false;
        prefs->cr = static_cast<uint8_t>(v);
        radioDirty = true;
        break;
      }
      case FIELD_TX: {
        long v = strtol(txt, nullptr, 10);
        if (v < -9 || v > 22) return false;
        prefs->tx_power_dbm = static_cast<int8_t>(v);
        radioDirty = true;
        break;
      }
      case FIELD_BLE_PIN: {
        unsigned long v = strtoul(txt, nullptr, 10);
        if (v > 999999UL) return false;
        prefs->ble_pin = static_cast<uint32_t>(v);
        break;
      }
      case FIELD_SCREEN_TIMEOUT: {
        unsigned long v = strtoul(txt, nullptr, 10);
        if (v > 24UL * 60UL * 60UL) return false;
        grid::runtime::setScreenTimeoutSec(static_cast<uint32_t>(v));
        grid::runtime::saveScreenTimeoutSec();
        break;
      }
      default:
        return false;
    }

    if (radioDirty) {
      radio_set_params(prefs->freq, prefs->bw, prefs->sf, prefs->cr);
      radio_set_tx_power(prefs->tx_power_dbm);
    }

    the_mesh.savePrefs();
    return true;
  }

  lv_obj_t* _layout = nullptr;
  lv_obj_t* _list = nullptr;
  lv_obj_t* _hint = nullptr;
  lv_obj_t* _syncClockBtn = nullptr;

  lv_obj_t* _editorPanel = nullptr;
  lv_obj_t* _editorTitle = nullptr;
  lv_obj_t* _editorInput = nullptr;
  lv_obj_t* _keyboard = nullptr;

  FieldId _editingField = FIELD_NODE_NAME;
  std::vector<RowBinding> _bindings;
};

class PowerApp : public MeshApp {
public:
  void release() override { this->~PowerApp(); heap_caps_free(this); }
  void onStart(lv_obj_t* layout) override {
    lv_obj_t* t = lv_label_create(layout);
    lv_label_set_text(t, "Power");
    lv_obj_set_style_text_font(t, &lv_font_montserrat_20, 0);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 14, 12);

    _batt = lv_label_create(layout);
    lv_obj_align(_batt, LV_ALIGN_TOP_LEFT, 16, 56);
    refreshBattery();

    lv_obj_t* reboot = lv_btn_create(layout);
    lv_obj_set_size(reboot, 120, 44);
    lv_obj_align(reboot, LV_ALIGN_TOP_LEFT, 16, 94);
    lv_obj_t* rbLbl = lv_label_create(reboot);
    lv_label_set_text(rbLbl, "Reboot");
    lv_obj_center(rbLbl);
    lv_obj_add_event_cb(reboot, [](lv_event_t*) { board.reboot(); }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* sleep = lv_btn_create(layout);
    lv_obj_set_size(sleep, 120, 44);
    lv_obj_align(sleep, LV_ALIGN_TOP_LEFT, 156, 94);
    lv_obj_t* slLbl = lv_label_create(sleep);
    lv_label_set_text(slLbl, "Hibernate");
    lv_obj_center(slLbl);
    lv_obj_add_event_cb(sleep, [](lv_event_t*) {
#ifdef DISPLAY_CLASS
      display.turnOff();
#endif
    #ifdef PIN_TFT_LEDA_CTL
    #ifdef PIN_TFT_LEDA_CTL_ACTIVE
      ledcWrite(0, 0);
    #else
      ledcWrite(0, 200);
    #endif
    #endif
      radio_driver.powerOff();
#ifdef PIN_VEXT_EN
#ifdef PIN_VEXT_EN_ACTIVE
      digitalWrite(PIN_VEXT_EN, !PIN_VEXT_EN_ACTIVE);
#else
      digitalWrite(PIN_VEXT_EN, LOW);
#endif
#endif
      board.enterDeepSleep(0, PIN_USER_BTN);
    }, LV_EVENT_CLICKED, nullptr);
  }
  void onLoop() override { refreshBattery(); }
  void onClose() override { _batt = nullptr; }
  void onMessageReceived(MeshMessage) override {}
private:
  void refreshBattery() {
    if (_batt == nullptr) return;
    char buf[48];
    snprintf(buf, sizeof(buf), "Battery: %u mV", (unsigned)board.getBattMilliVolts());
    lv_label_set_text(_batt, buf);
  }
  lv_obj_t* _batt = nullptr;
};

}  // namespace

void registerChatApp(WindowManager& wm, MeshBridge& bridge) {
  registerMessengerStubApp(wm, bridge);
}

void registerNodesApp(WindowManager& wm, MeshBridge& bridge) {
  wm.registerApp({"nodes", "Nodes", LV_SYMBOL_LIST,
                  [&bridge]() -> MeshApp* { return WindowManager::createInPsram<NodesApp>(&bridge); }});
}

void registerRadioApp(WindowManager& wm) {
  wm.registerApp({"radio", "Radio", LV_SYMBOL_WIFI,
                  []() -> MeshApp* { return WindowManager::createInPsram<RadioApp>(); }});
}

void registerBleApp(WindowManager& wm) {
  wm.registerApp({"ble", "BLE", LV_SYMBOL_BLUETOOTH,
                  []() -> MeshApp* { return WindowManager::createInPsram<BleApp>(); }});
}

void registerSettingsApp(WindowManager& wm) {
  wm.registerApp({"settings", "Settings", LV_SYMBOL_SETTINGS,
                  []() -> MeshApp* { return WindowManager::createInPsram<SettingsApp>(); }});
}

void registerPowerApp(WindowManager& wm) {
  wm.registerApp({"power", "Power", LV_SYMBOL_POWER,
                  []() -> MeshApp* { return WindowManager::createInPsram<PowerApp>(); }});
}
