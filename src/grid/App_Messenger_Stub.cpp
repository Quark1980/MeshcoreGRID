#include "WindowManager.h"

#include <vector>

#include "Packet.h"

namespace {

class MessengerStubApp : public MeshApp {
public:
  explicit MessengerStubApp(MeshBridge* bridge)
      : _bridge(bridge), _layout(nullptr), _list(nullptr) {}

  void release() override {
    this->~MessengerStubApp();
    heap_caps_free(this);
  }

  void onStart(lv_obj_t* layout) override {
    _layout = layout;

    lv_obj_t* title = lv_label_create(layout);
    lv_label_set_text(title, "Messenger");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 14, 12);

    _list = lv_list_create(layout);
    lv_obj_set_size(_list, LV_PCT(100), LV_PCT(86));
    lv_obj_align(_list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(_list, lv_color_hex(0x12161D), 0);
    lv_obj_set_style_border_width(_list, 0, 0);

    if (_bridge) {
      _bridge->subscribe(PAYLOAD_TYPE_TXT_MSG, [this](const MeshMessage& msg) {
        onMessageReceived(msg);
      });
    }

    appendLine("Waiting for MeshCore text packets...");
  }

  void onLoop() override {}

  void onClose() override {
    if (_bridge) {
      _bridge->clearSubscribers(PAYLOAD_TYPE_TXT_MSG);
    }
    _layout = nullptr;
    _list = nullptr;
  }

  void onMessageReceived(MeshMessage msg) override {
    if (_list == nullptr || msg.packetType != PAYLOAD_TYPE_TXT_MSG) {
      return;
    }

    std::string line = "[";
    line += std::to_string(msg.rssi);
    line += " dBm] ";
    line += msg.text.empty() ? "(empty)" : msg.text;
    appendLine(line.c_str());
  }

private:
  void appendLine(const char* text) {
    if (_list == nullptr) {
      return;
    }
    lv_obj_t* row = lv_list_add_text(_list, text);
    lv_obj_set_style_text_color(row, lv_color_hex(0xDDE5F0), 0);
  }

  MeshBridge* _bridge;
  lv_obj_t* _layout;
  lv_obj_t* _list;
};

}  // namespace

void registerMessengerStubApp(WindowManager& wm, MeshBridge& bridge) {
  wm.registerApp({
      "chat",
      "Chat",
      LV_SYMBOL_EDIT,
      [&bridge]() -> MeshApp* { return WindowManager::createInPsram<MessengerStubApp>(&bridge); },
  });
}
