#pragma once

#include <functional>
#include <string>
#include <vector>

#include <Arduino.h>
#include <esp_heap_caps.h>

#include "lvgl.h"

#include "MeshApp.h"
#include "MeshBridge.h"

class WindowManager {
public:
  struct AppDescriptor {
    const char* id;
    const char* label;
    const char* icon;
    std::function<MeshApp*()> factory;
  };

  static WindowManager& instance();

  bool begin(MeshBridge& bridge, lv_obj_t* root = nullptr);

  void tick();

  void registerApp(const AppDescriptor& app);
  const std::vector<AppDescriptor>& apps() const { return _registry; }

  bool openApp(const char* appId, bool pushToStack = true);
  void goBack();
  void goHome();
  void setRightNavAction(const char* label, std::function<void()> handler);
  void resetRightNavAction();

  lv_obj_t* contentRoot() const { return _contentRoot; }

  template <typename T, typename... Args>
  static T* createInPsram(Args&&... args) {
    void* mem = heap_caps_malloc(sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (mem == nullptr) {
      return nullptr;
    }
    return new (mem) T(std::forward<Args>(args)...);
  }

private:
  WindowManager();

  void buildShell(lv_obj_t* root);
  void buildStatusBar();
  void buildNavigationBar();
  void updateStatusSignal();
  void createTheme();
  void powerPeripherals();

  void transitionTo(lv_obj_t* newScreen, bool fromRight);
  void destroyCurrentApp();

  static void onBackPressed(lv_event_t* e);
  static void onRightNavPressed(lv_event_t* e);

  MeshBridge* _bridge;
  std::vector<AppDescriptor> _registry;
  std::vector<std::string> _appStack;

  MeshApp* _activeApp;
  std::string _activeId;

  lv_obj_t* _root;
  lv_obj_t* _statusBar;
  lv_obj_t* _statusClockLabel;
  lv_obj_t* _statusSignalLabel;
  lv_obj_t* _statusSignalBars[4];
  lv_obj_t* _statusBleLabel;
  lv_obj_t* _statusBatteryLabel;
  lv_obj_t* _navBar;
  lv_obj_t* _rightNavButton;
  lv_obj_t* _rightNavLabel;
  lv_obj_t* _contentRoot;
  lv_obj_t* _activeScreen;
  std::function<void()> _rightNavHandler;

  lv_style_t _styleShell;
  lv_style_t _styleCard;
  lv_style_t _styleStatus;
  lv_style_t _styleNav;
  lv_style_t _styleButton;
  uint32_t _lastClockUpdateMs;
  uint32_t _lastBatteryUpdateMs;
};
