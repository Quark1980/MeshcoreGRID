#include "grid_chat.h"

#include "grid_chat_overview.h"
#include "grid_ui_common.h"

#include <Arduino.h>
#include <cstring>
#include <map>
#include <string>
#include <vector>

// --- Data Structures ---
struct Message {
  std::string sender;
  std::string text;
  std::string time;
  uint8_t hop_count;
  bool is_me;
};

enum ChatMode { MODE_CHANNEL, MODE_DM };

static std::map<std::string, std::vector<Message>> chat_history;
static std::map<std::string, uint32_t> repeat_counters;

static std::string current_target = "#mesh";
static ChatMode current_mode = MODE_CHANNEL;

static lv_obj_t *msg_list = NULL;
static lv_obj_t *ta_input = NULL;
static lv_obj_t *title_label = NULL;

extern std::vector<std::string> hashtags;
extern std::vector<std::string> contacts;

// --- Helpers ---
void add_bubble(lv_obj_t *list, const Message &msg) {
  lv_obj_t *item = lv_obj_create(list);
  lv_obj_set_width(item, LV_PCT(100));
  lv_obj_set_height(item, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(item, 0, 0);
  lv_obj_set_style_border_width(item, 0, 0);
  lv_obj_set_style_pad_all(item, 4, 0); // Tight padding
  lv_obj_set_flex_flow(item, LV_FLEX_FLOW_COLUMN);

  // Metadata: Name + Repeat/Hop
  char info_buf[64];
  if (msg.is_me) {
    snprintf(info_buf, sizeof(info_buf), "ME | Repeat: %d", repeat_counters[current_target]);
  } else {
    snprintf(info_buf, sizeof(info_buf), "%s | Hops: %d", msg.sender.c_str(), msg.hop_count);
  }

  lv_obj_t *info = grid_create_label(item, info_buf, &lv_font_montserrat_14);
  lv_obj_set_style_text_color(info, GRID_COLOR_DIM, 0);

  lv_obj_t *bubble = lv_obj_create(item);
  lv_obj_set_width(bubble, LV_PCT(85));
  lv_obj_set_height(bubble, LV_SIZE_CONTENT);
  lv_obj_set_style_radius(bubble, 8, 0);
  lv_obj_set_style_pad_all(bubble, 6, 0);
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
    lv_obj_set_style_bg_color(bubble, lv_color_hex(0x2A2A2A), 0);
    lv_obj_set_style_text_color(bubble, lv_color_hex(0xFFFFFF), 0);
  }

  lv_obj_t *lbl = lv_label_create(bubble);
  lv_label_set_text(lbl, msg.text.c_str());
  lv_obj_set_width(lbl, LV_PCT(100));
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl, msg.is_me ? GRID_COLOR_BG : lv_color_hex(0xFFFFFF), 0);

  // Time
  lv_obj_t *time = grid_create_label(bubble, msg.time.c_str(), &lv_font_montserrat_14);
  lv_obj_set_style_text_color(time, msg.is_me ? GRID_COLOR_BG : GRID_COLOR_DIM, 0);
  lv_obj_align(time, LV_ALIGN_BOTTOM_RIGHT, 0, 2);

  lv_obj_scroll_to_view(item, LV_ANIM_OFF);
}

void switch_to_channel(const char *name) {
  if (!name) return;
  current_target = name;

  if (name[0] == '#' || strcmp(name, "Public") == 0) {
    current_mode = MODE_CHANNEL;
    channel_unread[name] = 0;
  } else {
    current_mode = MODE_DM;
    dm_unread[name] = 0;
  }

  update_all_badges();
  switch_app(&chat_app);
}

static void send_msg_cb(lv_event_t *e) {
  const char *txt = lv_textarea_get_text(ta_input);
  if (strlen(txt) == 0) return;

  repeat_counters[current_target]++;

  Message msg = { "ME", txt, "14:58", 0, true };
  chat_history[current_target].push_back(msg);
  add_bubble(msg_list, msg);
  lv_textarea_set_text(ta_input, "");

  /* [FUTURE: MESHCORE v1.14.0]
   * discover.neighbors();
   */
}

static void back_btn_cb(lv_event_t *e) {
  switch_app(&chat_overview_app);
}

void chat_init(GridApp *app) {
  app->screen = lv_obj_create(NULL);
  lv_obj_add_style(app->screen, &style_grid_main, 0);

  // Header Area
  lv_obj_t *header = lv_obj_create(app->screen);
  lv_obj_set_size(header, LV_HOR_RES, 40);
  lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 35);
  lv_obj_set_style_bg_opa(header, 0, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_pad_all(header, 0, 0);

  lv_obj_t *back = grid_create_btn(header, LV_SYMBOL_LEFT);
  lv_obj_set_size(back, 40, 35);
  lv_obj_align(back, LV_ALIGN_LEFT_MID, 5, 0);
  lv_obj_add_event_cb(back, back_btn_cb, LV_EVENT_CLICKED, NULL);

  static auto ta_ready_cb = [](lv_event_t *ev) {
    if (lv_event_get_code(ev) == LV_EVENT_READY) {
      lv_obj_send_event((lv_obj_t *)lv_event_get_user_data(ev), LV_EVENT_CLICKED, NULL);
    }
  };

  title_label = grid_create_label(header, current_target.c_str(), &lv_font_montserrat_16);
  lv_obj_align(title_label, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_text_color(title_label, GRID_COLOR_AMBER, 0);

  // List
  msg_list = lv_obj_create(app->screen);
  lv_obj_set_size(msg_list, LV_HOR_RES, LV_VER_RES - 145);
  lv_obj_align(msg_list, LV_ALIGN_TOP_MID, 0, 80);
  lv_obj_set_flex_flow(msg_list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_bg_opa(msg_list, 0, 0);
  lv_obj_set_style_border_width(msg_list, 0, 0);
  lv_obj_set_style_pad_all(msg_list, 4, 0);
  lv_obj_set_scrollbar_mode(msg_list, LV_SCROLLBAR_MODE_AUTO);

  for (auto &m : chat_history[current_target]) {
    add_bubble(msg_list, m);
  }

  // Footer
  lv_obj_t *footer = lv_obj_create(app->screen);
  lv_obj_set_size(footer, LV_HOR_RES, 65);
  lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_color(footer, lv_color_hex(0x000000), 0);
  lv_obj_set_style_pad_all(footer, 5, 0);

  ta_input = grid_create_textarea(footer, "Bericht...");
  lv_obj_set_size(ta_input, 170, 45);
  lv_obj_align(ta_input, LV_ALIGN_LEFT_MID, 5, 0);
  lv_obj_set_style_text_font(ta_input, &lv_font_montserrat_16, 0);

  lv_obj_t *send = grid_create_btn(footer, LV_SYMBOL_PLAY);
  lv_obj_set_size(send, 55, 45);
  lv_obj_align(send, LV_ALIGN_RIGHT_MID, -5, 0);
  lv_obj_add_event_cb(send, send_msg_cb, LV_EVENT_CLICKED, NULL);

  // Connect Keyboard Send
  lv_obj_add_event_cb(ta_input, ta_ready_cb, LV_EVENT_READY, send);
}

void chat_update(GridApp *app) {
  if (lv_screen_active() != app->screen) return;
}

GridApp chat_app = { "Conversation", "💬", NULL, chat_init, chat_update, NULL };
