#include "GridApps.h"

#include "MyMesh.h"
#include "Packet.h"
#include "RadioTelemetryStore.h"
#include "WindowManager.h"
#include "target.h"

#include <string>
#include <vector>

void registerMessengerStubApp(WindowManager& wm, MeshBridge& bridge);

namespace {

class NodesApp : public MeshApp {
public:
  explicit NodesApp(MeshBridge* bridge) : _bridge(bridge), _list(nullptr) {}
  void release() override { this->~NodesApp(); heap_caps_free(this); }
  void onStart(lv_obj_t* layout) override {
    lv_obj_t* title = lv_label_create(layout);
    lv_label_set_text(title, "Nodes Heard");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 14, 12);

    _list = lv_list_create(layout);
    lv_obj_set_size(_list, LV_PCT(100), LV_PCT(86));
    lv_obj_align(_list, LV_ALIGN_BOTTOM_MID, 0, 0);

    if (_bridge) {
      _bridge->subscribe(GRID_EVT_NODE_ADVERT, [this](const MeshMessage& msg) { onMessageReceived(msg); });
    }
  }
  void onLoop() override {}
  void onClose() override { if (_bridge) _bridge->clearSubscribers(GRID_EVT_NODE_ADVERT); _list = nullptr; }
  void onMessageReceived(MeshMessage msg) override {
    if (_list == nullptr || msg.packetType != GRID_EVT_NODE_ADVERT) return;
    lv_list_add_text(_list, msg.text.c_str());
  }
private:
  MeshBridge* _bridge;
  lv_obj_t* _list;
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
        _settingsText(nullptr),
        _rssi(nullptr),
        _snr(nullptr),
        _updated(nullptr),
        _rawDiag(nullptr),
        _rxStats(nullptr),
        _rawList(nullptr),
        _lastRefreshMs(0),
        _lastPacketSeq(0) {}
  void release() override { this->~RadioApp(); heap_caps_free(this); }
  void onStart(lv_obj_t* layout) override {
    lv_obj_t* t = lv_label_create(layout);
    lv_label_set_text(t, "Radio");
    lv_obj_set_style_text_font(t, &lv_font_montserrat_20, 0);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 14, 12);

    _tabview = lv_tabview_create(layout, LV_DIR_TOP, 36);
    lv_obj_set_size(_tabview, LV_PCT(96), LV_PCT(84));
    lv_obj_align(_tabview, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_obj_t* metricsTab = lv_tabview_add_tab(_tabview, "Metrics");
    lv_obj_t* settingsTab = lv_tabview_add_tab(_tabview, "Settings");
    lv_obj_t* rawTab = lv_tabview_add_tab(_tabview, "Raw RX");

    lv_obj_t* card = lv_obj_create(metricsTab);
    lv_obj_set_size(card, LV_PCT(98), LV_PCT(96));
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1A1F27), 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x2F3947), 0);

    lv_obj_t* settingsHint = lv_label_create(card);
    lv_label_set_text(settingsHint, "Open Settings tab for active radio profile");
    lv_obj_set_style_text_color(settingsHint, lv_color_hex(0x9FAABB), 0);
    lv_obj_align(settingsHint, LV_ALIGN_TOP_LEFT, 12, 12);

    lv_obj_t* settingsCard = lv_obj_create(settingsTab);
    lv_obj_set_size(settingsCard, LV_PCT(98), LV_PCT(96));
    lv_obj_align(settingsCard, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_radius(settingsCard, 12, 0);
    lv_obj_set_style_bg_color(settingsCard, lv_color_hex(0x1A1F27), 0);
    lv_obj_set_style_border_color(settingsCard, lv_color_hex(0x2F3947), 0);

    lv_obj_t* title2 = lv_label_create(settingsCard);
    lv_label_set_text(title2, "Current Settings");
    lv_obj_set_style_text_color(title2, lv_color_hex(0xE2E8F0), 0);
    lv_obj_set_style_text_font(title2, &lv_font_montserrat_16, 0);
    lv_obj_align(title2, LV_ALIGN_TOP_LEFT, 12, 10);

    _settingsText = lv_label_create(settingsCard);
    lv_obj_set_style_text_color(_settingsText, lv_color_hex(0xD6E0EC), 0);
    lv_obj_set_style_text_font(_settingsText, &lv_font_montserrat_14, 0);
    lv_obj_align(_settingsText, LV_ALIGN_TOP_LEFT, 12, 38);
    lv_label_set_text(_settingsText, "Loading...\n");

    _rssi = lv_label_create(card);
    lv_label_set_text(_rssi, "RSSI: waiting");
    lv_obj_align(_rssi, LV_ALIGN_TOP_LEFT, 12, 104);

    _snr = lv_label_create(card);
    lv_label_set_text(_snr, "SNR: waiting");
    lv_obj_align(_snr, LV_ALIGN_TOP_LEFT, 12, 126);

    _updated = lv_label_create(card);
    lv_label_set_text(_updated, "Updated: --");
    lv_obj_set_style_text_color(_updated, lv_color_hex(0x9FAABB), 0);
    lv_obj_align(_updated, LV_ALIGN_TOP_LEFT, 12, 148);

    _rawList = lv_list_create(rawTab);
    lv_obj_set_size(_rawList, LV_PCT(100), LV_PCT(100));
    lv_obj_align(_rawList, LV_ALIGN_TOP_MID, 0, 0);

    _rawDiag = lv_label_create(rawTab);
    lv_obj_set_style_text_color(_rawDiag, lv_color_hex(0x9FAABB), 0);
    lv_obj_align(_rawDiag, LV_ALIGN_TOP_LEFT, 8, 4);
    lv_label_set_text(_rawDiag, "raw packets: 0  last: --");

    _rxStats = lv_label_create(rawTab);
    lv_obj_set_style_text_color(_rxStats, lv_color_hex(0x9FAABB), 0);
    lv_obj_align(_rxStats, LV_ALIGN_TOP_LEFT, 8, 20);
    lv_label_set_text(_rxStats, "dispatcher rx flood: 0 direct: 0");

    _rxHitsLabel = lv_label_create(rawTab);
    lv_obj_set_style_text_color(_rxHitsLabel, lv_color_hex(0x9FAABB), 0);
    lv_obj_align(_rxHitsLabel, LV_ALIGN_TOP_LEFT, 8, 36);
    lv_label_set_text(_rxHitsLabel, "rx calls: 0");

    lv_obj_set_style_pad_top(_rawList, 56, 0);

    lv_obj_t* initHint = lv_list_add_text(_rawList, "Waiting for packets...");
    (void)initHint;

    refreshMetrics();
    refreshRawPackets(true);
  }
  void onLoop() override {
    const uint32_t now = millis();
    if (now - _lastRefreshMs < 250) {
      return;
    }
    _lastRefreshMs = now;

    refreshMetrics();
    refreshRawPackets(false);
  }
  void onClose() override {
    _tabview = nullptr;
    _settingsText = nullptr;
    _rssi = nullptr;
    _snr = nullptr;
    _updated = nullptr;
    _rawDiag = nullptr;
    _rxStats = nullptr;
    _rxHitsLabel = nullptr;
    _rawList = nullptr;
    _rawSnapshot.clear();
  }
  void onMessageReceived(MeshMessage) override {}

private:
  void refreshMetrics() {
    if (_rssi == nullptr || _snr == nullptr || _updated == nullptr) {
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

    if (_settingsText) {
      char settingsBuf[220];
      snprintf(settingsBuf,
               sizeof(settingsBuf),
               "FREQ: %.3f MHz\nBW: %.1f kHz\nSF: %d\nCR: %d\nTX: %d dBm\nAF: %.1f\nDuty limit: %.1f%%",
               (double)freq,
               (double)bw,
               sf,
               cr,
               tx,
               (double)af,
               (double)duty);
      lv_label_set_text(_settingsText, settingsBuf);
    }

    int16_t rssi = 0;
    int8_t snr = 0;
    uint32_t ts = 0;
    if (!grid::radio_telemetry::getLatestMetrics(rssi, snr, ts)) {
      lv_label_set_text(_rssi, "RSSI: waiting");
      lv_label_set_text(_snr, "SNR: waiting");
      lv_label_set_text(_updated, "Updated: --");
      return;
    }

    char buf[40];
    snprintf(buf, sizeof(buf), "RSSI: %d dBm", (int)rssi);
    lv_label_set_text(_rssi, buf);
    snprintf(buf, sizeof(buf), "SNR: %d dB", (int)snr);
    lv_label_set_text(_snr, buf);
    snprintf(buf, sizeof(buf), "Updated: %lu", (unsigned long)ts);
    lv_label_set_text(_updated, buf);
  }

  void refreshRawPackets(bool force) {
    if (_rawList == nullptr) {
      return;
    }

    if (_rawDiag) {
      char diag[64];
      snprintf(diag,
               sizeof(diag),
               "raw packets: %lu  last: %lu",
               (unsigned long)grid::radio_telemetry::packetCount(),
               (unsigned long)grid::radio_telemetry::lastRawPacketTimestamp());
      lv_label_set_text(_rawDiag, diag);
    }

    if (_rxStats) {
      char rxBuf[72];
      snprintf(rxBuf,
               sizeof(rxBuf),
               "dispatcher rx flood: %lu direct: %lu",
               (unsigned long)the_mesh.getNumRecvFlood(),
               (unsigned long)the_mesh.getNumRecvDirect());
      lv_label_set_text(_rxStats, rxBuf);
    }

    if (_rxHitsLabel) {
      char hitsBuf[72];
      snprintf(hitsBuf,
               sizeof(hitsBuf),
               "rx calls: %lu  disp raw: %lu",
               (unsigned long)grid::radio_telemetry::getRxCallHits(),
               (unsigned long)grid::radio_telemetry::getDispatcherRxRawHits());
      lv_label_set_text(_rxHitsLabel, hitsBuf);
    }

    const uint32_t seq = grid::radio_telemetry::packetSequence();
    if (!force && seq == _lastPacketSeq) {
      return;
    }
    _lastPacketSeq = seq;

    grid::radio_telemetry::snapshotRawPackets(_rawSnapshot, 20);

    lv_obj_clean(_rawList);
    if (_rawSnapshot.empty()) {
      lv_list_add_text(_rawList, "No packets yet");
      return;
    }

    for (size_t i = 0; i < _rawSnapshot.size(); ++i) {
      const auto& p = _rawSnapshot[i];
      char header[64];
      snprintf(header,
               sizeof(header),
               "t:%lu  r:%d  s:%d  len:%u",
               (unsigned long)p.timestamp,
               (int)p.rssi,
               (int)p.snr,
               (unsigned)p.byteLen);
      lv_list_add_text(_rawList, header);
      lv_obj_t* row = lv_list_add_btn(_rawList, nullptr, p.hex[0] ? p.hex : "(empty)");
      lv_obj_set_style_text_font(row, &lv_font_montserrat_14, 0);
    }
  }

  lv_obj_t* _tabview;
  lv_obj_t* _settingsText;
  lv_obj_t* _snr;
  lv_obj_t* _rssi;
  lv_obj_t* _updated;
  lv_obj_t* _rawDiag;
  lv_obj_t* _rxStats;
  lv_obj_t* _rxHitsLabel;
  lv_obj_t* _rawList;
  uint32_t _lastRefreshMs;
  uint32_t _lastPacketSeq;
  std::vector<grid::radio_telemetry::RawPacketEntry> _rawSnapshot;
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
    lv_obj_set_size(card, LV_PCT(92), 176);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 56);
    lv_obj_set_style_radius(card, 14, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1A1F27), 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x2F3947), 0);

    _state = lv_label_create(card);
    lv_obj_align(_state, LV_ALIGN_TOP_LEFT, 12, 14);

    char pinText[64];
#ifdef BLE_PIN_CODE
    snprintf(pinText, sizeof(pinText), "PIN: %d", (int)BLE_PIN_CODE);
#else
    snprintf(pinText, sizeof(pinText), "PIN: n/a");
#endif
    lv_obj_t* pin = lv_label_create(card);
    lv_label_set_text(pin, pinText);
    lv_obj_set_style_text_font(pin, &lv_font_montserrat_20, 0);
    lv_obj_align(pin, LV_ALIGN_TOP_LEFT, 12, 50);

    _toggleBtn = lv_btn_create(card);
    lv_obj_set_size(_toggleBtn, 136, 40);
    lv_obj_align(_toggleBtn, LV_ALIGN_BOTTOM_LEFT, 12, -12);
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
  void onLoop() override {}
  void onClose() override { _state = nullptr; _toggleBtn = nullptr; _toggleText = nullptr; }
  void onMessageReceived(MeshMessage) override {}

private:
  void refreshUi() {
    if (_state == nullptr || _toggleText == nullptr) {
      return;
    }

    const bool enabled = MeshBridge::instance().isBleEnabled();
    lv_label_set_text(_state, enabled ? "Companion BLE: enabled" : "Companion BLE: disabled");
    lv_label_set_text(_toggleText, enabled ? "Turn BLE Off" : "Turn BLE On");
  }

  lv_obj_t* _state = nullptr;
  lv_obj_t* _toggleBtn = nullptr;
  lv_obj_t* _toggleText = nullptr;
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
    lv_obj_add_event_cb(sleep, [](lv_event_t*) { board.powerOff(); }, LV_EVENT_CLICKED, nullptr);
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
                  []() -> MeshApp* { return WindowManager::createInPsram<SimpleCardApp>("Settings", "MeshCore app parity settings"); }});
}

void registerPowerApp(WindowManager& wm) {
  wm.registerApp({"power", "Power", LV_SYMBOL_POWER,
                  []() -> MeshApp* { return WindowManager::createInPsram<PowerApp>(); }});
}
