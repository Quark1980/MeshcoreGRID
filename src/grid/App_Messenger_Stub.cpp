#include "WindowManager.h"

#include <algorithm>
#include <vector>

#include "Packet.h"

namespace {

class MessengerManagerApp : public MeshApp {
public:
  struct RowBinding {
    lv_obj_t* row;
    uint32_t id;
    bool isPrivate;
  };

  explicit MessengerManagerApp(MeshBridge* bridge)
      : _bridge(bridge),
        _layout(nullptr),
        _header(nullptr),
        _content(nullptr),
        _backBtn(nullptr),
        _title(nullptr),
        _tabview(nullptr),
        _channelsList(nullptr),
        _contactsList(nullptr),
        _threadList(nullptr),
        _inThread(false),
        _threadId(0),
        _threadPrivate(false),
        _contactsLoaded(false),
        _lastRefresh(0) {
    _rowBindings.reserve(96);
  }

  void release() override {
    this->~MessengerManagerApp();
    heap_caps_free(this);
  }

  void onStart(lv_obj_t* layout) override {
    _layout = layout;
    buildShell();
    buildLandingTabs();
  }

  void onLoop() override {
    (void)_lastRefresh;
  }

  void onClose() override {
    _rowBindings.clear();

    if (_bridge != nullptr) {
      _bridge->clearThreadFilter();
    }

    _layout = nullptr;
  }

  void onMessageReceived(MeshMessage msg) override {
    if (_threadList == nullptr || !_inThread) {
      return;
    }
    if (msg.packetType != PAYLOAD_TYPE_TXT_MSG && msg.packetType != PAYLOAD_TYPE_GRP_TXT) {
      return;
    }
    if (msg.threadId != _threadId || msg.isPrivate != _threadPrivate) {
      return;
    }

    appendThreadLine(msg.sender.c_str(), msg.text.c_str(), msg.timestamp);
  }

private:
  static const char* relativeAge(uint32_t ts) {
    static char buf[24];
    if (ts == 0) {
      return "unknown";
    }
    snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(ts));
    return buf;
  }

  static void onThreadSelected(lv_event_t* e) {
    auto* self = static_cast<MessengerManagerApp*>(lv_event_get_user_data(e));
    if (self == nullptr) {
      return;
    }

    lv_obj_t* row = lv_event_get_target(e);
    for (const auto& binding : self->_rowBindings) {
      if (binding.row != row) {
        continue;
      }

      if (binding.isPrivate) {
        self->MapsToThread(binding.id, true);
      } else {
        self->loadChatThread(static_cast<uint8_t>(binding.id & 0xFFu), false);
      }
      return;
    }
  }

  static void onBackToTabs(lv_event_t* e) {
    auto* self = static_cast<MessengerManagerApp*>(lv_event_get_user_data(e));
    if (self != nullptr) {
      self->backToLanding();
    }
  }

  static void onTabChanged(lv_event_t* e) {
    auto* self = static_cast<MessengerManagerApp*>(lv_event_get_user_data(e));
    if (self == nullptr || self->_tabview == nullptr) {
      return;
    }

    uint16_t tab = lv_tabview_get_tab_act(self->_tabview);
    if (tab == 1 && !self->_contactsLoaded) {
      self->refreshContactList();
      self->_contactsLoaded = true;
    }
  }

  void buildShell() {
    _header = lv_obj_create(_layout);
    lv_obj_set_size(_header, LV_PCT(100), 46);
    lv_obj_align(_header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_radius(_header, 0, 0);
    lv_obj_set_style_bg_color(_header, lv_color_hex(0x0F141B), 0);
    lv_obj_set_style_border_width(_header, 0, 0);
    lv_obj_set_style_pad_all(_header, 8, 0);

    _backBtn = lv_btn_create(_header);
    lv_obj_set_size(_backBtn, 74, 30);
    lv_obj_align(_backBtn, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_set_style_bg_color(_backBtn, lv_color_hex(0x1B2530), 0);
    lv_obj_add_event_cb(_backBtn, onBackToTabs, LV_EVENT_CLICKED, this);
    lv_obj_add_flag(_backBtn, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* backLabel = lv_label_create(_backBtn);
    lv_label_set_text(backLabel, LV_SYMBOL_LEFT " Back");
    lv_obj_center(backLabel);

    _title = lv_label_create(_header);
    lv_obj_set_style_text_font(_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(_title, lv_color_hex(0xE8EFF7), 0);
    lv_label_set_text(_title, "Messenger");
    lv_obj_align(_title, LV_ALIGN_CENTER, 0, 0);

    _content = lv_obj_create(_layout);
    lv_obj_set_size(_content, LV_PCT(100), LV_PCT(100));
    lv_obj_align(_content, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(_content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_content, 0, 0);
    lv_obj_set_style_pad_top(_content, 46, 0);
    lv_obj_set_style_pad_left(_content, 0, 0);
    lv_obj_set_style_pad_right(_content, 0, 0);
    lv_obj_set_style_pad_bottom(_content, 0, 0);
  }

  void clearContent() {
    _rowBindings.clear();
    lv_obj_clean(_content);
    _tabview = nullptr;
    _channelsList = nullptr;
    _contactsList = nullptr;
    _threadList = nullptr;
  }

  void styleTabHeader(lv_obj_t* btns) {
    lv_obj_set_style_bg_color(btns, lv_color_hex(0x0F141B), 0);
    lv_obj_set_style_bg_opa(btns, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btns, 0, 0);
    lv_obj_set_style_pad_column(btns, 18, 0);

    lv_obj_set_style_text_font(btns, &lv_font_montserrat_16, LV_PART_ITEMS);
    lv_obj_set_style_text_color(btns, lv_color_hex(0x8E9AAC), LV_PART_ITEMS);
    lv_obj_set_style_text_color(btns, lv_color_hex(0xE7F0FB), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(btns, LV_OPA_TRANSP, LV_PART_ITEMS);
    lv_obj_set_style_border_side(btns, LV_BORDER_SIDE_BOTTOM, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_color(btns, lv_color_hex(0x4AA3FF), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_width(btns, 3, LV_PART_ITEMS | LV_STATE_CHECKED);
  }

  void buildLandingTabs() {
    _inThread = false;
    if (_bridge != nullptr) {
      _bridge->clearThreadFilter();
    }

    clearContent();
    lv_obj_add_flag(_backBtn, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(_title, "Messenger");

    _tabview = lv_tabview_create(_content, LV_DIR_TOP, 44);
    lv_obj_set_size(_tabview, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(_tabview, lv_color_hex(0x101820), 0);
    lv_obj_set_style_border_width(_tabview, 0, 0);

    lv_obj_t* channelsTab = lv_tabview_add_tab(_tabview, "Channels");
    lv_obj_t* contactsTab = lv_tabview_add_tab(_tabview, "Contacts");

    lv_obj_t* btns = lv_tabview_get_tab_btns(_tabview);
    styleTabHeader(btns);

    _channelsList = lv_list_create(channelsTab);
    lv_obj_set_size(_channelsList, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(_channelsList, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_channelsList, 0, 0);

    _contactsList = lv_list_create(contactsTab);
    lv_obj_set_size(_contactsList, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(_contactsList, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_contactsList, 0, 0);

    lv_obj_add_event_cb(_tabview, onTabChanged, LV_EVENT_VALUE_CHANGED, this);
    _contactsLoaded = false;
    lv_list_add_text(_contactsList, "Open this tab to load contacts");

    refreshChannelList();
  }

  void addUnreadBadge(lv_obj_t* row, int unreadCount) {
    if (unreadCount <= 0) {
      return;
    }

    lv_obj_t* badge = lv_obj_create(row);
    lv_obj_set_size(badge, 20, 20);
    lv_obj_align(badge, LV_ALIGN_RIGHT_MID, -8, 0);
    lv_obj_set_style_radius(badge, 10, 0);
    lv_obj_set_style_bg_color(badge, lv_color_hex(0xE64A5F), 0);
    lv_obj_set_style_border_width(badge, 0, 0);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* n = lv_label_create(badge);
    uint8_t count = static_cast<uint8_t>(std::min(unreadCount, 99));
    char text[4];
    snprintf(text, sizeof(text), "%u", count);
    lv_label_set_text(n, text);
    lv_obj_set_style_text_font(n, &lv_font_montserrat_14, 0);
    lv_obj_center(n);
  }

  void refreshChannelList() {
    if (_channelsList == nullptr || _bridge == nullptr) {
      return;
    }

    lv_obj_clean(_channelsList);
    _rowBindings.clear();
    auto channels = _bridge->getChannels();

    if (channels.empty()) {
      lv_obj_t* t = lv_list_add_text(_channelsList, "No channels configured");
      lv_obj_set_style_text_color(t, lv_color_hex(0xDCE7F5), 0);
      return;
    }

    constexpr size_t kMaxRows = 64;
    size_t shown = 0;
    for (const auto& channel : channels) {
      if (shown >= kMaxRows) {
        break;
      }
      char label[48];
      snprintf(label, sizeof(label), "%s", channel.name.c_str());

      lv_obj_t* row = lv_list_add_btn(_channelsList, LV_SYMBOL_LIST, label);
      lv_obj_set_style_bg_color(row, lv_color_hex(0x1A2532), 0);
      lv_obj_set_style_border_width(row, 0, 0);
      lv_obj_set_style_pad_right(row, 34, 0);
      lv_obj_set_style_text_color(row, lv_color_hex(0xEDF4FF), 0);
      lv_obj_set_style_text_font(row, &lv_font_montserrat_16, 0);

      int unread = _bridge->getUnreadCount(channel.id, false);
      addUnreadBadge(row, unread);

      _rowBindings.push_back({row, channel.id, false});
      lv_obj_add_event_cb(row, onThreadSelected, LV_EVENT_CLICKED, this);
      shown++;
    }

    if (channels.size() > kMaxRows) {
      char more[48];
      snprintf(more, sizeof(more), "+ %u more channels", static_cast<unsigned>(channels.size() - kMaxRows));
      lv_obj_t* t = lv_list_add_text(_channelsList, more);
      lv_obj_set_style_text_color(t, lv_color_hex(0xDCE7F5), 0);
    }
  }

  void refreshContactList() {
    if (_contactsList == nullptr || _bridge == nullptr) {
      return;
    }

    lv_obj_clean(_contactsList);
    _rowBindings.clear();
    auto contacts = _bridge->getContacts();

    if (contacts.empty()) {
      lv_obj_t* t = lv_list_add_text(_contactsList, "No contacts in routing table");
      lv_obj_set_style_text_color(t, lv_color_hex(0xDCE7F5), 0);
      return;
    }

    constexpr size_t kMaxRows = 20;
    size_t shown = 0;
    for (const auto& contact : contacts) {
      if (shown >= kMaxRows) {
        break;
      }
      char label[92];
      snprintf(label, sizeof(label), "%s  ·  Last seen %s", contact.name.c_str(), relativeAge(contact.lastSeen));

      lv_obj_t* row = lv_list_add_btn(_contactsList, LV_SYMBOL_LIST, label);
      lv_obj_set_style_bg_color(row, lv_color_hex(0x1A2532), 0);
      lv_obj_set_style_border_width(row, 0, 0);
      lv_obj_set_style_text_color(row, lv_color_hex(0xEDF4FF), 0);
      lv_obj_set_style_text_font(row, &lv_font_montserrat_16, 0);

      if (contact.heardRecently) {
        lv_obj_set_style_border_color(row, lv_color_hex(0x27D468), 0);
        lv_obj_set_style_border_width(row, 1, 0);
      }

      _rowBindings.push_back({row, contact.id, true});
      lv_obj_add_event_cb(row, onThreadSelected, LV_EVENT_CLICKED, this);
      shown++;
    }

    if (contacts.size() > kMaxRows) {
      char more[48];
      snprintf(more, sizeof(more), "+ %u more contacts", static_cast<unsigned>(contacts.size() - kMaxRows));
      lv_obj_t* t = lv_list_add_text(_contactsList, more);
      lv_obj_set_style_text_color(t, lv_color_hex(0xDCE7F5), 0);
    }
  }

  void appendThreadLine(const char* sender, const char* text, uint32_t ts) {
    if (_threadList == nullptr) {
      return;
    }

    char line[170];
    snprintf(line, sizeof(line), "%s  [%s]\n%s",
             (sender && sender[0]) ? sender : "Unknown",
             relativeAge(ts),
             (text && text[0]) ? text : "(empty)");
    lv_list_add_text(_threadList, line);

    lv_obj_t* last = lv_obj_get_child(_threadList, lv_obj_get_child_cnt(_threadList) - 1);
    if (last != nullptr) {
      lv_obj_set_style_text_color(last, lv_color_hex(0xEDF4FF), 0);
      lv_obj_set_style_text_font(last, &lv_font_montserrat_16, 0);
      lv_obj_scroll_to_view(last, LV_ANIM_OFF);
    }
  }

  void backToLanding() {
    buildLandingTabs();
  }

  void loadChatThread(uint8_t channelID, bool isPrivate) {
    MapsToThread(channelID, isPrivate);
  }

  void MapsToThread(uint32_t id, bool isPrivate) {
    _inThread = true;
    _threadId = id;
    _threadPrivate = isPrivate;

    if (_bridge != nullptr) {
      _bridge->setThreadFilter(id, isPrivate);
    }

    clearContent();
    lv_obj_clear_flag(_backBtn, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(_title, isPrivate ? "Direct Chat" : "Channel Chat");

    _threadList = lv_list_create(_content);
    lv_obj_set_size(_threadList, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(_threadList, lv_color_hex(0x101820), 0);
    lv_obj_set_style_border_width(_threadList, 0, 0);

    bool addedHistory = false;
    if (_bridge != nullptr) {
      auto history = _bridge->getThreadHistory(id, isPrivate);
      for (const auto& entry : history) {
        appendThreadLine(entry.sender.c_str(), entry.text.c_str(), entry.timestamp);
        addedHistory = true;
      }
    }

    if (!addedHistory) {
      lv_obj_t* empty = lv_list_add_text(_threadList, "No messages yet");
      lv_obj_set_style_text_color(empty, lv_color_hex(0xDCE7F5), 0);
      lv_obj_set_style_text_font(empty, &lv_font_montserrat_16, 0);
    }
  }

  MeshBridge* _bridge;
  lv_obj_t* _layout;
  lv_obj_t* _header;
  lv_obj_t* _content;
  lv_obj_t* _backBtn;
  lv_obj_t* _title;

  lv_obj_t* _tabview;
  lv_obj_t* _channelsList;
  lv_obj_t* _contactsList;
  lv_obj_t* _threadList;

  bool _inThread;
  uint32_t _threadId;
  bool _threadPrivate;
  bool _contactsLoaded;
  uint32_t _lastRefresh;
  std::vector<RowBinding> _rowBindings;
};

}  // namespace

void registerMessengerStubApp(WindowManager& wm, MeshBridge& bridge) {
  wm.registerApp({
      "chat",
      "Chat",
      LV_SYMBOL_EDIT,
      [&bridge]() -> MeshApp* { return WindowManager::createInPsram<MessengerManagerApp>(&bridge); },
  });
}
