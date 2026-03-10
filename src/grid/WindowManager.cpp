#include "WindowManager.h"

#include <algorithm>

namespace {
constexpr int kStatusHeight = 32;
constexpr int kNavHeight = 52;
constexpr bool kEnableFullscreenSlideAnimation = false;

constexpr uint32_t COLOR_BG = 0x101318;
constexpr uint32_t COLOR_SURFACE = 0x1A1F27;
constexpr uint32_t COLOR_ACCENT = 0x66D9EF;
constexpr uint32_t COLOR_TEXT = 0xE2E8F0;
constexpr uint32_t COLOR_DIM = 0x7C8799;
}

WindowManager& WindowManager::instance() {
  static WindowManager wm;
  return wm;
}

WindowManager::WindowManager()
    : _bridge(nullptr),
      _activeApp(nullptr),
      _root(nullptr),
      _statusBar(nullptr),
  _statusSignalLabel(nullptr),
  _statusSignalBars{nullptr, nullptr, nullptr, nullptr},
      _navBar(nullptr),
      _contentRoot(nullptr),
      _activeScreen(nullptr) {}

bool WindowManager::begin(MeshBridge& bridge, lv_obj_t* root) {
  _bridge = &bridge;
  _root = root ? root : lv_scr_act();

  powerPeripherals();
  createTheme();
  buildShell(_root);

  return true;
}

void WindowManager::tick() {
  if (_activeApp) {
    _activeApp->onLoop();
  }
  if (_bridge) {
    _bridge->dispatchForUi();
    updateStatusSignal();
  }
}

void WindowManager::registerApp(const AppDescriptor& app) {
  _registry.push_back(app);
}

bool WindowManager::openApp(const char* appId, bool pushToStack) {
  if (appId == nullptr) {
    return false;
  }

  auto it = std::find_if(_registry.begin(), _registry.end(), [appId](const AppDescriptor& d) {
    return strcmp(d.id, appId) == 0;
  });
  if (it == _registry.end() || !it->factory) {
    return false;
  }

  if (pushToStack && !_activeId.empty()) {
    _appStack.push_back(_activeId);
  }

  MeshApp* nextApp = it->factory();
  if (nextApp == nullptr) {
    return false;
  }

  lv_obj_t* prevScreen = _activeScreen;

  lv_obj_t* nextScreen = lv_obj_create(_contentRoot);
  lv_obj_remove_style_all(nextScreen);
  lv_obj_add_style(nextScreen, &_styleShell, 0);
  lv_obj_set_size(nextScreen, LV_PCT(100), LV_PCT(100));

  nextApp->onStart(nextScreen);
  transitionTo(nextScreen, true);

  destroyCurrentApp();
  _activeApp = nextApp;
  _activeId = it->id;

  if (_bridge) {
    _bridge->setActiveApp(_activeApp);
  }
  if (prevScreen != nullptr) {
    lv_obj_del_async(prevScreen);
  }
  return true;
}

void WindowManager::goBack() {
  if (_appStack.empty()) {
    return;
  }
  std::string previous = _appStack.back();
  _appStack.pop_back();
  openApp(previous.c_str(), false);
}

void WindowManager::goHome() {
  if (!_registry.empty()) {
    _appStack.clear();
    openApp(_registry.front().id, false);
  }
}

void WindowManager::buildShell(lv_obj_t* root) {
  lv_obj_remove_style_all(root);
  lv_obj_add_style(root, &_styleShell, 0);

  _statusBar = lv_obj_create(root);
  lv_obj_remove_style_all(_statusBar);
  lv_obj_add_style(_statusBar, &_styleStatus, 0);
  lv_obj_set_size(_statusBar, LV_PCT(100), kStatusHeight);
  lv_obj_align(_statusBar, LV_ALIGN_TOP_MID, 0, 0);

  _contentRoot = lv_obj_create(root);
  lv_obj_remove_style_all(_contentRoot);
  lv_obj_add_style(_contentRoot, &_styleShell, 0);
  lv_obj_set_size(_contentRoot, LV_PCT(100), LV_VER_RES - kStatusHeight - kNavHeight);
  lv_obj_align(_contentRoot, LV_ALIGN_TOP_MID, 0, kStatusHeight);
  lv_obj_set_scrollbar_mode(_contentRoot, LV_SCROLLBAR_MODE_OFF);

  _navBar = lv_obj_create(root);
  lv_obj_remove_style_all(_navBar);
  lv_obj_add_style(_navBar, &_styleNav, 0);
  lv_obj_set_size(_navBar, LV_PCT(100), kNavHeight);
  lv_obj_align(_navBar, LV_ALIGN_BOTTOM_MID, 0, 0);

  buildStatusBar();
  buildNavigationBar();
}

void WindowManager::buildStatusBar() {
  lv_obj_t* clock = lv_label_create(_statusBar);
  lv_label_set_text(clock, "12:00");
  lv_obj_align(clock, LV_ALIGN_LEFT_MID, 10, 0);
  lv_obj_set_style_text_color(clock, lv_color_hex(COLOR_TEXT), 0);

  lv_obj_t* meter = lv_obj_create(_statusBar);
  lv_obj_remove_style_all(meter);
  lv_obj_set_size(meter, 122, 24);
  lv_obj_align(meter, LV_ALIGN_CENTER, 0, 0);

  for (int i = 0; i < 4; ++i) {
    _statusSignalBars[i] = lv_obj_create(meter);
    lv_obj_remove_style_all(_statusSignalBars[i]);
    lv_obj_set_size(_statusSignalBars[i], 4, 6 + i * 4);
    lv_obj_align(_statusSignalBars[i], LV_ALIGN_LEFT_MID, i * 6, 4 - i * 2);
    lv_obj_set_style_radius(_statusSignalBars[i], 1, 0);
    lv_obj_set_style_bg_color(_statusSignalBars[i], lv_color_hex(0x384152), 0);
    lv_obj_set_style_bg_opa(_statusSignalBars[i], LV_OPA_COVER, 0);
  }

  _statusSignalLabel = lv_label_create(meter);
  lv_label_set_text(_statusSignalLabel, "SNR --");
  lv_obj_align(_statusSignalLabel, LV_ALIGN_LEFT_MID, 30, 0);
  lv_obj_set_style_text_color(_statusSignalLabel, lv_color_hex(COLOR_DIM), 0);
  lv_obj_set_style_text_font(_statusSignalLabel, &lv_font_montserrat_14, 0);

  lv_obj_t* batt = lv_label_create(_statusBar);
  lv_label_set_text(batt, "84%");
  lv_obj_align(batt, LV_ALIGN_RIGHT_MID, -12, 0);
  lv_obj_set_style_text_color(batt, lv_color_hex(COLOR_TEXT), 0);
}

void WindowManager::updateStatusSignal() {
  if (_bridge == nullptr || _statusSignalLabel == nullptr) {
    return;
  }

  int barsOn = 0;
  int8_t snr = 0;

  if (_bridge->hasRadioMetrics()) {
    const int16_t rssi = _bridge->lastRssi();
    snr = _bridge->lastSnr();

    if (rssi >= -70) {
      barsOn = 4;
    } else if (rssi >= -85) {
      barsOn = 3;
    } else if (rssi >= -100) {
      barsOn = 2;
    } else if (rssi >= -112) {
      barsOn = 1;
    }

    char snrText[24];
    snprintf(snrText, sizeof(snrText), "SNR %d dB", (int)snr);
    lv_label_set_text(_statusSignalLabel, snrText);
  } else {
    lv_label_set_text(_statusSignalLabel, "SNR --");
  }

  for (int i = 0; i < 4; ++i) {
    if (_statusSignalBars[i] == nullptr) {
      continue;
    }
    const bool on = i < barsOn;
    lv_obj_set_style_bg_color(_statusSignalBars[i], on ? lv_color_hex(0x59D37A) : lv_color_hex(0x384152), 0);
  }
}

void WindowManager::buildNavigationBar() {
  lv_obj_t* back = lv_btn_create(_navBar);
  lv_obj_add_style(back, &_styleButton, 0);
  lv_obj_set_size(back, 96, 36);
  lv_obj_align(back, LV_ALIGN_LEFT_MID, 10, 0);
  lv_obj_add_event_cb(back, onBackPressed, LV_EVENT_CLICKED, this);
  lv_obj_t* backLabel = lv_label_create(back);
  lv_label_set_text(backLabel, LV_SYMBOL_LEFT " Back");
  lv_obj_center(backLabel);

  lv_obj_t* home = lv_btn_create(_navBar);
  lv_obj_add_style(home, &_styleButton, 0);
  lv_obj_set_size(home, 96, 36);
  lv_obj_align(home, LV_ALIGN_RIGHT_MID, -10, 0);
  lv_obj_add_event_cb(home, onHomePressed, LV_EVENT_CLICKED, this);
  lv_obj_t* homeLabel = lv_label_create(home);
  lv_label_set_text(homeLabel, LV_SYMBOL_HOME " Home");
  lv_obj_center(homeLabel);
}

void WindowManager::createTheme() {
  lv_style_init(&_styleShell);
  lv_style_set_bg_color(&_styleShell, lv_color_hex(COLOR_BG));
  lv_style_set_bg_opa(&_styleShell, LV_OPA_COVER);
  lv_style_set_text_color(&_styleShell, lv_color_hex(COLOR_TEXT));

  lv_style_init(&_styleCard);
  lv_style_set_bg_color(&_styleCard, lv_color_hex(COLOR_SURFACE));
  lv_style_set_bg_opa(&_styleCard, LV_OPA_COVER);
  lv_style_set_border_color(&_styleCard, lv_color_hex(0x2A3039));
  lv_style_set_border_width(&_styleCard, 1);
  lv_style_set_radius(&_styleCard, 14);
  lv_style_set_shadow_width(&_styleCard, 0);
  lv_style_set_shadow_opa(&_styleCard, LV_OPA_TRANSP);
  lv_style_set_shadow_color(&_styleCard, lv_color_black());

  lv_style_init(&_styleStatus);
  lv_style_set_bg_color(&_styleStatus, lv_color_hex(0x0C0F14));
  lv_style_set_border_color(&_styleStatus, lv_color_hex(0x2A3039));
  lv_style_set_border_side(&_styleStatus, LV_BORDER_SIDE_BOTTOM);
  lv_style_set_border_width(&_styleStatus, 1);

  lv_style_init(&_styleNav);
  lv_style_set_bg_color(&_styleNav, lv_color_hex(0x0C0F14));
  lv_style_set_border_color(&_styleNav, lv_color_hex(0x2A3039));
  lv_style_set_border_side(&_styleNav, LV_BORDER_SIDE_TOP);
  lv_style_set_border_width(&_styleNav, 1);

  lv_style_init(&_styleButton);
  lv_style_set_radius(&_styleButton, 12);
  lv_style_set_bg_color(&_styleButton, lv_color_hex(COLOR_SURFACE));
  lv_style_set_border_color(&_styleButton, lv_color_hex(COLOR_ACCENT));
  lv_style_set_border_width(&_styleButton, 1);
  lv_style_set_text_color(&_styleButton, lv_color_hex(COLOR_TEXT));
}

void WindowManager::powerPeripherals() {
#ifdef PIN_VEXT_EN
  pinMode(PIN_VEXT_EN, OUTPUT);
#ifdef PIN_VEXT_EN_ACTIVE
  digitalWrite(PIN_VEXT_EN, PIN_VEXT_EN_ACTIVE);
#else
  digitalWrite(PIN_VEXT_EN, HIGH);
#endif
#else
  pinMode(45, OUTPUT);
  digitalWrite(45, HIGH);
#endif
  delay(120);
}

void WindowManager::transitionTo(lv_obj_t* newScreen, bool fromRight) {
  if (!kEnableFullscreenSlideAnimation) {
    lv_obj_set_pos(newScreen, 0, 0);
    _activeScreen = newScreen;
    return;
  }

  const int width = lv_obj_get_width(_contentRoot);
  const int startX = fromRight ? width : -width;
  const int endX = 0;

  lv_obj_set_pos(newScreen, startX, 0);

  lv_anim_t inAnim;
  lv_anim_init(&inAnim);
  lv_anim_set_var(&inAnim, newScreen);
  lv_anim_set_exec_cb(&inAnim, (lv_anim_exec_xcb_t)lv_obj_set_x);
  lv_anim_set_time(&inAnim, 100);
  lv_anim_set_values(&inAnim, startX, endX);
  lv_anim_set_path_cb(&inAnim, lv_anim_path_ease_out);
  lv_anim_start(&inAnim);

  _activeScreen = newScreen;
}

void WindowManager::destroyCurrentApp() {
  if (_activeApp) {
    _activeApp->onClose();
    _activeApp->release();
    _activeApp = nullptr;
  }
}

void WindowManager::onBackPressed(lv_event_t* e) {
  auto* self = static_cast<WindowManager*>(lv_event_get_user_data(e));
  if (self) {
    self->goBack();
  }
}

void WindowManager::onHomePressed(lv_event_t* e) {
  auto* self = static_cast<WindowManager*>(lv_event_get_user_data(e));
  if (self) {
    self->goHome();
  }
}
