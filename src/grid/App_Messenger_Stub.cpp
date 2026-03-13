#include "WindowManager.h"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <cstring>
#include <vector>

#include "MyMesh.h"
#include "Packet.h"

namespace {

constexpr lv_coord_t kKeyboardHeight = 180;
constexpr lv_coord_t kBottomNavHeight = 52;

lv_obj_t* gKeyboard = nullptr;
lv_obj_t* gKeyboardTarget = nullptr;

static const char* kKeyboardMapLower[] = {
  "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "\n",
  "a", "s", "d", "f", "g", "h", "j", "k", "l", "\n",
  "ABC", "z", "x", "c", "v", "b", "n", "m", LV_SYMBOL_BACKSPACE, "\n",
  "1#", LV_SYMBOL_LEFT, " ", LV_SYMBOL_RIGHT, LV_SYMBOL_OK, ""
};

static const lv_btnmatrix_ctrl_t kKeyboardCtrlLower[] = {
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1,
  2, 1, 1, 1, 1, 1, 1, 1, 2,
  2, 1, 4, 1, 2
};

static const char* kKeyboardMapUpper[] = {
  "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "\n",
  "A", "S", "D", "F", "G", "H", "J", "K", "L", "\n",
  "abc", "Z", "X", "C", "V", "B", "N", "M", LV_SYMBOL_BACKSPACE, "\n",
  "1#", LV_SYMBOL_LEFT, " ", LV_SYMBOL_RIGHT, LV_SYMBOL_OK, ""
};

static const lv_btnmatrix_ctrl_t kKeyboardCtrlUpper[] = {
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1,
  2, 1, 1, 1, 1, 1, 1, 1, 2,
  2, 1, 4, 1, 2
};

static const char* kKeyboardMapSpecial[] = {
  "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
  "-", "/", ":", ";", "(", ")", "$", "&", "@", "\"", "\n",
  "ABC", ".", ",", "?", "!", "'", "_", LV_SYMBOL_BACKSPACE, "\n",
  "abc", LV_SYMBOL_LEFT, " ", LV_SYMBOL_RIGHT, LV_SYMBOL_OK, ""
};

static const lv_btnmatrix_ctrl_t kKeyboardCtrlSpecial[] = {
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  2, 1, 1, 1, 1, 1, 1, 2,
  2, 1, 4, 1, 2
};

void applyGridKeyboardLayout(lv_obj_t* keyboard) {
  if (keyboard == nullptr) {
    return;
  }
  lv_keyboard_set_map(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER, kKeyboardMapLower, kKeyboardCtrlLower);
  lv_keyboard_set_map(keyboard, LV_KEYBOARD_MODE_TEXT_UPPER, kKeyboardMapUpper, kKeyboardCtrlUpper);
  lv_keyboard_set_map(keyboard, LV_KEYBOARD_MODE_SPECIAL, kKeyboardMapSpecial, kKeyboardCtrlSpecial);
}

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

bool isPublicChannelAlias(const std::string& normalizedName) {
  return normalizedName == "public" || normalizedName == "#public";
}

const char* visibleChannelLabel(const std::string& normalizedName, const std::string& originalName) {
  if (isPublicChannelAlias(normalizedName)) {
    return "Public";
  }
  return originalName.c_str();
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
        _contactSyncBtn(nullptr),
        _title(nullptr),
        _tabview(nullptr),
        _channelsList(nullptr),
        _addChannelBtn(nullptr),
        _contactsList(nullptr),
        _threadList(nullptr),
        _composer(nullptr),
        _input(nullptr),
        _sendBtn(nullptr),
        _channelEditorPanel(nullptr),
        _channelEditorInput(nullptr),
        _channelDeletePanel(nullptr),
        _contactDetailsPopup(nullptr),
        _detailsFavCheckbox(nullptr),
        _detailsFavActionLabel(nullptr),
        _inThread(false),
        _threadId(0),
        _threadPrivate(false),
        _contactsLoaded(false),
        _lastRefresh(0),
        _lastContactLongPressMs(0),
        _contactSortMode(ContactSortMode::LastSeen),
        _favoritesOnly(false),
        _detailsContactId(0) {
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
    if (_bridge != nullptr) {
      uint32_t requestedContactId = 0;
      if (_bridge->consumePendingContactDetails(requestedContactId) && requestedContactId != 0) {
        if (_inThread) {
          backToLanding();
        }
        if (_tabview != nullptr) {
          lv_tabview_set_act(_tabview, 1, LV_ANIM_OFF);
        }
        _contactsLoaded = true;
        refreshContactList();
        showContactDetails(requestedContactId);
      }
    }
    (void)_lastRefresh;
  }

  void onClose() override {
    _rowBindings.clear();

    if (_bridge != nullptr) {
      _bridge->clearThreadFilter();
    }

    WindowManager::instance().resetRightNavAction();
    WindowManager::instance().resetLeftNavAction();
    hideKeyboard();
    closeChannelEditor();
    closeDeleteChannelPrompt();
    closeContactDetails();

    _layout = nullptr;
  }

  void onMessageReceived(MeshMessage msg) override {
    if (_threadList == nullptr || !_inThread) {
      return;
    }

    if (msg.packetType == GRID_EVT_DIRECT_ACK) {
      if (msg.isPrivate && msg.threadId == _threadId) {
        tryResolveDirectAck(msg);
      }
      return;
    }

    // Group-repeat event: a rebroadcast of our own group message was heard.
    // Find the matching pending echo by threadId + timestamp and update the label.
    if (msg.packetType == GRID_EVT_GROUP_REPEAT) {
      if (msg.threadId == _threadId && !msg.isPrivate) {
        tryResolveRepeat(msg);
      }
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

    const uint32_t now = the_mesh.getRTCClock()->getCurrentTime();
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

  static void onChannelLongPressed(lv_event_t* e) {
    auto* self = static_cast<MessengerManagerApp*>(lv_event_get_user_data(e));
    if (self == nullptr) {
      return;
    }

    lv_obj_t* row = lv_event_get_target(e);
    for (const auto& binding : self->_rowBindings) {
      if (binding.row != row || binding.isPrivate) {
        continue;
      }
      self->openDeleteChannelPrompt(static_cast<uint8_t>(binding.id & 0xFFu));
      return;
    }
  }

  static void onAddChannelButton(lv_event_t* e) {
    auto* self = static_cast<MessengerManagerApp*>(lv_event_get_user_data(e));
    if (self != nullptr) {
      self->openChannelEditor();
    }
  }

  static void onChannelEditorSave(lv_event_t* e) {
    auto* self = static_cast<MessengerManagerApp*>(lv_event_get_user_data(e));
    if (self == nullptr || self->_channelEditorInput == nullptr) {
      return;
    }

    const char* text = lv_textarea_get_text(self->_channelEditorInput);
    uint8_t channelIdx = 0xFF;
    if (the_mesh.createHashtagChannel(text, channelIdx)) {
      self->closeChannelEditor();
      self->refreshChannelList();
    }
  }

  static void onChannelEditorCancel(lv_event_t* e) {
    auto* self = static_cast<MessengerManagerApp*>(lv_event_get_user_data(e));
    if (self != nullptr) {
      self->closeChannelEditor();
    }
  }

  static void onDeleteChannelConfirm(lv_event_t* e) {
    auto* self = static_cast<MessengerManagerApp*>(lv_event_get_user_data(e));
    if (self == nullptr) {
      return;
    }

    if (the_mesh.deleteChannelAt(self->_pendingDeleteChannelIdx)) {
      self->closeDeleteChannelPrompt();
      self->refreshChannelList();
      return;
    }
    self->closeDeleteChannelPrompt();
  }

  static void onDeleteChannelCancel(lv_event_t* e) {
    auto* self = static_cast<MessengerManagerApp*>(lv_event_get_user_data(e));
    if (self != nullptr) {
      self->closeDeleteChannelPrompt();
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

  static void onBackToTabs(lv_event_t* e) {
    auto* self = static_cast<MessengerManagerApp*>(lv_event_get_user_data(e));
    if (self == nullptr) {
      return;
    }

    if (self->_inThread) {
      self->backToLanding();
      return;
    }

    self->hideKeyboard();
    WindowManager::instance().openApp("home", false);
  }

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

  static void onBackgroundTapped(lv_event_t* e) {
    auto* self = static_cast<MessengerManagerApp*>(lv_event_get_user_data(e));
    if (self != nullptr) {
      self->dismissKeyboardFromOutsideTap();
    }
  }

  static void onTabChanged(lv_event_t* e) {
    auto* self = static_cast<MessengerManagerApp*>(lv_event_get_user_data(e));
    if (self == nullptr || self->_tabview == nullptr) {
      return;
    }

    uint16_t tab = lv_tabview_get_tab_act(self->_tabview);
    if (tab == 1) {
      if (self->_addChannelBtn != nullptr) {
        lv_obj_add_flag(self->_addChannelBtn, LV_OBJ_FLAG_HIDDEN);
      }
      if (self->_contactSortBtn != nullptr) {
        lv_obj_clear_flag(self->_contactSortBtn, LV_OBJ_FLAG_HIDDEN);
        self->updateContactFilterButton();
      }
      if (self->_contactSyncBtn != nullptr) {
        lv_obj_clear_flag(self->_contactSyncBtn, LV_OBJ_FLAG_HIDDEN);
      }
      WindowManager::instance().setRightNavAction(LV_SYMBOL_SETTINGS " Sort", [self]() { self->toggleContactSort(); });
      if (!self->_contactsLoaded) {
        self->refreshContactList();
        self->_contactsLoaded = true;
      }
    } else {
      if (self->_addChannelBtn != nullptr) {
        lv_obj_clear_flag(self->_addChannelBtn, LV_OBJ_FLAG_HIDDEN);
      }
      if (self->_contactSortBtn != nullptr) {
        lv_obj_add_flag(self->_contactSortBtn, LV_OBJ_FLAG_HIDDEN);
      }
      if (self->_contactSyncBtn != nullptr) {
        lv_obj_add_flag(self->_contactSyncBtn, LV_OBJ_FLAG_HIDDEN);
      }
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

  static void onContactFilterClicked(lv_event_t* e) {
    auto* self = static_cast<MessengerManagerApp*>(lv_event_get_user_data(e));
    if (self == nullptr) {
      return;
    }

    self->toggleFavoritesFilter();
  }

  static void onContactSyncClicked(lv_event_t* e) {
    auto* self = static_cast<MessengerManagerApp*>(lv_event_get_user_data(e));
    if (self == nullptr) {
      return;
    }

    self->_contactsLoaded = true;
    self->refreshContactList();
  }

  static void onFavoriteCheckboxChanged(lv_event_t* e) {
    auto* self = static_cast<MessengerManagerApp*>(lv_event_get_user_data(e));
    if (self == nullptr || self->_bridge == nullptr || self->_detailsContactId == 0) {
      return;
    }

    lv_obj_t* checkbox = lv_event_get_target(e);
    const bool checked = lv_obj_has_state(checkbox, LV_STATE_CHECKED);
    self->_bridge->setFavoriteContact(self->_detailsContactId, checked);
    if (self->_detailsFavActionLabel != nullptr) {
      lv_label_set_text(self->_detailsFavActionLabel, checked ? "Unmark Favorite" : "Mark Favorite");
    }
    self->refreshContactList();
  }

  static void onDetailsFavoriteClicked(lv_event_t* e) {
    auto* self = static_cast<MessengerManagerApp*>(lv_event_get_user_data(e));
    if (self == nullptr || self->_bridge == nullptr || self->_detailsContactId == 0) {
      return;
    }

    const bool nextFav = !self->_bridge->isFavoriteContact(self->_detailsContactId);
    self->_bridge->setFavoriteContact(self->_detailsContactId, nextFav);
    if (self->_detailsFavCheckbox != nullptr) {
      if (nextFav) {
        lv_obj_add_state(self->_detailsFavCheckbox, LV_STATE_CHECKED);
      } else {
        lv_obj_clear_state(self->_detailsFavCheckbox, LV_STATE_CHECKED);
      }
    }
    if (self->_detailsFavActionLabel != nullptr) {
      lv_label_set_text(self->_detailsFavActionLabel, nextFav ? "Unmark Favorite" : "Mark Favorite");
    }
    self->refreshContactList();
  }

  static void onDetailsCloseClicked(lv_event_t* e) {
    auto* self = static_cast<MessengerManagerApp*>(lv_event_get_user_data(e));
    if (self != nullptr) {
      self->closeContactDetails();
    }
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

  void toggleFavoritesFilter() {
    if (_inThread) {
      return;
    }

    _favoritesOnly = !_favoritesOnly;
    updateContactFilterButton();
    refreshContactList();
  }

  void updateContactFilterButton() {
    if (_contactSortBtn == nullptr) {
      return;
    }

    if (_favoritesOnly) {
      lv_obj_set_style_bg_color(_contactSortBtn, lv_color_hex(0x2B7FFF), 0);
    } else {
      lv_obj_set_style_bg_color(_contactSortBtn, lv_color_hex(0x1B2530), 0);
    }
  }

  void closeContactDetails() {
    if (_contactDetailsPopup != nullptr) {
      lv_obj_del(_contactDetailsPopup);
      _contactDetailsPopup = nullptr;
    }
    _detailsFavCheckbox = nullptr;
    _detailsFavActionLabel = nullptr;
    _detailsContactId = 0;
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
    if (_bridge == nullptr) {
      return;
    }

    MeshBridge::ContactSummary contactInfo{};
    const MeshBridge::ContactSummary* visible = findVisibleContact(contactId);
    if (visible != nullptr) {
      contactInfo = *visible;
    } else {
      bool found = false;
      auto contacts = _bridge->getContacts();
      for (const auto& contact : contacts) {
        if (contact.id != contactId) {
          continue;
        }
        contactInfo = contact;
        found = true;
        break;
      }
      if (!found) {
        return;
      }
    }

    closeContactDetails();

    char latText[24];
    char lonText[24];
    if (contactInfo.gpsLat == 0 && contactInfo.gpsLon == 0) {
      snprintf(latText, sizeof(latText), "unknown");
      snprintf(lonText, sizeof(lonText), "unknown");
    } else {
      snprintf(latText, sizeof(latText), "%.6f", static_cast<double>(contactInfo.gpsLat) / 1000000.0);
      snprintf(lonText, sizeof(lonText), "%.6f", static_cast<double>(contactInfo.gpsLon) / 1000000.0);
    }

    char details[900];
    snprintf(details,
             sizeof(details),
             "Name: %s\nAddress: 0x%08lX\nLast heard: %s ago\nAdvert timestamp (remote epoch): %lu\nPosition: lat %s, lon %s\nType: %u\nFlags: 0x%02X\nOut path len: %u\nSync since: %lu\nRecently heard: %s\nUnread: %d",
             contactInfo.name.c_str(),
             static_cast<unsigned long>(contactInfo.id),
             relativeAge(contactInfo.lastSeen),
             static_cast<unsigned long>(contactInfo.lastAdvertTimestamp),
             latText,
             lonText,
             static_cast<unsigned>(contactInfo.type),
             static_cast<unsigned>(contactInfo.flags),
             static_cast<unsigned>(contactInfo.outPathLen),
             static_cast<unsigned long>(contactInfo.syncSince),
             contactInfo.heardRecently ? "yes" : "no",
             _bridge->getUnreadCount(contactInfo.id, true));

    _contactDetailsPopup = lv_obj_create(lv_layer_top());
    _detailsContactId = contactInfo.id;
    lv_obj_set_size(_contactDetailsPopup, LV_PCT(94), LV_PCT(88));
    lv_obj_center(_contactDetailsPopup);
    lv_obj_set_style_bg_color(_contactDetailsPopup, lv_color_hex(0x0F141B), 0);
    lv_obj_set_style_border_color(_contactDetailsPopup, lv_color_hex(0x2B3A4D), 0);
    lv_obj_set_style_border_width(_contactDetailsPopup, 2, 0);
    lv_obj_set_style_radius(_contactDetailsPopup, 14, 0);
    lv_obj_set_style_pad_all(_contactDetailsPopup, 10, 0);
    lv_obj_clear_flag(_contactDetailsPopup, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(_contactDetailsPopup);
    lv_label_set_text(title, "Contact details");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xEDF4FF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t* content = lv_obj_create(_contactDetailsPopup);
    lv_obj_set_size(content, LV_PCT(100), LV_PCT(100));
    lv_obj_align(content, LV_ALIGN_TOP_MID, 0, 24);
    lv_obj_set_style_pad_top(content, 8, 0);
    lv_obj_set_style_pad_bottom(content, 56, 0);
    lv_obj_set_style_pad_left(content, 8, 0);
    lv_obj_set_style_pad_right(content, 8, 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_scroll_dir(content, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(content, 10, 0);

    lv_obj_t* detailsLabel = lv_label_create(content);
    lv_obj_set_width(detailsLabel, LV_PCT(100));
    lv_label_set_long_mode(detailsLabel, LV_LABEL_LONG_WRAP);
    lv_label_set_text(detailsLabel, details);
    lv_obj_set_style_text_font(detailsLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(detailsLabel, lv_color_hex(0xDCE7F5), 0);

    _detailsFavCheckbox = lv_checkbox_create(content);
    lv_checkbox_set_text(_detailsFavCheckbox, "Favorite contact");
    if (_bridge->isFavoriteContact(contactInfo.id)) {
      lv_obj_add_state(_detailsFavCheckbox, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(_detailsFavCheckbox, onFavoriteCheckboxChanged, LV_EVENT_VALUE_CHANGED, this);

    lv_obj_t* actionBar = lv_obj_create(_contactDetailsPopup);
    lv_obj_set_size(actionBar, LV_PCT(100), 44);
    lv_obj_align(actionBar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(actionBar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(actionBar, 0, 0);
    lv_obj_set_style_pad_all(actionBar, 0, 0);
    lv_obj_clear_flag(actionBar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* favBtn = lv_btn_create(actionBar);
    lv_obj_set_size(favBtn, LV_PCT(60), 40);
    lv_obj_align(favBtn, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(favBtn, lv_color_hex(0x2B7FFF), 0);
    lv_obj_set_style_bg_color(favBtn, lv_color_hex(0x1F63C6), LV_STATE_PRESSED);
    lv_obj_add_event_cb(favBtn, onDetailsFavoriteClicked, LV_EVENT_CLICKED, this);
    _detailsFavActionLabel = lv_label_create(favBtn);
    lv_label_set_text(_detailsFavActionLabel,
                      _bridge->isFavoriteContact(contactInfo.id) ? "Unmark Favorite" : "Mark Favorite");
    lv_obj_center(_detailsFavActionLabel);

    lv_obj_t* closeBtn = lv_btn_create(actionBar);
    lv_obj_set_size(closeBtn, LV_PCT(38), 40);
    lv_obj_align(closeBtn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(closeBtn, lv_color_hex(0x1B2530), 0);
    lv_obj_set_style_bg_color(closeBtn, lv_color_hex(0x334055), LV_STATE_PRESSED);
    lv_obj_add_event_cb(closeBtn, onDetailsCloseClicked, LV_EVENT_CLICKED, this);
    lv_obj_t* closeLabel = lv_label_create(closeBtn);
    lv_label_set_text(closeLabel, "Close");
    lv_obj_center(closeLabel);
  }

  void buildShell() {
    _header = lv_obj_create(_layout);
    lv_obj_set_size(_header, LV_PCT(100), 46);
    lv_obj_align(_header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_add_flag(_header, LV_OBJ_FLAG_CLICKABLE);
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

    if (_contactSyncBtn == nullptr) {
      _contactSyncBtn = lv_btn_create(_header);
      lv_obj_set_size(_contactSyncBtn, 58, 30);
      lv_obj_align(_contactSyncBtn, LV_ALIGN_LEFT_MID, 4, 0);
      lv_obj_set_style_radius(_contactSyncBtn, 8, 0);
      lv_obj_set_style_bg_color(_contactSyncBtn, lv_color_hex(0x1B2530), 0);
      lv_obj_set_style_bg_color(_contactSyncBtn, lv_color_hex(0x334055), LV_STATE_PRESSED);
      lv_obj_add_event_cb(_contactSyncBtn, onContactSyncClicked, LV_EVENT_CLICKED, this);
      lv_obj_t* syncLabel = lv_label_create(_contactSyncBtn);
      lv_label_set_text(syncLabel, "Sync");
      lv_obj_center(syncLabel);
    }
    lv_obj_add_flag(_contactSyncBtn, LV_OBJ_FLAG_HIDDEN);

    if (_contactSortBtn == nullptr) {
      _contactSortBtn = lv_btn_create(_header);
      lv_obj_set_size(_contactSortBtn, 54, 30);
      lv_obj_align(_contactSortBtn, LV_ALIGN_RIGHT_MID, -6, 0);
      lv_obj_set_style_radius(_contactSortBtn, 8, 0);
      lv_obj_set_style_bg_color(_contactSortBtn, lv_color_hex(0x1B2530), 0);
      lv_obj_set_style_bg_color(_contactSortBtn, lv_color_hex(0xFFB300), LV_STATE_PRESSED);
      lv_obj_add_event_cb(_contactSortBtn, onContactFilterClicked, LV_EVENT_CLICKED, this);

      lv_obj_t* favLabel = lv_label_create(_contactSortBtn);
      lv_label_set_text(favLabel, "FAV");
      lv_obj_center(favLabel);
    }
    lv_obj_add_flag(_contactSortBtn, LV_OBJ_FLAG_HIDDEN);

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
    lv_obj_add_event_cb(_header, onBackgroundTapped, LV_EVENT_CLICKED, this);
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
    applyGridKeyboardLayout(gKeyboard);

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

  void dismissKeyboardFromOutsideTap() {
    if (_input == nullptr || gKeyboard == nullptr) {
      return;
    }
    if (lv_obj_has_flag(gKeyboard, LV_OBJ_FLAG_HIDDEN)) {
      return;
    }

    lv_obj_clear_state(_input, LV_STATE_FOCUSED);
    hideKeyboard();
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
    WindowManager::instance().setLeftNavAction(LV_SYMBOL_LEFT " Back", [this]() {
      hideKeyboard();
      WindowManager::instance().openApp("home", false);
    });

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

    if (_addChannelBtn == nullptr) {
      _addChannelBtn = lv_btn_create(_header);
      lv_obj_set_size(_addChannelBtn, 34, 34);
      lv_obj_align(_addChannelBtn, LV_ALIGN_RIGHT_MID, -6, 0);
      lv_obj_set_style_radius(_addChannelBtn, LV_RADIUS_CIRCLE, 0);
      lv_obj_add_event_cb(_addChannelBtn, onAddChannelButton, LV_EVENT_CLICKED, this);
      lv_obj_t* addLabel = lv_label_create(_addChannelBtn);
      lv_label_set_text(addLabel, LV_SYMBOL_PLUS);
      lv_obj_center(addLabel);
    }
    lv_obj_clear_flag(_addChannelBtn, LV_OBJ_FLAG_HIDDEN);
    if (_contactSortBtn != nullptr) {
      lv_obj_add_flag(_contactSortBtn, LV_OBJ_FLAG_HIDDEN);
    }
    if (_contactSyncBtn != nullptr) {
      lv_obj_add_flag(_contactSyncBtn, LV_OBJ_FLAG_HIDDEN);
    }

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
    bool publicAliasShown = false;
    for (const auto& channel : channels) {
      if (shown >= kMaxRows) {
        break;
      }
      const std::string normalizedName = normalizedChannelName(channel.name);
      if (normalizedName.empty() || normalizedName == "channel") {
        skipped++;
        continue;
      }

      if (isPublicChannelAlias(normalizedName)) {
        if (publicAliasShown) {
          skipped++;
          continue;
        }
        publicAliasShown = true;
      }

      char label[48];
      snprintf(label, sizeof(label), "%s", visibleChannelLabel(normalizedName, channel.name));

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
      lv_obj_add_event_cb(row, onChannelLongPressed, LV_EVENT_LONG_PRESSED, this);
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

  void openChannelEditor() {
    closeChannelEditor();

    _channelEditorPanel = lv_obj_create(lv_layer_top());
    lv_obj_set_size(_channelEditorPanel, LV_PCT(92), 170);
    lv_obj_align(_channelEditorPanel, LV_ALIGN_TOP_MID, 0, 12);
    lv_obj_set_style_bg_color(_channelEditorPanel, lv_color_hex(0x0F141B), 0);
    lv_obj_set_style_border_color(_channelEditorPanel, lv_color_hex(0x2D3A4A), 0);
    lv_obj_set_style_radius(_channelEditorPanel, 12, 0);

    lv_obj_t* title = lv_label_create(_channelEditorPanel);
    lv_label_set_text(title, "Add Channel");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 10, 8);

    _channelEditorInput = lv_textarea_create(_channelEditorPanel);
    lv_obj_set_size(_channelEditorInput, LV_PCT(96), 50);
    lv_obj_align(_channelEditorInput, LV_ALIGN_TOP_MID, 0, 34);
    lv_textarea_set_one_line(_channelEditorInput, true);
    lv_textarea_set_placeholder_text(_channelEditorInput, "#channel");

    lv_obj_t* saveBtn = lv_btn_create(_channelEditorPanel);
    lv_obj_set_size(saveBtn, 92, 34);
    lv_obj_align(saveBtn, LV_ALIGN_BOTTOM_LEFT, 12, -10);
    lv_obj_add_event_cb(saveBtn, onChannelEditorSave, LV_EVENT_CLICKED, this);
    lv_obj_t* saveLbl = lv_label_create(saveBtn);
    lv_label_set_text(saveLbl, "Add");
    lv_obj_center(saveLbl);

    lv_obj_t* cancelBtn = lv_btn_create(_channelEditorPanel);
    lv_obj_set_size(cancelBtn, 92, 34);
    lv_obj_align(cancelBtn, LV_ALIGN_BOTTOM_RIGHT, -12, -10);
    lv_obj_add_event_cb(cancelBtn, onChannelEditorCancel, LV_EVENT_CLICKED, this);
    lv_obj_t* cancelLbl = lv_label_create(cancelBtn);
    lv_label_set_text(cancelLbl, "Cancel");
    lv_obj_center(cancelLbl);

    ensureGlobalKeyboard();
    if (gKeyboard != nullptr) {
      gKeyboardTarget = _channelEditorInput;
      lv_keyboard_set_textarea(gKeyboard, _channelEditorInput);
      lv_keyboard_set_mode(gKeyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
      lv_obj_clear_flag(gKeyboard, LV_OBJ_FLAG_HIDDEN);
      lv_obj_align(gKeyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    }
  }

  void closeChannelEditor() {
    if (gKeyboard != nullptr && gKeyboardTarget == _channelEditorInput) {
      lv_keyboard_set_textarea(gKeyboard, nullptr);
      lv_obj_add_flag(gKeyboard, LV_OBJ_FLAG_HIDDEN);
      gKeyboardTarget = nullptr;
    }
    if (_channelEditorPanel != nullptr) {
      lv_obj_del(_channelEditorPanel);
      _channelEditorPanel = nullptr;
    }
    _channelEditorInput = nullptr;
  }

  void openDeleteChannelPrompt(uint8_t channelIdx) {
    ChannelDetails details;
    if (!the_mesh.getChannel(channelIdx, details)) {
      return;
    }
    if (details.name[0] == '\0' || isPublicChannelAlias(normalizedChannelName(details.name))) {
      return;
    }

    closeDeleteChannelPrompt();
    _pendingDeleteChannelIdx = channelIdx;

    _channelDeletePanel = lv_obj_create(lv_layer_top());
    lv_obj_set_size(_channelDeletePanel, LV_PCT(88), 140);
    lv_obj_align(_channelDeletePanel, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_bg_color(_channelDeletePanel, lv_color_hex(0x0F141B), 0);
    lv_obj_set_style_border_color(_channelDeletePanel, lv_color_hex(0x2D3A4A), 0);
    lv_obj_set_style_radius(_channelDeletePanel, 12, 0);

    char prompt[96];
    snprintf(prompt, sizeof(prompt), "Delete channel %s?", details.name);
    lv_obj_t* label = lv_label_create(_channelDeletePanel);
    lv_label_set_text(label, prompt);
    lv_obj_set_width(label, LV_PCT(90));
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 16);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t* deleteBtn = lv_btn_create(_channelDeletePanel);
    lv_obj_set_size(deleteBtn, 96, 34);
    lv_obj_align(deleteBtn, LV_ALIGN_BOTTOM_LEFT, 12, -10);
    lv_obj_add_event_cb(deleteBtn, onDeleteChannelConfirm, LV_EVENT_CLICKED, this);
    lv_obj_t* deleteLbl = lv_label_create(deleteBtn);
    lv_label_set_text(deleteLbl, "Delete");
    lv_obj_center(deleteLbl);

    lv_obj_t* cancelBtn = lv_btn_create(_channelDeletePanel);
    lv_obj_set_size(cancelBtn, 96, 34);
    lv_obj_align(cancelBtn, LV_ALIGN_BOTTOM_RIGHT, -12, -10);
    lv_obj_add_event_cb(cancelBtn, onDeleteChannelCancel, LV_EVENT_CLICKED, this);
    lv_obj_t* cancelLbl = lv_label_create(cancelBtn);
    lv_label_set_text(cancelLbl, "Cancel");
    lv_obj_center(cancelLbl);
  }

  void closeDeleteChannelPrompt() {
    if (_channelDeletePanel != nullptr) {
      lv_obj_del(_channelDeletePanel);
      _channelDeletePanel = nullptr;
    }
    _pendingDeleteChannelIdx = 0xFF;
  }

  void refreshContactList() {
    if (_contactsList == nullptr || _bridge == nullptr) {
      return;
    }

    lv_obj_clean(_contactsList);
    _rowBindings.clear();
    _visibleContacts.clear();
    auto contacts = _bridge->getContacts();
    const uint32_t now = the_mesh.getRTCClock()->getCurrentTime();

    if (_favoritesOnly) {
      contacts.erase(std::remove_if(contacts.begin(), contacts.end(), [](const MeshBridge::ContactSummary& contact) {
        return (contact.flags & 0x01u) == 0;
      }), contacts.end());
    }

    if (_contactSortMode == ContactSortMode::Name) {
      std::sort(contacts.begin(), contacts.end(), contactNameLess);
    } else {
      std::sort(contacts.begin(), contacts.end(), [now](const MeshBridge::ContactSummary& a, const MeshBridge::ContactSummary& b) {
        const bool aUnknown = (a.lastSeen == 0) || (now == 0) || (a.lastSeen > now);
        const bool bUnknown = (b.lastSeen == 0) || (now == 0) || (b.lastSeen > now);
        if (aUnknown != bUnknown) {
          return bUnknown;
        }
        if (a.lastSeen == b.lastSeen) {
          return contactNameLess(a, b);
        }
        return a.lastSeen > b.lastSeen;
      });
    }

    if (contacts.empty()) {
      lv_obj_t* t = lv_list_add_text(_contactsList,
                                     _favoritesOnly ? "No favorite contacts yet"
                                                    : "No contacts in routing table");
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
      char timeText[28];
      if (_contactSortMode == ContactSortMode::LastSeen && contact.lastSeen != 0) {
        snprintf(timeText, sizeof(timeText), "%s ago", relativeAge(contact.lastSeen));
      } else {
        snprintf(timeText, sizeof(timeText), "%s", formatClockTime(contact.lastSeen));
      }
      lv_label_set_text(timeLabel, timeText);
      lv_obj_set_width(timeLabel, 92);
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
    // For column layout: main_align=vertical, cross_align=horizontal (left/right)
    lv_obj_set_flex_align(row,
                          LV_FLEX_ALIGN_START,
                          isMe ? LV_FLEX_ALIGN_END : LV_FLEX_ALIGN_START,
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
        if (_threadPrivate) {
          snprintf(metaText, sizeof(metaText), "* ACK heard • Delivered");
        } else {
          snprintf(metaText, sizeof(metaText), "* Heard: %u", static_cast<unsigned>(timesHeard));
        }
        lv_obj_set_style_text_color(meta, lv_color_hex(0x00D084), 0);  // Green = confirmed by mesh
      } else {
        snprintf(metaText, sizeof(metaText), "* Sent");
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
      const bool isDirect = (hopCount == 0xFF);
      const uint8_t hops = static_cast<uint8_t>(hopCount & 0x3F);
      if (isDirect) {
        snprintf(hopsText, sizeof(hopsText), "~ Direct");
        lv_obj_set_style_text_color(metaHops, lv_color_hex(0x00D084), 0);  // Green = zero hops
      } else {
        snprintf(hopsText, sizeof(hopsText), "~ %u Hops", static_cast<unsigned>(hops));
        lv_obj_set_style_text_color(metaHops, lv_color_hex(0x6D7B8E), 0);  // Grey = via repeater
      }
      lv_label_set_text(metaHops, hopsText);
      lv_obj_set_style_text_font(metaHops, &lv_font_montserrat_10, 0);
      lv_obj_set_style_pad_top(metaHops, 2, 0);
      lv_obj_set_style_pad_left(metaHops, 2, 0);
      meta = metaHops;
    }

    lv_obj_update_layout(_threadList);
    // Scroll to latest message (bottom of thread)
    lv_obj_scroll_to_y(_threadList, lv_obj_get_height(_threadList), LV_ANIM_OFF);
    return meta;
  }

  bool tryResolveRepeat(const MeshMessage& msg) {
    // Match the most recent pending echo for this thread by timestamp (10-second tolerance).
    // Used for GRID_EVT_GROUP_REPEAT events where we know hash already matched.
    constexpr uint32_t kRepeatTimestampToleranceMs = 10000;
    for (auto it = _pendingEchoes.rbegin(); it != _pendingEchoes.rend(); ++it) {
      auto& pending = *it;
      if (pending.threadId != msg.threadId || pending.isPrivate != msg.isPrivate) {
        continue;
      }
      const uint32_t tsA = pending.timestamp;
      const uint32_t tsB = msg.timestamp;
      const uint32_t diff = (tsA > tsB) ? (tsA - tsB) : (tsB - tsA);
      if (diff > kRepeatTimestampToleranceMs) {
        continue;
      }
      pending.timesHeard = msg.timesHeard;
      if (pending.metaLabel != nullptr) {
        char buf[48];
        snprintf(buf, sizeof(buf), "* Heard: %u", static_cast<unsigned>(pending.timesHeard));
        lv_label_set_text(pending.metaLabel, buf);
        lv_obj_set_style_text_color(pending.metaLabel, lv_color_hex(0x00D084), 0); // green = confirmed
      }
      return true;
    }
    return false;
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
      if (msg.isPrivate) {
        snprintf(heardText, sizeof(heardText), "* ACK heard • Delivered");
      } else {
        snprintf(heardText, sizeof(heardText), "* Heard: %u", static_cast<unsigned>(pending.timesHeard));
      }
      lv_label_set_text(pending.metaLabel, heardText);
      lv_obj_set_style_text_color(pending.metaLabel, lv_color_hex(0x00D084), 0);  // Green = confirmed
      return true;
    }
    return false;
  }

  bool tryResolveDirectAck(const MeshMessage& msg) {
    if (!msg.isPrivate) {
      return false;
    }

    for (auto it = _pendingEchoes.rbegin(); it != _pendingEchoes.rend(); ++it) {
      auto& pending = *it;
      if (!pending.isPrivate || pending.threadId != msg.threadId) {
        continue;
      }
      if (pending.timesHeard > 0) {
        continue;
      }

      pending.timesHeard = 1;
      if (pending.metaLabel != nullptr) {
        char delivered[48];
        snprintf(delivered, sizeof(delivered), "* ACK heard • Delivered");
        lv_label_set_text(pending.metaLabel, delivered);
        lv_obj_set_style_text_color(pending.metaLabel, lv_color_hex(0x00D084), 0);
      }
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

  std::string resolveThreadTitle(uint32_t id, bool isPrivate) const {
    if (_bridge == nullptr) {
      return isPrivate ? "Direct Chat" : "Channel Chat";
    }

    if (isPrivate) {
      auto contacts = _bridge->getContacts();
      for (const auto& contact : contacts) {
        if (contact.id != id) {
          continue;
        }
        if (!contact.name.empty()) {
          return contact.name;
        }
        char fallback[16];
        snprintf(fallback, sizeof(fallback), "%08lX", static_cast<unsigned long>(id));
        return fallback;
      }
      return "Direct Chat";
    }

    auto channels = _bridge->getChannels();
    const uint8_t channelId = static_cast<uint8_t>(id & 0xFFu);
    for (const auto& channel : channels) {
      if (channel.id != channelId) {
        continue;
      }
      if (!channel.name.empty()) {
        const std::string normalizedName = normalizedChannelName(channel.name);
        return visibleChannelLabel(normalizedName, channel.name);
      }
      break;
    }

    return "Channel Chat";
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
    if (_addChannelBtn != nullptr) {
      lv_obj_add_flag(_addChannelBtn, LV_OBJ_FLAG_HIDDEN);
    }
    if (_contactSortBtn != nullptr) {
      lv_obj_add_flag(_contactSortBtn, LV_OBJ_FLAG_HIDDEN);
    }
    if (_contactSyncBtn != nullptr) {
      lv_obj_add_flag(_contactSyncBtn, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_add_flag(_backBtn, LV_OBJ_FLAG_HIDDEN);
    const std::string threadTitle = resolveThreadTitle(id, isPrivate);
    lv_label_set_text(_title, threadTitle.c_str());
    WindowManager::instance().setLeftNavAction(LV_SYMBOL_LEFT " Back", [this]() { backToLanding(); });
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
    lv_obj_add_flag(_threadList, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(_threadList, LV_PCT(100), LV_PCT(100));
    lv_obj_set_height(_threadList, lv_obj_get_height(_content) - navOverlapInContent());
    lv_obj_align(_threadList, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(_threadList, lv_color_hex(0x101820), 0);
    lv_obj_set_style_bg_opa(_threadList, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_threadList, 0, 0);
    lv_obj_set_style_pad_top(_threadList, 6, 0);
    lv_obj_set_style_pad_bottom(_threadList, 70, 0);  // Extra padding to avoid navbar overlap
    lv_obj_set_style_pad_left(_threadList, 0, 0);
    lv_obj_set_style_pad_right(_threadList, 0, 0);
    lv_obj_set_style_pad_row(_threadList, 2, 0);
    lv_obj_set_scrollbar_mode(_threadList, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(_threadList, LV_DIR_VER);
    lv_obj_set_flex_flow(_threadList, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_threadList, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_add_event_cb(_threadList, onBackgroundTapped, LV_EVENT_CLICKED, this);

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
  lv_obj_t* _contactSyncBtn;
  lv_obj_t* _title;

  lv_obj_t* _tabview;
  lv_obj_t* _channelsList;
  lv_obj_t* _addChannelBtn;
  lv_obj_t* _contactsList;
  lv_obj_t* _threadList;
  lv_obj_t* _composer;
  lv_obj_t* _input;
  lv_obj_t* _sendBtn;
  lv_obj_t* _channelEditorPanel;
  lv_obj_t* _channelEditorInput;
  lv_obj_t* _channelDeletePanel;
  lv_obj_t* _contactDetailsPopup;
  lv_obj_t* _detailsFavCheckbox;
  lv_obj_t* _detailsFavActionLabel;

  bool _inThread;
  uint32_t _threadId;
  bool _threadPrivate;
  uint8_t _pendingDeleteChannelIdx = 0xFF;
  bool _contactsLoaded;
  uint32_t _lastRefresh;
  uint32_t _lastContactLongPressMs;
  ContactSortMode _contactSortMode;
  bool _favoritesOnly;
  uint32_t _detailsContactId;
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
