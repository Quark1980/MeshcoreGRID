#include "grid_chat_overview.h"

#include "grid_chat.h"
#include "grid_ui_common.h"

#include <Arduino.h>
#include <set>
#include <string>
#include <vector>

static lv_obj_t *tv_main = NULL;
static lv_obj_t *hashtag_list = NULL;
static lv_obj_t *dm_list = NULL;
static lv_obj_t *dm_filter_dd = NULL;

extern std::vector<std::string> hashtags;
extern std::vector<std::string> contacts;
static std::set<std::string> favorites;

static void list_item_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  const char *target = (const char *)lv_event_get_user_data(e);

  if (code == LV_EVENT_CLICKED) {
    switch_to_channel(target);
  } else if (code == LV_EVENT_LONG_PRESSED) {
    // Popup for DM items
    if (target[0] != '#') {
      grid_create_popup(
          "Contact Opties", target, "Favoriet",
          [](lv_event_t *ev) {
            lv_obj_t *mbox = (lv_obj_t *)lv_event_get_user_data(ev);
            lv_msgbox_close(mbox);
            Serial.println("GRID: Markeer als favoriet");
          },
          "Sluiten", [](lv_event_t *ev) { lv_msgbox_close((lv_obj_t *)lv_event_get_user_data(ev)); });
    }
  }
}

static void add_hashtag_cb(lv_event_t *e) {
  grid_create_popup(
      "Nieuw Kanaal", "Hier komt tekstveld voor #hashtag", "OK",
      [](lv_event_t *ev) { lv_msgbox_close((lv_obj_t *)lv_event_get_user_data(ev)); }, "Annuleren",
      [](lv_event_t *ev) { lv_msgbox_close((lv_obj_t *)lv_event_get_user_data(ev)); });
}

static void filter_event_cb(lv_event_t *e) {
  Serial.println("GRID: DM Filter changed");
}

static void refresh_lists() {
  if (!hashtag_list || !dm_list) return;

  // 1. Channels List
  lv_obj_clean(hashtag_list);

  const char *public_ch = "Public";
  lv_obj_t *btn_pub = grid_create_btn(hashtag_list, public_ch);
  lv_obj_set_width(btn_pub, LV_PCT(100));
  lv_obj_set_height(btn_pub, 35);
  lv_obj_add_event_cb(btn_pub, list_item_event_cb, LV_EVENT_CLICKED, (void *)public_ch);
  grid_create_badge(btn_pub, channel_unread[public_ch]);

  for (const auto &tag : hashtags) {
    lv_obj_t *btn = grid_create_btn(hashtag_list, tag.c_str());
    lv_obj_set_width(btn, LV_PCT(100));
    lv_obj_set_height(btn, 35);
    lv_obj_add_event_cb(btn, list_item_event_cb, LV_EVENT_CLICKED, (void *)tag.c_str());
    grid_create_badge(btn, channel_unread[tag]);
  }

  // 2. DM List
  lv_obj_clean(dm_list);
  for (const auto &name : contacts) {
    lv_obj_t *btn = grid_create_btn(dm_list, name.c_str());
    lv_obj_set_width(btn, LV_PCT(100));
    lv_obj_set_height(btn, 35);
    lv_obj_add_event_cb(btn, list_item_event_cb, LV_EVENT_ALL, (void *)name.c_str()); // Need LONG_PRESS too
    grid_create_badge(btn, dm_unread[name]);
  }
}

void chat_overview_init(GridApp *app) {
  app->screen = lv_obj_create(NULL);
  lv_obj_add_style(app->screen, &style_grid_main, 0);

  // Header Area
  lv_obj_t *header = lv_obj_create(app->screen);
  lv_obj_set_size(header, LV_HOR_RES, 45);
  lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 35);
  lv_obj_set_style_bg_opa(header, 0, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_pad_all(header, 0, 0);

  lv_obj_t *title = grid_create_label(header, "Chat Overview", &lv_font_montserrat_16);
  lv_obj_align(title, LV_ALIGN_CENTER, 0, -2);
  lv_obj_set_style_text_color(title, GRID_COLOR_AMBER, 0);

  // Tabview
  tv_main = lv_tabview_create(app->screen);
  lv_tabview_set_tab_bar_position(tv_main, LV_DIR_TOP);
  lv_obj_set_size(tv_main, LV_HOR_RES, LV_VER_RES - 80);
  lv_obj_align(tv_main, LV_ALIGN_TOP_MID, 0, 80);

  lv_obj_t *tab_bar = lv_tabview_get_tab_bar(tv_main);
  lv_obj_set_style_bg_color(tab_bar, lv_color_hex(0x000000), 0);
  lv_obj_set_style_text_color(tab_bar, GRID_COLOR_AMBER, 0);
  lv_obj_set_style_text_font(tab_bar, &lv_font_montserrat_14, 0);

  lv_obj_t *tab_hashtag = lv_tabview_add_tab(tv_main, "Channels");
  lv_obj_t *tab_dm = lv_tabview_add_tab(tv_main, "DMs / Nodes");

  // --- Channels Tab ---
  // Add Channel Button in the tab header area (actually just float it over the list)
  lv_obj_t *add_ch = grid_create_btn(tab_hashtag, LV_SYMBOL_PLUS);
  lv_obj_set_size(add_ch, 35, 35);
  lv_obj_align(add_ch, LV_ALIGN_TOP_RIGHT, -5, 5);
  lv_obj_set_style_radius(add_ch, LV_RADIUS_CIRCLE, 0);
  lv_obj_add_event_cb(add_ch, add_hashtag_cb, LV_EVENT_CLICKED, NULL);

  hashtag_list = lv_obj_create(tab_hashtag);
  lv_obj_set_size(hashtag_list, LV_PCT(100), LV_PCT(85));
  lv_obj_align(hashtag_list, LV_ALIGN_TOP_MID, 0, 45);
  lv_obj_set_flex_flow(hashtag_list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_bg_opa(hashtag_list, 0, 0);
  lv_obj_set_style_border_width(hashtag_list, 0, 0);
  lv_obj_set_style_pad_all(hashtag_list, 4, 0);
  lv_obj_set_style_pad_row(hashtag_list, 4, 0);
  lv_obj_set_scrollbar_mode(hashtag_list, LV_SCROLLBAR_MODE_AUTO);

  // --- DMs Tab ---
  dm_filter_dd = lv_dropdown_create(tab_dm);
  lv_dropdown_set_options(dm_filter_dd, "Alle\nFavorieten\nAlleen repeaters\nLast heard");
  lv_obj_set_width(dm_filter_dd, LV_PCT(95));
  lv_obj_align(dm_filter_dd, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_add_style(dm_filter_dd, &style_grid_btn, 0);
  lv_obj_set_style_text_font(dm_filter_dd, &lv_font_montserrat_14, 0);
  lv_obj_add_event_cb(dm_filter_dd, filter_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

  dm_list = lv_obj_create(tab_dm);
  lv_obj_set_size(dm_list, LV_PCT(100), LV_PCT(80));
  lv_obj_align(dm_list, LV_ALIGN_TOP_MID, 0, 40);
  lv_obj_set_flex_flow(dm_list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_bg_opa(dm_list, 0, 0);
  lv_obj_set_style_border_width(dm_list, 0, 0);
  lv_obj_set_style_pad_all(dm_list, 4, 0);
  lv_obj_set_style_pad_row(dm_list, 4, 0);
  lv_obj_set_scrollbar_mode(dm_list, LV_SCROLLBAR_MODE_AUTO);

  refresh_lists();
}

void chat_overview_refresh() {
  refresh_lists();
}

void chat_overview_update(GridApp *app) {}

GridApp chat_overview_app = { "Chat Overview", "💬", NULL, chat_overview_init, chat_overview_update, NULL };
