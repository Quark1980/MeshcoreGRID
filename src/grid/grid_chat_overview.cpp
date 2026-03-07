#include "grid_chat_overview.h"

#include "grid_chat.h"
#include "grid_ui_common.h"

#include <Arduino.h>
#include <string>
#include <vector>


static lv_obj_t *tv_main = NULL;
static lv_obj_t *hashtag_list = NULL;
static lv_obj_t *dm_list = NULL;

extern std::vector<std::string> hashtags;
extern std::vector<std::string> contacts;

static void list_item_event_cb(lv_event_t *e) {
  const char *target = (const char *)lv_event_get_user_data(e);
  switch_to_channel(target);
}

static void refresh_lists() {
  if (!hashtag_list || !dm_list) return;

  lv_obj_clean(hashtag_list);
  for (const auto &tag : hashtags) {
    lv_obj_t *btn = grid_create_btn(hashtag_list, tag.c_str());
    lv_obj_set_width(btn, LV_PCT(100));
    lv_obj_set_height(btn, 50);
    lv_obj_add_event_cb(btn, list_item_event_cb, LV_EVENT_CLICKED, (void *)tag.c_str());
    grid_create_badge(btn, channel_unread[tag]);
  }

  lv_obj_clean(dm_list);
  for (const auto &name : contacts) {
    lv_obj_t *btn = grid_create_btn(dm_list, name.c_str());
    lv_obj_set_width(btn, LV_PCT(100));
    lv_obj_set_height(btn, 50);
    lv_obj_add_event_cb(btn, list_item_event_cb, LV_EVENT_CLICKED, (void *)name.c_str());
    grid_create_badge(btn, dm_unread[name]);
  }
}

void chat_overview_init(GridApp *app) {
  app->screen = lv_obj_create(NULL);
  lv_obj_add_style(app->screen, &style_grid_main, 0);

  tv_main = lv_tabview_create(app->screen);
  lv_tabview_set_tab_bar_position(tv_main, LV_DIR_TOP);
  lv_obj_set_size(tv_main, LV_HOR_RES, LV_VER_RES - 35);
  lv_obj_align(tv_main, LV_ALIGN_TOP_MID, 0, 35);

  lv_obj_t *tab_bar = lv_tabview_get_tab_bar(tv_main);
  lv_obj_set_style_bg_color(tab_bar, lv_color_hex(0x000000), 0);
  lv_obj_set_style_text_color(tab_bar, GRID_COLOR_AMBER, 0);

  lv_obj_t *tab_hashtag = lv_tabview_add_tab(tv_main, "HASHTAGS");
  lv_obj_t *tab_dm = lv_tabview_add_tab(tv_main, "DMs");

  hashtag_list = lv_obj_create(tab_hashtag);
  lv_obj_set_size(hashtag_list, LV_PCT(100), LV_PCT(100));
  lv_obj_set_flex_flow(hashtag_list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_bg_opa(hashtag_list, 0, 0);
  lv_obj_set_style_border_width(hashtag_list, 0, 0);

  dm_list = lv_obj_create(tab_dm);
  lv_obj_set_size(dm_list, LV_PCT(100), LV_PCT(100));
  lv_obj_set_flex_flow(dm_list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_bg_opa(dm_list, 0, 0);
  lv_obj_set_style_border_width(dm_list, 0, 0);

  refresh_lists();
}

void chat_overview_refresh() {
  refresh_lists();
}

void chat_overview_update(GridApp *app) {}

GridApp chat_overview_app = { "Chat", "💬", NULL, chat_overview_init, chat_overview_update, NULL };
