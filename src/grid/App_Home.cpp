#include "WindowManager.h"

#include <algorithm>
#include <vector>

namespace {

class HomeApp : public MeshApp {
public:
  explicit HomeApp(WindowManager* wm)
      : _wm(wm), _layout(nullptr), _chatBadge(nullptr), _chatBadgeLabel(nullptr) {}

  void release() override {
    this->~HomeApp();
    heap_caps_free(this);
  }

  void onStart(lv_obj_t* layout) override {
    _layout = layout;

    lv_obj_t* title = lv_label_create(layout);
    lv_label_set_text(title, "App Drawer");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 14, 10);

    lv_obj_t* grid = lv_obj_create(layout);
    lv_obj_set_size(grid, LV_PCT(100), LV_PCT(88));
    lv_obj_align(grid, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 10, 0);

    static lv_coord_t col[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row[] = {86, 86, 86, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(grid, col, row);

    const auto& apps = _wm->apps();
    uint32_t visibleIdx = 0;
    for (uint32_t i = 0; i < apps.size(); ++i) {
      if (strcmp(apps[i].id, "home") == 0) {
        continue;
      }

      lv_obj_t* card = lv_btn_create(grid);
      lv_obj_set_grid_cell(card,
                           LV_GRID_ALIGN_STRETCH,
                           visibleIdx % 2,
                           1,
                           LV_GRID_ALIGN_STRETCH,
                           (visibleIdx / 2) % 3,
                           1);
      lv_obj_set_style_radius(card, 14, 0);
      lv_obj_set_style_bg_color(card, lv_color_hex(0x1A1F27), 0);
      lv_obj_set_style_border_color(card, lv_color_hex(0x2E3642), 0);
      lv_obj_set_style_border_width(card, 1, 0);
      lv_obj_set_style_translate_y(card, 0, 0);
      lv_obj_set_style_shadow_width(card, 0, 0);
      lv_obj_set_style_shadow_opa(card, LV_OPA_TRANSP, 0);

      lv_obj_t* icon = lv_label_create(card);
      lv_label_set_text(icon, apps[i].icon ? apps[i].icon : "*");
      lv_obj_set_style_text_font(icon, &lv_font_montserrat_20, 0);
      lv_obj_align(icon, LV_ALIGN_CENTER, 0, -12);

      lv_obj_t* name = lv_label_create(card);
      lv_label_set_text(name, apps[i].label);
      lv_obj_set_style_text_font(name, &lv_font_montserrat_14, 0);
      lv_obj_align(name, LV_ALIGN_CENTER, 0, 18);

      if (strcmp(apps[i].id, "chat") == 0) {
        createChatBadge(card);
        updateChatBadge();
      }

      lv_obj_add_event_cb(card, [](lv_event_t* e) {
        const char* id = static_cast<const char*>(lv_event_get_user_data(e));
        if (id) {
          WindowManager::instance().openApp(id, true);
        }
      }, LV_EVENT_CLICKED, (void*)apps[i].id);
      lv_obj_add_event_cb(card, onCardPressed, LV_EVENT_PRESSED, nullptr);
      lv_obj_add_event_cb(card, onCardReleased, LV_EVENT_RELEASED, nullptr);
      lv_obj_add_event_cb(card, onCardReleased, LV_EVENT_PRESS_LOST, nullptr);
      ++visibleIdx;
    }
  }

  void onLoop() override { updateChatBadge(); }

  void onClose() override {
    _layout = nullptr;
    _chatBadge = nullptr;
    _chatBadgeLabel = nullptr;
  }

  void onMessageReceived(MeshMessage msg) override {
    (void)msg;
  }

private:
  static void applyPressedVisual(lv_obj_t* card, bool pressed) {
    if (card == nullptr) {
      return;
    }

    if (pressed) {
      lv_obj_set_style_bg_color(card, lv_color_hex(0x243244), 0);
      lv_obj_set_style_border_color(card, lv_color_hex(0xFFB300), 0);
      lv_obj_set_style_border_width(card, 2, 0);
      lv_obj_set_style_translate_y(card, 2, 0);
      lv_obj_set_style_shadow_width(card, 12, 0);
      lv_obj_set_style_shadow_color(card, lv_color_hex(0xFFB300), 0);
      lv_obj_set_style_shadow_opa(card, LV_OPA_30, 0);
    } else {
      lv_obj_set_style_bg_color(card, lv_color_hex(0x1A1F27), 0);
      lv_obj_set_style_border_color(card, lv_color_hex(0x2E3642), 0);
      lv_obj_set_style_border_width(card, 1, 0);
      lv_obj_set_style_translate_y(card, 0, 0);
      lv_obj_set_style_shadow_width(card, 0, 0);
      lv_obj_set_style_shadow_opa(card, LV_OPA_TRANSP, 0);
    }
  }

  static void onCardPressed(lv_event_t* e) {
    lv_obj_t* card = lv_event_get_target(e);
    applyPressedVisual(card, true);
  }

  static void onCardReleased(lv_event_t* e) {
    lv_obj_t* card = lv_event_get_target(e);
    applyPressedVisual(card, false);
  }

  void createChatBadge(lv_obj_t* card) {
    _chatBadge = lv_obj_create(card);
    lv_obj_set_size(_chatBadge, 20, 20);
    lv_obj_align(_chatBadge, LV_ALIGN_TOP_RIGHT, -8, 8);
    lv_obj_set_style_radius(_chatBadge, 10, 0);
    lv_obj_set_style_bg_color(_chatBadge, lv_color_hex(0xE64A5F), 0);
    lv_obj_set_style_border_width(_chatBadge, 0, 0);
    lv_obj_clear_flag(_chatBadge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_chatBadge, LV_OBJ_FLAG_HIDDEN);

    _chatBadgeLabel = lv_label_create(_chatBadge);
    lv_obj_set_style_text_font(_chatBadgeLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(_chatBadgeLabel, lv_color_white(), 0);
    lv_obj_center(_chatBadgeLabel);
  }

  void updateChatBadge() {
    if (_chatBadge == nullptr || _chatBadgeLabel == nullptr) {
      return;
    }

    const int unreadCount = MeshBridge::instance().getTotalUnreadCount();
    if (unreadCount <= 0) {
      lv_obj_add_flag(_chatBadge, LV_OBJ_FLAG_HIDDEN);
      return;
    }

    const int clamped = std::min(unreadCount, 99);
    char text[4];
    snprintf(text, sizeof(text), "%d", clamped);
    lv_label_set_text(_chatBadgeLabel, text);
    lv_obj_clear_flag(_chatBadge, LV_OBJ_FLAG_HIDDEN);
  }

  WindowManager* _wm;
  lv_obj_t* _layout;
  lv_obj_t* _chatBadge;
  lv_obj_t* _chatBadgeLabel;
};

}  // namespace

void registerHomeApp(WindowManager& wm) {
  wm.registerApp({
      "home",
      "Home",
      LV_SYMBOL_HOME,
      [&wm]() -> MeshApp* { return WindowManager::createInPsram<HomeApp>(&wm); },
  });
}
