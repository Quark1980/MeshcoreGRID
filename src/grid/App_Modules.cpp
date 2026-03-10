#include "GridApps.h"

#include "Packet.h"
#include "WindowManager.h"
#include "target.h"

#include <string>

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
  explicit RadioApp(MeshBridge* bridge) : _bridge(bridge), _snr(nullptr), _rssi(nullptr) {}
  void release() override { this->~RadioApp(); heap_caps_free(this); }
  void onStart(lv_obj_t* layout) override {
    lv_obj_t* t = lv_label_create(layout);
    lv_label_set_text(t, "Radio");
    lv_obj_set_style_text_font(t, &lv_font_montserrat_20, 0);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 14, 12);

    lv_obj_t* card = lv_obj_create(layout);
    lv_obj_set_size(card, LV_PCT(92), 160);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 56);
    lv_obj_set_style_radius(card, 14, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1A1F27), 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x2F3947), 0);

    char buf[64];
    snprintf(buf, sizeof(buf), "LoRa %.3f MHz", (double)LORA_FREQ);
    lv_obj_t* freq = lv_label_create(card);
    lv_label_set_text(freq, buf);
    lv_obj_align(freq, LV_ALIGN_TOP_LEFT, 12, 12);

    snprintf(buf, sizeof(buf), "BW %.1f  SF %d", (double)LORA_BW, (int)LORA_SF);
    lv_obj_t* mod = lv_label_create(card);
    lv_label_set_text(mod, buf);
    lv_obj_align(mod, LV_ALIGN_TOP_LEFT, 12, 36);

    _rssi = lv_label_create(card);
    lv_label_set_text(_rssi, "RSSI: waiting");
    lv_obj_align(_rssi, LV_ALIGN_TOP_LEFT, 12, 68);

    _snr = lv_label_create(card);
    lv_label_set_text(_snr, "SNR: waiting");
    lv_obj_align(_snr, LV_ALIGN_TOP_LEFT, 12, 92);

    if (_bridge) {
      _bridge->subscribe(PAYLOAD_TYPE_TXT_MSG, [this](const MeshMessage& msg) { onMessageReceived(msg); });
      _bridge->subscribe(PAYLOAD_TYPE_GRP_TXT, [this](const MeshMessage& msg) { onMessageReceived(msg); });
    }
  }
  void onLoop() override {}
  void onClose() override {
    if (_bridge) {
      _bridge->clearSubscribers(PAYLOAD_TYPE_TXT_MSG);
      _bridge->clearSubscribers(PAYLOAD_TYPE_GRP_TXT);
    }
    _rssi = _snr = nullptr;
  }
  void onMessageReceived(MeshMessage msg) override {
    if (_rssi == nullptr || _snr == nullptr) return;
    char buf[32];
    snprintf(buf, sizeof(buf), "RSSI: %d dBm", (int)msg.rssi);
    lv_label_set_text(_rssi, buf);
    snprintf(buf, sizeof(buf), "SNR: %d dB", (int)msg.snr);
    lv_label_set_text(_snr, buf);
  }
private:
  MeshBridge* _bridge;
  lv_obj_t* _snr;
  lv_obj_t* _rssi;
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
                  []() -> MeshApp* { return WindowManager::createInPsram<RadioApp>(&MeshBridge::instance()); }});
}

void registerBleApp(WindowManager& wm) {
  wm.registerApp({"ble", "BLE", LV_SYMBOL_WIFI,
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
