#include "WindowManager.h"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <cstring>
#include <vector>

#include "Packet.h"

namespace {

constexpr lv_coord_t kKeyboardHeight = 180;
constexpr lv_coord_t kBottomNavHeight = 52;

lv_obj_t* gKeyboard = nullptr;
lv_obj_t* gKeyboardTarget = nullptr;

void animHeightExec(void* var, int32_t value) {
  lv_obj_set_height(static_cast<lv_obj_t*>(var), value);
}

lv_color_t senderColor(const char* sender) {
  static const uint32_t palette[] = {
    0x4CC9A6, 0xF9C74F, 0xF9844A, 0x90BE6D, 0x43AA8B, 0xA1C181, 0x79C2D0
  };

  if (sender == nullptr || sender[0] == '\0') {
    return lv_color_hex(0x8EA0B4);
  }

  uint32_t hash = 2166136261u;
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(sender); *p != '\0'; ++p) {
    hash ^= static_cast<uint32_t>(*p);
    hash *= 16777619u;
  }
  return lv_color_hex(palette[hash % (sizeof(palette) / sizeof(palette[0]))]);
}

std::string normalizedChannelName(const std::string& name) {
  size_t start = 0;
  while (start < name.size() && std::isspace(static_cast<unsigned char>(name[start])) != 0) {
    start++;
  }

  size_t end = name.size();
  while (end > start && std::isspace(static_cast<unsigned char>(name[end - 1])) != 0) {
    end--;
  }

  std::string out = name.substr(start, end - start);
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return out;
}

class MessengerManagerApp : public MeshApp {
public:
  enum class ContactSortMode {
    LastSeen,
    Name,
  };

  struct RowBinding {
    lv_obj_t* row;
    uint32_t id;
    bool isPrivate;
  };

  struct PendingEchoBubble {
    uint32_t threadId;
    bool isPrivate;
    uint32_t timestamp;
    std::string text;
    uint8_t timesHeard;
    lv_obj_t* metaLabel;
  };

  explicit MessengerManagerApp(MeshBridge* bridge)
      : _bridge(bridge),
        _layout(nullptr),
        _header(nullptr),
        _content(nullptr),
        _backBtn(nullptr),
        _contactSortBtn(nullptr),
        _title(nullptr),
        _tabview(nullptr),
        _channelsList(nullptr),
        _contactsList(nullptr),
        _threadList(nullptr),
        _composer(nullptr),
        _input(nullptr),
        _sendBtn(nullptr),
        _inThread(false),
        _threadId(0),
        _threadPrivate(false),
        _contactsLoaded(false),
        _lastRefresh(0),
        _lastContactLongPressMs(0),
        _contactSortMode(ContactSortMode::LastSeen) {
    _rowBindings.reserve(96);
      _visibleContacts.reserve(64);
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

    WindowManager::instance().resetRightNavAction();
    hideKeyboard();

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

    if (msg.isLocal && tryResolveEcho(msg)) {
      return;
    }

    lv_obj_t* meta = appendThreadBubble(msg.sender.c_str(),
                                        msg.text.c_str(),
                                        msg.hopCount,
                                        msg.isLocal,
                                        msg.timesHeard);
    if (msg.isLocal) {
      _pendingEchoes.push_back({msg.threadId, msg.isPrivate, msg.timestamp, msg.text, msg.timesHeard, meta});
    }
  }

private:
  static const char* relativeAge(uint32_t ts) {
    static char buf[24];
    if (ts == 0) {
      return "unknown";
    }

    const uint32_t now = static_cast<uint32_t>(time(nullptr));
    if (now == 0 || now < ts) {
      return "unknown";
    }

    const uint32_t age = now - ts;
    if (age < 60) {
      snprintf(buf, sizeof(buf), "%lus", static_cast<unsigned long>(age));
    } else if (age < 3600) {
      snprintf(buf, sizeof(buf), "%lum", static_cast<unsigned long>(age / 60));
    } else if (age < 86400) {
      snprintf(buf, sizeof(buf), "%luh", static_cast<unsigned long>(age / 3600));
    } else {
      snprintf(buf, sizeof(buf), "%lud", static_cast<unsigned long>(age / 86400));
    }
    return buf;
  }

  static const char* formatClockTime(uint32_t ts) {
    static char buf[16];
    if (ts == 0) {
      return "--:--:--";
    }

    time_t raw = static_cast<time_t>(ts);
    struct tm t;
    if (localtime_r(&raw, &t) == nullptr) {
      return "--:--:--";
    }

    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
    return buf;
  }

  static void onContactLongPressed(lv_event_t* e) {
    auto* self = static_cast<MessengerManagerApp*>(lv_event_get_user_data(e));
    if (self == nullptr) {
      return;
    }

    lv_obj_t* row = lv_event_get_target(e);
    for (const auto& binding : self->_rowBindings) {
      if (binding.row != row || !binding.isPrivate) {
        continue;
      }

      self->_lastContactLongPressMs = millis();
      self->showContactDetails(binding.id);
      return;
    }
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
        const uint32_t now = millis();
        if (now - self->_lastContactLongPressMs < 800) {
          return;
        }
        self->MapsToThread(binding.id, true);
      } else {
        self->loadChatThread(static_cast<uint8_t>(binding.id & 0xFFu), false);
      }
      return;
    }
  }

  static void onBackToTabs(lv_event_t* e) { (void)e; }

  static void onInputFocused(lv_event_t* e) {
    auto* self = static_cast<MessengerManagerApp*>(lv_event_get_user_data(e));
    if (self != nullptr) {
      self->showKeyboardForInput(static_cast<lv_obj_t*>(lv_event_get_target(e)));
    }
  }

  static void onInputDefocused(lv_event_t* e) {
    auto* self = static_cast<MessengerManagerApp*>(lv_event_get_user_data(e));
    if (self != nullptr) {
      self->hideKeyboard();
    }
  }

  static void onInputReady(lv_event_t* e) {
    auto* self = static_cast<MessengerManagerApp*>(lv_event_get_user_data(e));
    if (self != nullptr) {
      self->sendCurrentInput();
      self->hideKeyboard();
    }
  }

  static void onSendClicked(lv_event_t* e) {
    auto* self = static_cast<MessengerManagerApp*>(lv_event_get_user_data(e));
    if (self != nullptr) {
      self->sendCurrentInput();
      self->hideKeyboard();
    }
  }

  static void onTabChanged(lv_event_t* e) {
    auto* self = static_cast<MessengerManagerApp*>(lv_event_get_user_data(e));
    if (self == nullptr || self->_tabview == nullptr) {
      return;
    }

    uint16_t tab = lv_tabview_get_tab_act(self->_tabview);
    if (tab == 1) {
      WindowManager::instance().setRightNavAction(LV_SYMBOL_SETTINGS " Sort", [self]() { self->toggleContactSort(); });
      if (!self->_contactsLoaded) {
        self->refreshContactList();
        self->_contactsLoaded = true;
      }
    } else {
      WindowManager::instance().resetRightNavAction();
    }
  }

  static bool contactNameLess(const MeshBridge::ContactSummary& a, const MeshBridge::ContactSummary& b) {
    std::string an = normalizedChannelName(a.name);
    std::string bn = normalizedChannelName(b.name);
    if (an == bn) {
      return a.id < b.id;
    }
    return an < bn;
  }

  static void onContactSortClicked(lv_event_t* e) {
    auto* self = static_cast<MessengerManagerApp*>(lv_event_get_user_data(e));
    if (self == nullptr) {
      return;
    }

    self->toggleContactSort();
  }

  void toggleContactSort() {
    if (_inThread) {
      return;
    }

    _contactSortMode = (_contactSortMode == ContactSortMode::LastSeen)
        ? ContactSortMode::Name
        : ContactSortMode::LastSeen;
    refreshContactList();
  }

  const MeshBridge::ContactSummary* findVisibleContact(uint32_t id) const {
    for (const auto& contact : _visibleContacts) {
      if (contact.id == id) {
        return &contact;
      }
    }
    return nullptr;
  }

  void showContactDetails(uint32_t contactId) {
    const MeshBridge::ContactSummary* contact = findVisibleContact(contactId);
    if (contact == nullptr || _bridge == nullptr) {
      return;
    }

    char latText[24];
    char lonText[24];
    if (contact->gpsLat == 0 && contact->gpsLon == 0) {
      snprintf(latText, sizeof(latText), "unknown");
      snprintf(lonText, sizeof(lonText), "unknown");
    } else {
      snprintf(latText, sizeof(latText), "%.6f", static_cast<double>(contact->gpsLat) / 1000000.0);
      snprintf(lonText, sizeof(lonText), "%.6f", static_cast<double>(contact->gpsLon) / 1000000.0);
    }

    char details[900];
    snprintf(details,
             sizeof(details),
             "Name: %s\nAddress: 0x%08lX\nPublic key: %s\nLast heard: %s ago\nLast seen (local epoch): %lu\nAdvert timestamp (remote epoch): %lu\nPosition: lat %s, lon %s\nType: %u\nFlags: 0x%02X\nOut path len: %u\nSync since: %lu\nRecently heard: %s\nUnread: %d",
             contact->name.c_str(),
             static_cast<unsigned long>(contact->id),
             contact->publicKeyHex.empty() ? "unknown" : contact->publicKeyHex.c_str(),
             relativeAge(contact->lastSeen),
             static_cast<unsigned long>(contact->lastSeen),
             static_cast<unsigned long>(contact->lastAdvertTimestamp),
             latText,
             lonText,
             static_cast<unsigned>(contact->type),
             static_cast<unsigned>(contact->flags),
             static_cast<unsigned>(contact->outPathLen),
             static_cast<unsigned long>(contact->syncSince),
             contact->heardRecently ? "yes" : "no",
             _bridge->getUnreadCount(contact->id, true));

    static const char* buttons[] = {"Close", ""};
    lv_obj_t* msg = lv_msgbox_create(nullptr, "Contact details", details, buttons, true);
    lv_obj_set_width(msg, LV_PCT(88));
    lv_obj_center(msg);
    lv_obj_t* btns = lv_msgbox_get_btns(msg);
    if (btns != nullptr) {
      lv_obj_add_event_cb(btns, [](lv_event_t* e) {
        lv_obj_t* mb = static_cast<lv_obj_t*>(lv_event_get_user_data(e));
        lv_msgbox_close(mb);
      }, LV_EVENT_VALUE_CHANGED, msg);
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
    lv_obj_set_style_bg_color(_backBtn, lv_color_hex(0xFFB300), LV_STATE_PRESSED);
    lv_obj_add_event_cb(_backBtn, onBackToTabs, LV_EVENT_CLICKED, this);
    lv_obj_add_flag(_backBtn, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* backLabel = lv_label_create(_backBtn);
    lv_label_set_text(backLabel, LV_SYMBOL_LEFT " Back");
    lv_obj_center(backLabel);

    _title = lv_label_create(_header);
    lv_obj_set_style_text_font(_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(_title, lv_color_hex(0xE8EFF7), 0);
    lv_label_set_text(_title, "Messenger");
    lv_obj_align(_title, LV_ALIGN_CENTER, 0, 0);

    _content = lv_obj_create(_layout);
    lv_obj_set_size(_content, LV_PCT(100), LV_PCT(100));
    lv_obj_align(_content, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(_content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_content, 0, 0);
    lv_obj_set_style_pad_top(_content, 42, 0);
    lv_obj_set_style_pad_left(_content, 0, 0);
    lv_obj_set_style_pad_right(_content, 0, 0);
    lv_obj_set_style_pad_bottom(_content, 0, 0);
    lv_obj_clear_flag(_content, LV_OBJ_FLAG_SCROLLABLE);

    // Keep header controls touchable above tab/content layers.
    lv_obj_move_foreground(_header);
  }

  void clearContent() {
    _rowBindings.clear();
    _pendingEchoes.clear();
    lv_obj_clean(_content);
    _tabview = nullptr;
    _channelsList = nullptr;
    _contactsList = nullptr;
    _threadList = nullptr;
    _composer = nullptr;
    _input = nullptr;
    _sendBtn = nullptr;
  }

  void ensureGlobalKeyboard() {
    if (gKeyboard != nullptr || _layout == nullptr) {
      return;
    }

    gKeyboard = lv_keyboard_create(lv_layer_top());
    lv_obj_set_size(gKeyboard, LV_PCT(100), kKeyboardHeight);
    lv_obj_align(gKeyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(gKeyboard, LV_OBJ_FLAG_HIDDEN);
    lv_keyboard_set_mode(gKeyboard, LV_KEYBOARD_MODE_TEXT_LOWER);

    lv_obj_set_style_bg_color(gKeyboard, lv_color_hex(0x101820), 0);
    lv_obj_set_style_bg_opa(gKeyboard, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(gKeyboard, 0, 0);
    lv_obj_set_style_bg_color(gKeyboard, lv_color_hex(0x1B2530), LV_PART_ITEMS);
    lv_obj_set_style_text_color(gKeyboard, lv_color_hex(0xE7F0FB), LV_PART_ITEMS);
    lv_obj_set_style_radius(gKeyboard, 6, LV_PART_ITEMS);
  }

  void animateThreadListHeight(int32_t targetHeight) {
    if (_threadList == nullptr) {
      return;
    }

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, _threadList);
    lv_anim_set_values(&a, lv_obj_get_height(_threadList), targetHeight);
    lv_anim_set_time(&a, 180);
    lv_anim_set_exec_cb(&a, animHeightExec);
    lv_anim_start(&a);
  }

  int32_t keyboardOverlapInContent() const {
    if (_content == nullptr || gKeyboard == nullptr || lv_obj_has_flag(gKeyboard, LV_OBJ_FLAG_HIDDEN)) {
      return 0;
    }

    lv_area_t contentArea;
    lv_area_t keyboardArea;
    lv_obj_get_coords(_content, &contentArea);
    lv_obj_get_coords(gKeyboard, &keyboardArea);

    const int32_t top = std::max(contentArea.y1, keyboardArea.y1);
    const int32_t bottom = std::min(contentArea.y2, keyboardArea.y2);
    if (bottom < top) {
      return 0;
    }
    return bottom - top + 1;
  }

  int32_t navOverlapInContent() const {
    if (_content == nullptr) {
      return 0;
    }

    lv_area_t contentArea;
    lv_obj_get_coords(_content, &contentArea);

    const int32_t contentTop = static_cast<int32_t>(contentArea.y1);
    const int32_t contentBottom = static_cast<int32_t>(contentArea.y2);
    const int32_t navTop = LV_VER_RES - kBottomNavHeight;
    const int32_t navBottom = LV_VER_RES - 1;
    const int32_t top = std::max(contentTop, navTop);
    const int32_t bottom = std::min(contentBottom, navBottom);
    if (bottom < top) {
      return 0;
    }
    return bottom - top + 1;
  }

  int32_t bottomObstructionInContent() const {
    return std::max(navOverlapInContent(), keyboardOverlapInContent());
  }

  void showKeyboardForInput(lv_obj_t* input) {
    if (_content == nullptr || _threadList == nullptr || input == nullptr) {
      return;
    }

    ensureGlobalKeyboard();
    if (gKeyboard == nullptr) {
      return;
    }

    gKeyboardTarget = input;
    lv_keyboard_set_textarea(gKeyboard, input);
    lv_obj_clear_flag(gKeyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(gKeyboard, LV_ALIGN_BOTTOM_MID, 0, 0);

    // On the first show, keyboard coordinates may be stale until a layout pass.
    lv_obj_update_layout(lv_layer_top());
    lv_obj_update_layout(_content);

    const int32_t obstruction = bottomObstructionInContent();

    if (_composer != nullptr) {
      lv_obj_move_foreground(_composer);
      lv_obj_align(_composer, LV_ALIGN_BOTTOM_MID, 0, -obstruction);
      lv_obj_update_layout(_composer);
    }

    const int32_t composerH = (_composer != nullptr) ? lv_obj_get_height(_composer) : 0;
    const int32_t target = std::max<int32_t>(56, lv_obj_get_height(_content) - composerH - obstruction);
    animateThreadListHeight(target);
  }

  void hideKeyboard() {
    if (_content == nullptr || _threadList == nullptr) {
      return;
    }

    if (gKeyboard != nullptr) {
      lv_keyboard_set_textarea(gKeyboard, nullptr);
      lv_obj_add_flag(gKeyboard, LV_OBJ_FLAG_HIDDEN);
    }
    gKeyboardTarget = nullptr;

    if (_composer != nullptr) {
      lv_obj_align(_composer, LV_ALIGN_BOTTOM_MID, 0, 0);
    }

    int32_t target = lv_obj_get_height(_content) - navOverlapInContent();
    if (_composer != nullptr && !lv_obj_has_flag(_composer, LV_OBJ_FLAG_HIDDEN)) {
      target -= lv_obj_get_height(_composer);
    }
    animateThreadListHeight(target);
  }

  void showComposer() {
    if (_composer == nullptr || _input == nullptr) {
      return;
    }

    lv_obj_clear_flag(_composer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_update_layout(_content);
    lv_obj_update_layout(_composer);
    lv_obj_move_foreground(_composer);
    lv_textarea_set_text(_input, "");
    lv_obj_add_state(_input, LV_STATE_FOCUSED);
    showKeyboardForInput(_input);
  }

  void hideComposer() {
    hideKeyboard();
    if (_composer != nullptr) {
      lv_obj_add_flag(_composer, LV_OBJ_FLAG_HIDDEN);
    }
    if (_threadList != nullptr && _content != nullptr) {
      animateThreadListHeight(lv_obj_get_height(_content) - navOverlapInContent());
    }
  }

  void styleTabHeader(lv_obj_t* btns) {
    lv_obj_set_style_bg_color(btns, lv_color_hex(0x0F141B), 0);
    lv_obj_set_style_bg_opa(btns, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btns, 0, 0);
    lv_obj_set_style_pad_column(btns, 18, 0);

    lv_obj_set_style_text_font(btns, &lv_font_montserrat_14, LV_PART_ITEMS);
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

    WindowManager::instance().resetRightNavAction();

    clearContent();
    lv_obj_add_flag(_backBtn, LV_OBJ_FLAG_HIDDEN);
    WindowManager::instance().resetRightNavAction();
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
    size_t skipped = 0;
    for (const auto& channel : channels) {
      if (shown >= kMaxRows) {
        break;
      }
      const std::string normalizedName = normalizedChannelName(channel.name);
      if (normalizedName.empty() || normalizedName == "channel") {
        skipped++;
        continue;
      }
      char label[48];
      snprintf(label, sizeof(label), "%s", channel.name.c_str());

      lv_obj_t* row = lv_list_add_btn(_channelsList, LV_SYMBOL_LIST, label);
      lv_obj_set_style_bg_color(row, lv_color_hex(0x1A2532), 0);
      lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
      lv_obj_set_style_radius(row, 12, 0);
      lv_obj_set_style_border_width(row, 1, 0);
      lv_obj_set_style_border_color(row, lv_color_hex(0x263040), 0);
      lv_obj_set_style_pad_left(row, 12, 0);
      lv_obj_set_style_pad_right(row, 34, 0);
      lv_obj_set_style_pad_top(row, 10, 0);
      lv_obj_set_style_pad_bottom(row, 10, 0);
      lv_obj_set_style_text_color(row, lv_color_hex(0xEDF4FF), 0);
      lv_obj_set_style_text_font(row, &lv_font_montserrat_14, 0);

      int unread = _bridge->getUnreadCount(channel.id, false);
      addUnreadBadge(row, unread);

      _rowBindings.push_back({row, channel.id, false});
      lv_obj_add_event_cb(row, onThreadSelected, LV_EVENT_CLICKED, this);
      shown++;
    }

    if (shown == 0) {
      lv_obj_t* t = lv_list_add_text(_channelsList, "No populated channels");
      lv_obj_set_style_text_color(t, lv_color_hex(0xDCE7F5), 0);
    } else if (channels.size() - skipped > kMaxRows) {
      char more[48];
      snprintf(more, sizeof(more), "+ %u more channels", static_cast<unsigned>(channels.size() - skipped - kMaxRows));
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
    _visibleContacts.clear();
    auto contacts = _bridge->getContacts();

    if (_contactSortMode == ContactSortMode::Name) {
      std::sort(contacts.begin(), contacts.end(), contactNameLess);
    } else {
      std::sort(contacts.begin(), contacts.end(), [](const MeshBridge::ContactSummary& a, const MeshBridge::ContactSummary& b) {
        if (a.lastSeen == b.lastSeen) {
          return contactNameLess(a, b);
        }
        return a.lastSeen > b.lastSeen;
      });
    }

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
      // Use a plain container instead of lv_list_add_btn so no inherited
      // lv_btn / list-button theme styles fight with our flex layout.
      lv_obj_t* row = lv_obj_create(_contactsList);
      lv_obj_remove_style_all(row);
      lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_add_flag(row, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
      lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
      lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
      lv_obj_set_style_bg_color(row, lv_color_hex(0x1A2532), 0);
      lv_obj_set_style_bg_color(row, lv_color_hex(0x253447), LV_STATE_PRESSED);
      lv_obj_set_style_pad_left(row, 12, 0);
      lv_obj_set_style_pad_right(row, 12, 0);
      lv_obj_set_style_pad_top(row, 10, 0);
      lv_obj_set_style_pad_bottom(row, 10, 0);
      lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
      lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

      lv_obj_t* nameLabel = lv_label_create(row);
      lv_label_set_text(nameLabel, contact.name.c_str());
      lv_label_set_long_mode(nameLabel, LV_LABEL_LONG_CLIP);
      lv_obj_set_flex_grow(nameLabel, 1);
      lv_obj_set_style_text_align(nameLabel, LV_TEXT_ALIGN_LEFT, 0);
      lv_obj_set_style_text_color(nameLabel, lv_color_hex(0xEDF4FF), 0);
      lv_obj_set_style_text_font(nameLabel, &lv_font_montserrat_14, 0);

      lv_obj_t* timeLabel = lv_label_create(row);
      lv_label_set_text(timeLabel, formatClockTime(contact.lastSeen));
      lv_obj_set_width(timeLabel, 72);
      lv_obj_set_style_text_align(timeLabel, LV_TEXT_ALIGN_RIGHT, 0);
      lv_obj_set_style_text_color(timeLabel, lv_color_hex(0xAFC2D8), 0);
      lv_obj_set_style_text_font(timeLabel, &lv_font_montserrat_14, 0);

      if (contact.heardRecently) {
        lv_obj_set_style_border_color(row, lv_color_hex(0x27D468), 0);
        lv_obj_set_style_border_width(row, 1, 0);
      }

      _rowBindings.push_back({row, contact.id, true});
      _visibleContacts.push_back(contact);
      lv_obj_add_event_cb(row, onThreadSelected, LV_EVENT_CLICKED, this);
      lv_obj_add_event_cb(row, onContactLongPressed, LV_EVENT_LONG_PRESSED, this);
      shown++;
    }

    if (contacts.size() > kMaxRows) {
      char more[48];
      snprintf(more, sizeof(more), "+ %u more contacts", static_cast<unsigned>(contacts.size() - kMaxRows));
      lv_obj_t* t = lv_list_add_text(_contactsList, more);
      lv_obj_set_style_text_color(t, lv_color_hex(0xDCE7F5), 0);
    }
  }

  lv_obj_t* appendThreadBubble(const char* sender,
                              const char* text,
                              uint8_t hopCount,
                              bool isMe,
                              uint8_t timesHeard = 0) {
    if (_threadList == nullptr) {
      return nullptr;
    }

    lv_obj_t* row = lv_obj_create(_threadList);
    lv_obj_remove_style_all(row);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(row,
                          isMe ? LV_FLEX_ALIGN_END : LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_top(row, 4, 0);
    lv_obj_set_style_pad_bottom(row, 4, 0);
    lv_obj_set_style_pad_left(row, 6, 0);
    lv_obj_set_style_pad_right(row, 6, 0);

    if (!isMe) {
      lv_obj_t* senderLbl = lv_label_create(row);
      lv_label_set_text(senderLbl, (sender && sender[0]) ? sender : "Unknown");
      lv_obj_set_style_text_color(senderLbl, senderColor(sender), 0);
      lv_obj_set_style_text_font(senderLbl, &lv_font_montserrat_12, 0);
      lv_obj_set_style_pad_left(senderLbl, 2, 0);
      lv_obj_set_style_pad_bottom(senderLbl, 2, 0);
    }

    lv_obj_t* bubble = lv_obj_create(row);
    lv_obj_remove_style_all(bubble);
    lv_obj_clear_flag(bubble, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(bubble, LV_PCT(86));
    lv_obj_set_height(bubble, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(bubble, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(bubble, 2, 0);
    lv_obj_set_style_radius(bubble, 14, 0);
    lv_obj_set_style_border_width(bubble, 0, 0);
    lv_obj_set_style_pad_left(bubble, 10, 0);
    lv_obj_set_style_pad_right(bubble, 10, 0);
    lv_obj_set_style_pad_top(bubble, 7, 0);
    lv_obj_set_style_pad_bottom(bubble, 7, 0);
    lv_obj_set_style_bg_opa(bubble, LV_OPA_COVER, 0);

    if (isMe) {
      lv_obj_set_style_bg_color(bubble, lv_color_hex(0x2F6DF6), 0);
    } else {
      lv_obj_set_style_bg_color(bubble, lv_color_hex(0xE7EDF6), 0);
    }

    lv_obj_t* body = lv_label_create(bubble);
    lv_label_set_text(body, (text && text[0]) ? text : "(empty)");
    lv_obj_set_width(body, LV_PCT(100));
    lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(body, isMe ? lv_color_hex(0xEFF4FF) : lv_color_hex(0x0E1622), 0);
    lv_obj_set_style_text_font(body, &lv_font_montserrat_12, 0);

    lv_obj_t* meta = nullptr;
    if (isMe) {
      // Mesh confirmation for own sent messages: show Times Heard (repeater echoes)
      meta = lv_label_create(row);
      char metaText[48];
      if (timesHeard > 0) {
        snprintf(metaText, sizeof(metaText), "👂 %u", static_cast<unsigned>(timesHeard));
        lv_obj_set_style_text_color(meta, lv_color_hex(0x00D084), 0);  // Green = confirmed by mesh
      } else {
        snprintf(metaText, sizeof(metaText), "👂 0");
        lv_obj_set_style_text_color(meta, lv_color_hex(0x9BA3AF), 0);  // Grey = waiting
      }
      lv_label_set_text(meta, metaText);
      lv_obj_set_style_text_font(meta, &lv_font_montserrat_10, 0);
      lv_obj_set_style_pad_top(meta, 2, 0);
      lv_obj_set_style_pad_left(meta, 3, 0);
    } else {
      // Mesh confirmation for received messages: show hop count
      lv_obj_t* metaHops = lv_label_create(row);
      char hopsText[32];
      if (hopCount == 0) {
        snprintf(hopsText, sizeof(hopsText), "📡 Direct");
        lv_obj_set_style_text_color(metaHops, lv_color_hex(0x00D084), 0);  // Green = zero hops
      } else {
        snprintf(hopsText, sizeof(hopsText), "📡 %u Hops", static_cast<unsigned>(hopCount));
        lv_obj_set_style_text_color(metaHops, lv_color_hex(0x6D7B8E), 0);  // Grey = via repeater
      }
      lv_label_set_text(metaHops, hopsText);
      lv_obj_set_style_text_font(metaHops, &lv_font_montserrat_10, 0);
      lv_obj_set_style_pad_top(metaHops, 2, 0);
      lv_obj_set_style_pad_left(metaHops, 2, 0);
      meta = metaHops;
    }

    lv_obj_update_layout(_threadList);
    lv_obj_scroll_to_view(row, LV_ANIM_OFF);
    return meta;
  }

  bool tryResolveEcho(const MeshMessage& msg) {
    constexpr uint32_t kEchoTimestampToleranceMs = 5000;

    for (auto it = _pendingEchoes.rbegin(); it != _pendingEchoes.rend(); ++it) {
      auto& pending = *it;
      if (pending.threadId != msg.threadId || pending.isPrivate != msg.isPrivate) {
        continue;
      }
      if (pending.text != msg.text) {
        continue;
      }

      const uint32_t tsA = pending.timestamp;
      const uint32_t tsB = msg.timestamp;
      const uint32_t diff = (tsA > tsB) ? (tsA - tsB) : (tsB - tsA);
      const bool timestampClose = (tsA == tsB) || (diff <= kEchoTimestampToleranceMs);
      const bool fallbackToLatestSameText = (it == _pendingEchoes.rbegin() && pending.timesHeard == 0);
      if (!timestampClose && !fallbackToLatestSameText) {
        continue;
      }
      if (pending.metaLabel == nullptr) {
        return true;
      }

      uint8_t nextHeard = static_cast<uint8_t>(pending.timesHeard + 1);
      if (msg.timesHeard > nextHeard) {
        nextHeard = msg.timesHeard;
      }
      pending.timesHeard = nextHeard;
      char heardText[48];
      snprintf(heardText, sizeof(heardText), "👂 %u", static_cast<unsigned>(pending.timesHeard));
      lv_label_set_text(pending.metaLabel, heardText);
      lv_obj_set_style_text_color(pending.metaLabel, lv_color_hex(0x00D084), 0);  // Green = confirmed
      return true;
    }
    return false;
  }

  void sendCurrentInput() {
    if (_bridge == nullptr || _input == nullptr) {
      return;
    }

    const char* txt = lv_textarea_get_text(_input);
    if (txt == nullptr || txt[0] == '\0') {
      return;
    }

    const uint32_t ts = static_cast<uint32_t>(millis());
    _bridge->enqueueOutboxText(_threadId, _threadPrivate, txt, ts);
    MeshMessage localMsg = _bridge->recordLocalMessage(_threadId, _threadPrivate, "You", txt, ts);
    lv_obj_t* meta = appendThreadBubble(localMsg.sender.c_str(), localMsg.text.c_str(), 0, true, localMsg.timesHeard);
    _pendingEchoes.push_back({_threadId, _threadPrivate, ts, txt, localMsg.timesHeard, meta});
    lv_textarea_set_text(_input, "");
    hideComposer();
  }

  void backToLanding() {
    hideKeyboard();
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
    lv_obj_update_layout(_content);
    lv_obj_add_flag(_backBtn, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(_title, isPrivate ? "Direct Chat" : "Channel Chat");
    WindowManager::instance().setRightNavAction(LV_SYMBOL_EDIT " Write", [this]() { showComposer(); });

    _composer = lv_obj_create(_content);
    lv_obj_set_size(_composer, LV_PCT(100), 58);
    lv_obj_align(_composer, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_radius(_composer, 0, 0);
    lv_obj_set_style_bg_color(_composer, lv_color_hex(0x0F141B), 0);
    lv_obj_set_style_border_width(_composer, 0, 0);
    lv_obj_set_style_pad_all(_composer, 8, 0);

    _threadList = lv_obj_create(_content);
    lv_obj_remove_style_all(_threadList);
    lv_obj_set_size(_threadList, LV_PCT(100), LV_PCT(100));
    lv_obj_set_height(_threadList, lv_obj_get_height(_content) - navOverlapInContent());
    lv_obj_align(_threadList, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(_threadList, lv_color_hex(0x101820), 0);
    lv_obj_set_style_bg_opa(_threadList, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_threadList, 0, 0);
    lv_obj_set_style_pad_top(_threadList, 6, 0);
    lv_obj_set_style_pad_bottom(_threadList, 10, 0);
    lv_obj_set_style_pad_left(_threadList, 0, 0);
    lv_obj_set_style_pad_right(_threadList, 0, 0);
    lv_obj_set_style_pad_row(_threadList, 2, 0);
    lv_obj_set_scrollbar_mode(_threadList, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(_threadList, LV_DIR_VER);
    lv_obj_set_flex_flow(_threadList, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_threadList, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_move_foreground(_composer);

    _input = lv_textarea_create(_composer);
    lv_obj_set_size(_input, LV_PCT(78), 46);
    lv_obj_align(_input, LV_ALIGN_LEFT_MID, 0, 0);
    lv_textarea_set_placeholder_text(_input, "Type a message");
    lv_textarea_set_one_line(_input, false);
    lv_obj_set_style_bg_color(_input, lv_color_hex(0x1B2530), 0);
    lv_obj_set_style_border_width(_input, 0, 0);
    lv_obj_set_style_text_color(_input, lv_color_hex(0xEDF4FF), 0);
    lv_obj_set_style_text_font(_input, &lv_font_montserrat_14, 0);
    lv_obj_add_event_cb(_input, onInputFocused, LV_EVENT_FOCUSED, this);
    lv_obj_add_event_cb(_input, onInputDefocused, LV_EVENT_DEFOCUSED, this);
    lv_obj_add_event_cb(_input, onInputReady, LV_EVENT_READY, this);

    _sendBtn = lv_btn_create(_composer);
    lv_obj_set_size(_sendBtn, LV_PCT(20), 46);
    lv_obj_align(_sendBtn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(_sendBtn, lv_color_hex(0x25D366), 0);
    lv_obj_set_style_border_width(_sendBtn, 0, 0);
    lv_obj_add_event_cb(_sendBtn, onSendClicked, LV_EVENT_CLICKED, this);
    lv_obj_t* sendLbl = lv_label_create(_sendBtn);
    lv_label_set_text(sendLbl, "Send");
    lv_obj_set_style_text_color(sendLbl, lv_color_hex(0x06210E), 0);
    lv_obj_set_style_text_font(sendLbl, &lv_font_montserrat_14, 0);
    lv_obj_center(sendLbl);

    // Let LVGL calculate child geometry once before the composer is hidden.
    lv_obj_update_layout(_composer);
    lv_obj_add_flag(_composer, LV_OBJ_FLAG_HIDDEN);

    bool addedHistory = false;
    if (_bridge != nullptr) {
      auto history = _bridge->getThreadHistory(id, isPrivate);
      for (const auto& entry : history) {
        lv_obj_t* meta = appendThreadBubble(entry.sender.c_str(),
                                            entry.text.c_str(),
                                            entry.hopCount,
                                            entry.isLocal,
                                            entry.timesHeard);
        if (entry.isLocal) {
          _pendingEchoes.push_back({entry.threadId,
                                    entry.isPrivate,
                                    entry.timestamp,
                                    entry.text,
                                    entry.timesHeard,
                                    meta});
        }
        addedHistory = true;
      }
    }

    if (!addedHistory) {
      lv_obj_t* empty = lv_label_create(_threadList);
      lv_label_set_text(empty, "No messages yet");
      lv_obj_set_style_text_color(empty, lv_color_hex(0xDCE7F5), 0);
      lv_obj_set_style_text_font(empty, &lv_font_montserrat_14, 0);
      lv_obj_set_style_pad_left(empty, 10, 0);
      lv_obj_set_style_pad_top(empty, 6, 0);
    }
  }

  MeshBridge* _bridge;
  lv_obj_t* _layout;
  lv_obj_t* _header;
  lv_obj_t* _content;
  lv_obj_t* _backBtn;
  lv_obj_t* _contactSortBtn;
  lv_obj_t* _title;

  lv_obj_t* _tabview;
  lv_obj_t* _channelsList;
  lv_obj_t* _contactsList;
  lv_obj_t* _threadList;
  lv_obj_t* _composer;
  lv_obj_t* _input;
  lv_obj_t* _sendBtn;

  bool _inThread;
  uint32_t _threadId;
  bool _threadPrivate;
  bool _contactsLoaded;
  uint32_t _lastRefresh;
  uint32_t _lastContactLongPressMs;
  ContactSortMode _contactSortMode;
  std::vector<RowBinding> _rowBindings;
  std::vector<MeshBridge::ContactSummary> _visibleContacts;
  std::vector<PendingEchoBubble> _pendingEchoes;
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
