#include "grid_chat.h"

#include "grid_chat_overview.h"
#include "grid_ui_common.h"

#include <Arduino.h>
#include <cstring>
#include <map>
#include <string>
#include <vector>


// --- Shared Data ---
struct Message {
  std::string sender;
  std::string text;
  std::string time;
  bool is_me;
};

static std::map<std::string, std::vector<Message>> chat_history;
std::vector<std::string> hashtags = { "#mesh", "#general", "#tech" };
std::vector<std::string> contacts = { "Bob", "Alice", "Charlie" };

static std::string current_conversation = "#mesh";
static lv_obj_t *msg_list = NULL;
static lv_obj_t *ta_input = NULL;

// --- Helpers ---
void add_bubble(lv_obj_t *list, const Message &msg) {
  lv_obj_t *item = lv_obj_create(list);
  lv_obj_set_width(item, LV_PCT(100));
  lv_obj_set_height(item, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(item, 0, 0);
  lv_obj_set_style_border_width(item, 0, 0);
  lv_obj_set_style_pad_all(item, 5, 0);
  lv_obj_set_flex_flow(item, LV_FLEX_FLOW_COLUMN);

  lv_obj_t *info = grid_create_label(item, msg.sender.c_str(), &lv_font_montserrat_14);
  lv_obj_set_style_text_color(info, GRID_COLOR_DIM, 0);

  lv_obj_t *bubble = lv_obj_create(item);
  lv_obj_set_width(bubble, LV_PCT(80));
  lv_obj_set_height(bubble, LV_SIZE_CONTENT);
  lv_obj_set_style_radius(bubble, 12, 0);
  lv_obj_set_style_pad_all(bubble, 8, 0);
  lv_obj_set_style_border_width(bubble, 0, 0);

  if (msg.is_me) {
    lv_obj_set_align(item, LV_ALIGN_TOP_RIGHT);
    lv_obj_set_style_align(info, LV_ALIGN_TOP_RIGHT, 0);
    lv_obj_set_style_align(bubble, LV_ALIGN_TOP_RIGHT, 0);
    lv_obj_set_style_bg_color(bubble, GRID_COLOR_AMBER, 0);
    lv_obj_set_style_text_color(bubble, GRID_COLOR_BG, 0);
  } else {
    lv_obj_set_align(item, LV_ALIGN_TOP_LEFT);
    lv_obj_set_style_align(info, LV_ALIGN_TOP_LEFT, 0);
    lv_obj_set_style_align(bubble, LV_ALIGN_TOP_LEFT, 0);
    lv_obj_set_style_bg_color(bubble, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_color(bubble, lv_color_hex(0xFFFFFF), 0);
  }

  lv_obj_t *lbl = lv_label_create(bubble);
  lv_label_set_text(lbl, msg.text.c_str());
  lv_obj_set_width(lbl, LV_PCT(100));
  lv_obj_set_style_text_color(lbl, msg.is_me ? GRID_COLOR_BG : lv_color_hex(0xFFFFFF), 0);

  lv_obj_t *time = grid_create_label(bubble, msg.time.c_str(), &lv_font_montserrat_14);
  lv_obj_set_style_text_color(time, msg.is_me ? GRID_COLOR_BG : GRID_COLOR_DIM, 0);
  lv_obj_align(time, LV_ALIGN_BOTTOM_RIGHT, 0, 3);

  lv_obj_scroll_to_view(item, LV_ANIM_OFF);
}

void switch_to_channel(const char *name) {
  if (!name) return;
  current_conversation = name;

  // Reset unread count
  if (name[0] == '#')
    channel_unread[name] = 0;
  else
    dm_unread[name] = 0;

  update_all_badges();
  switch_app(&chat_app);
}

static void send_msg_cb(lv_event_t *e) {
  const char *txt = lv_textarea_get_text(ta_input);
  if (strlen(txt) == 0) return;

  Message msg = { "ME", txt, "23:50", true };
  chat_history[current_conversation].push_back(msg);
  add_bubble(msg_list, msg);
  lv_textarea_set_text(ta_input, "");

  /* [FUTURE: MESHCORE Integration]
   * Node node = MeshCore.findNode(current_conversation);
   * node.send(txt);
   */
}

static void back_btn_cb(lv_event_t *e) {
  switch_app(&chat_overview_app);
}

void chat_init(GridApp *app) {
  app->screen = lv_obj_create(NULL);
  lv_obj_add_style(app->screen, &style_grid_main, 0);

  // Custom Back Button (since global one only goes to launcher)
  lv_obj_t *back = grid_create_btn(app->screen, LV_SYMBOL_LEFT);
  lv_obj_set_size(back, 45, 35);
  lv_obj_align(back, LV_ALIGN_TOP_LEFT, 5, 35);
  lv_obj_add_event_cb(back, back_btn_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *title = grid_create_label(app->screen, current_conversation.c_str(), &lv_font_montserrat_20);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 40);
  lv_obj_set_style_text_color(title, GRID_COLOR_AMBER, 0);

  msg_list = lv_obj_create(app->screen);
  lv_obj_set_size(msg_list, LV_HOR_RES, LV_VER_RES - 110);
  lv_obj_align(msg_list, LV_ALIGN_TOP_MID, 0, 75);
  lv_obj_set_flex_flow(msg_list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_bg_opa(msg_list, 0, 0);
  lv_obj_set_style_border_width(msg_list, 0, 0);
  lv_obj_set_scrollbar_mode(msg_list, LV_SCROLLBAR_MODE_AUTO);

  // Load History
  for (auto &m : chat_history[current_conversation])
    add_bubble(msg_list, m);

  // Input Footer
  lv_obj_t *footer = lv_obj_create(app->screen);
  lv_obj_set_size(footer, LV_HOR_RES, 70);
  lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_color(footer, lv_color_hex(0x000000), 0);

  ta_input = grid_create_textarea(footer, "Message...");
  lv_obj_set_size(ta_input, 160, 45);
  lv_obj_align(ta_input, LV_ALIGN_LEFT_MID, 10, 0);

  lv_obj_t *send = grid_create_btn(footer, LV_SYMBOL_PLAY);
  lv_obj_set_size(send, 60, 45);
  lv_obj_align(send, LV_ALIGN_RIGHT_MID, -10, 0);
  lv_obj_add_event_cb(send, send_msg_cb, LV_EVENT_CLICKED, NULL);
}

void chat_update(GridApp *app) {
  if (lv_screen_active() != app->screen) {
    // Simulation of incoming messages in background
    static uint32_t last = 0;
    if (millis() - last > 20000) {
      last = millis();
      channel_unread["#mesh"]++;
      dm_unread["Bob"]++;
      update_all_badges();
      Serial.println("GRID: Simulated unread messages received.");
    }
    return;
  }
}

GridApp chat_app = { "Conversation", "💬", NULL, chat_init, chat_update, NULL };
