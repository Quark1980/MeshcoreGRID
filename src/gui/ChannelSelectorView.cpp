#include "ChannelSelectorView.h"

#include "../../examples/companion_radio/ui-new/UITask.h"
#include "../../examples/companion_radio/ui-new/UI_Bridge.h"

#include <stdio.h>
#include <string.h>

ChannelSelectorView::ChannelSelectorView(UITask *task)
    : _task(task), _current_tab(TAB_CHANNELS), _scroll_offset(0), _item_count(0) {}

int ChannelSelectorView::render(DisplayDriver &display) {
  display.setColor(DisplayDriver::DARK);
  display.fillRect(0, 0, display.width(), display.height());

  // Update item count according to tab
  if (_current_tab == TAB_CHANNELS) {
    _item_count = UI_Bridge::getInstance().getChannelCount();
  } else {
    _item_count = UI_Bridge::getInstance().getContactCount();
  }

  drawTabs(display);
  drawList(display);

  return 100; // Refresh every 100ms
}

void ChannelSelectorView::drawTabs(DisplayDriver &display) {
  int tab_h = 32;
  int start_x = 74;
  int tab_w = (display.width() - start_x - 4) / 2;

  // Back button (Matching dashboard style)
  display.setColor(DisplayDriver::DARK_GREY);
  display.fillRoundRect(2, 2, 68, 28, 6);
  display.setColor(DisplayDriver::NEON_CYAN);
  display.drawRoundRect(2, 2, 68, 28, 6);
  display.setTextSize(2);
  display.drawTextCentered(36, 16 - 7, "<");
  display.setTextSize(1);

  // Channels Tab
  display.setColor(_current_tab == TAB_CHANNELS ? DisplayDriver::NEON_CYAN : DisplayDriver::CHARCOAL);
  display.fillRoundRect(start_x, 2, tab_w - 2, tab_h, 6);
  display.setColor(_current_tab == TAB_CHANNELS ? DisplayDriver::LIGHT : DisplayDriver::GREY);
  display.drawTextCentered(start_x + (tab_w - 2) / 2, 14, "CHANNELS");

  // Contacts Tab
  int nodes_x = start_x + tab_w;
  display.setColor(_current_tab == TAB_CONTACTS ? DisplayDriver::NEON_CYAN : DisplayDriver::CHARCOAL);
  display.fillRoundRect(nodes_x, 2, tab_w - 2, tab_h, 6);
  display.setColor(_current_tab == TAB_CONTACTS ? DisplayDriver::LIGHT : DisplayDriver::GREY);
  display.drawTextCentered(nodes_x + (tab_w - 2) / 2, 14, "NODES");
}

void ChannelSelectorView::drawList(DisplayDriver &display) {
  int start_y = 50;
  int row_h = 30;
  int max_visible = (display.height() - start_y) / row_h;
  int sw = display.width();
  int btn_w = 60;

  for (int i = 0; i < max_visible; i++) {
    int idx = _scroll_offset + i;
    if (idx >= _item_count) break;

    drawItem(display, idx, start_y + i * row_h, _current_tab == TAB_CHANNELS);
  }

  // Draw scroll buttons
  display.setColor(DisplayDriver::CHARCOAL);
  display.fillRoundRect(sw - btn_w + 5, start_y + 5, btn_w - 10, 60, 8);           // Up
  display.fillRoundRect(sw - btn_w + 5, display.height() - 65, btn_w - 10, 60, 8); // Down
  display.setColor(DisplayDriver::LIGHT);
  display.drawTextCentered(sw - (btn_w / 2), start_y + 35, "^");
  display.drawTextCentered(sw - (btn_w / 2), display.height() - 35, "v");
}

void ChannelSelectorView::drawItem(DisplayDriver &display, int index, int y, bool is_group) {
  char label[64];
  bool unread = UI_Bridge::getInstance().isChannelUnread(index, is_group);

  if (is_group) {
    ChannelDetails ch;
    if (UI_Bridge::getInstance().getChannel(index, ch)) {
      strncpy(label, ch.name, sizeof(label));
    } else {
      snprintf(label, sizeof(label), "Channel %d", index);
    }
  } else {
    ContactInfo ci;
    if (UI_Bridge::getInstance().getContactByIdx(index, ci)) {
      strncpy(label, ci.name, sizeof(label));
    } else {
      snprintf(label, sizeof(label), "Contact %d", index);
    }
  }

  int row_w = display.width() - 70; // leave space for larger scroll buttons

  // Item Outline
  display.setColor(unread ? DisplayDriver::ORANGE : DisplayDriver::DARK_GREY);
  display.drawRect(5, y + 2, row_w, 26);

  if (unread) {
    display.setColor(DisplayDriver::ORANGE);
    display.fillRect(6, y + 3, 4, 24); // Accent bar inside outline
    display.drawTextLeftAlign(15, y + 8, label);
  } else {
    display.setColor(DisplayDriver::LIGHT);
    display.drawTextLeftAlign(15, y + 8, label);
  }
}

bool ChannelSelectorView::handleTouch(int x, int y) {
  int tab_h = 40;
  int sw = 320; // Assume 320 for now, should ideally use a stored width

  // Check Back switch (Matching new 68x28 button area)
  if (x < 70 && y < 32) {
    _task->gotoHomeScreen(true);
    return true;
  }

  // Check Tab switch
  if (y < 40) {
    int start_x = 74;
    int tab_w = (sw - start_x - 4) / 2;
    if (x < start_x + tab_w) {
      _current_tab = TAB_CHANNELS;
    } else {
      _current_tab = TAB_CONTACTS;
    }
    _scroll_offset = 0;
    return true;
  }

  // Check Scroll Buttons
  int btn_w = 60;
  if (x > sw - btn_w) {
    if (y > 50 && y < 120) { // Up (enlarged hit area)
      if (_scroll_offset > 0) _scroll_offset--;
      return true;
    }
    if (y > 170) { // Down (enlarged hit area)
      if (_scroll_offset < _item_count - 1) _scroll_offset++;
      return true;
    }
  }

  // Check List Item
  int start_y = 50;
  int row_h = 30;
  if (y >= start_y && x < sw - btn_w) {
    int list_idx = (y - start_y) / row_h;
    int idx = _scroll_offset + list_idx;
    if (idx < _item_count) {
      // Switch to this channel/contact
      _task->selectChannel(idx, _current_tab == TAB_CHANNELS);
      return true;
    }
  }

  return false;
}
