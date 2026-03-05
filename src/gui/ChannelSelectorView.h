#pragma once

#include "../helpers/ChannelDetails.h"
#include "../helpers/ContactInfo.h"
#include "../helpers/ui/UIScreen.h"

class UITask;

class ChannelSelectorView : public UIScreen {
public:
  enum Tab { TAB_CHANNELS, TAB_CONTACTS };

  ChannelSelectorView(UITask *task);

  int render(DisplayDriver &display) override;
  bool handleTouch(int x, int y) override;

private:
  UITask *_task;
  Tab _current_tab;
  int _scroll_offset;
  int _item_count;

  void drawTabs(DisplayDriver &display);
  void drawList(DisplayDriver &display);
  void drawItem(DisplayDriver &display, int index, int y, bool is_group);
};
