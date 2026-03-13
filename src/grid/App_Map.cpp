#include "GridApps.h"
#include "GridRuntimeSettings.h"

#include "MyMesh.h"
#include "WindowManager.h"
#include "target.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_set>
#include <vector>

namespace {
double degToRad(double deg) {
  return deg * 0.017453292519943295;
}

double haversineKm(double lat1Deg, double lon1Deg, double lat2Deg, double lon2Deg) {
  constexpr double kEarthRadiusKm = 6371.0088;
  const double lat1 = degToRad(lat1Deg);
  const double lon1 = degToRad(lon1Deg);
  const double lat2 = degToRad(lat2Deg);
  const double lon2 = degToRad(lon2Deg);
  const double dLat = lat2 - lat1;
  const double dLon = lon2 - lon1;
  const double sinHalfLat = sin(dLat * 0.5);
  const double sinHalfLon = sin(dLon * 0.5);
  const double a = sinHalfLat * sinHalfLat + cos(lat1) * cos(lat2) * sinHalfLon * sinHalfLon;
  const double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
  return kEarthRadiusKm * c;
}

double bearingRad(double lat1Deg, double lon1Deg, double lat2Deg, double lon2Deg) {
  const double lat1 = degToRad(lat1Deg);
  const double lat2 = degToRad(lat2Deg);
  const double dLon = degToRad(lon2Deg - lon1Deg);
  const double y = sin(dLon) * cos(lat2);
  const double x = cos(lat1) * sin(lat2) - sin(lat1) * cos(lat2) * cos(dLon);
  double b = atan2(y, x);
  if (b < 0.0) {
    b += 2.0 * 3.14159265358979323846;
  }
  return b;
}

class MapApp : public MeshApp {
public:
  static constexpr size_t kMaxMapNodes = 28;

  struct RelativeNode {
    uint32_t id;
    std::string name;
    double distanceKm;
    double bearing;
    lv_point_t linePts[2];
    lv_obj_t* line;
    lv_obj_t* marker;
    lv_obj_t* nameLabel;
    lv_obj_t* distanceLabel;
  };

  explicit MapApp(MeshBridge* bridge)
      : _bridge(bridge),
        _layout(nullptr),
        _toolbar(nullptr),
        _hintLabel(nullptr),
        _scaleBar(nullptr),
        _scaleLabel(nullptr),
        _overlayZoomOutBtn(nullptr),
        _overlayZoomInBtn(nullptr),
        _zoomValueLabel(nullptr), // (removed from UI)
        _unitBtnLabel(nullptr),
        _viewport(nullptr),
        _plane(nullptr),
        _centerMarker(nullptr),
        _zoom(1.0f),
        _useMiles(false),
        _lastRefreshMs(0),
        _planeSize(1800),
        _centerPx(_planeSize / 2),
        _didInitialCenter(false) {
    _nodes.reserve(64);
  }

  void release() override { this->~MapApp(); heap_caps_free(this); }

  void onStart(lv_obj_t* layout) override {
    _layout = layout;
    buildUi();
    refreshMap(true);
  }

  void onLoop() override {
    float pinchScale = 1.0f;
    if (grid::runtime::consumeMapPinchScale(pinchScale)) {
      if (pinchScale > 1.015f || pinchScale < 0.985f) {
        _zoom = std::max(0.01f, std::min(200000.0f, _zoom * pinchScale));
        refreshMap(false);
        return;
      }
    }

    const uint32_t now = millis();
    if (now - _lastRefreshMs < 2000) {
      return;
    }
    _lastRefreshMs = now;
    refreshMap(false);
  }

  void onClose() override {
    _nodes.clear();
    _layout = nullptr;
    _toolbar = nullptr;
    _hintLabel = nullptr;
    _scaleBar = nullptr;
    _scaleLabel = nullptr;
    _overlayZoomOutBtn = nullptr;
    _overlayZoomInBtn = nullptr;
    _zoomValueLabel = nullptr;
    _unitBtnLabel = nullptr;
    _viewport = nullptr;
    _plane = nullptr;
    _centerMarker = nullptr;
  }

  void onMessageReceived(MeshMessage) override {}

private:
  static void onZoomIn(lv_event_t* e) {
    auto* self = static_cast<MapApp*>(lv_event_get_user_data(e));
    if (self == nullptr) {
      return;
    }
    self->_zoom = std::min(200000.0f, self->_zoom * 1.25f);
    self->refreshMap(false);
  }

  static void onZoomOut(lv_event_t* e) {
    auto* self = static_cast<MapApp*>(lv_event_get_user_data(e));
    if (self == nullptr) {
      return;
    }
    self->_zoom = std::max(0.01f, self->_zoom / 1.25f);
    self->refreshMap(false);
  }

  static void onResetView(lv_event_t* e) {
    auto* self = static_cast<MapApp*>(lv_event_get_user_data(e));
    if (self == nullptr) {
      return;
    }
    self->_zoom = 1.0f;
    self->refreshMap(true);
  }

  static void onUnitToggle(lv_event_t* e) {
    auto* self = static_cast<MapApp*>(lv_event_get_user_data(e));
    if (self == nullptr) {
      return;
    }
    self->_useMiles = !self->_useMiles;
    self->refreshMap(false);
  }

  static void onNodeClicked(lv_event_t* e) {
    auto* self = static_cast<MapApp*>(lv_event_get_user_data(e));
    if (self == nullptr || self->_bridge == nullptr) {
      return;
    }

    lv_obj_t* marker = lv_event_get_target(e);
    for (const auto& node : self->_nodes) {
      if (node.marker != marker) {
        continue;
      }
      self->_bridge->requestContactDetails(node.id);
      WindowManager::instance().openApp("chat", false);
      return;
    }
  }

  void buildUi() {
    _toolbar = lv_obj_create(_layout);
    lv_obj_set_size(_toolbar, LV_PCT(100), 44);
    lv_obj_align(_toolbar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_radius(_toolbar, 0, 0);
    lv_obj_set_style_bg_color(_toolbar, lv_color_hex(0x0F141B), 0);
    lv_obj_set_style_border_width(_toolbar, 0, 0);
    lv_obj_set_style_pad_left(_toolbar, 8, 0);
    lv_obj_set_style_pad_right(_toolbar, 8, 0);
    lv_obj_set_style_pad_top(_toolbar, 6, 0);
    lv_obj_set_style_pad_bottom(_toolbar, 6, 0);
    lv_obj_clear_flag(_toolbar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(_toolbar);
    lv_label_set_text(title, "Relative MAP");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xE6EEF9), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);


    lv_obj_t* resetBtn = lv_btn_create(_toolbar);
    lv_obj_set_size(resetBtn, 46, 30);
    lv_obj_align(resetBtn, LV_ALIGN_RIGHT_MID, -40, 0);
    lv_obj_set_style_radius(resetBtn, 8, 0);
    lv_obj_add_event_cb(resetBtn, onResetView, LV_EVENT_CLICKED, this);
    lv_obj_t* resetLbl = lv_label_create(resetBtn);
    lv_label_set_text(resetLbl, "Reset");
    lv_obj_set_style_text_font(resetLbl, &lv_font_montserrat_10, 0);
    lv_obj_center(resetLbl);

    lv_obj_t* unitBtn = lv_btn_create(_toolbar);
    lv_obj_set_size(unitBtn, 38, 30);
    lv_obj_align(unitBtn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_radius(unitBtn, 8, 0);
    lv_obj_add_event_cb(unitBtn, onUnitToggle, LV_EVENT_CLICKED, this);
    _unitBtnLabel = lv_label_create(unitBtn);
    lv_label_set_text(_unitBtnLabel, "km");
    lv_obj_center(_unitBtnLabel);

    _viewport = lv_obj_create(_layout);
    lv_obj_set_size(_viewport, LV_PCT(100), LV_PCT(100));
    lv_obj_align(_viewport, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_pad_top(_viewport, 46, 0);
    lv_obj_set_style_pad_bottom(_viewport, 0, 0);
    lv_obj_set_style_pad_left(_viewport, 0, 0);
    lv_obj_set_style_pad_right(_viewport, 0, 0);
    lv_obj_set_style_bg_color(_viewport, lv_color_hex(0x0D1218), 0);
    lv_obj_set_style_border_width(_viewport, 0, 0);

    _plane = lv_obj_create(_viewport);
    lv_obj_set_size(_plane, _planeSize, _planeSize);
    lv_obj_align(_plane, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(_plane, lv_color_hex(0x0D1218), 0);
    lv_obj_set_style_border_width(_plane, 0, 0);
    lv_obj_set_scroll_dir(_plane, LV_DIR_ALL);
    lv_obj_set_scrollbar_mode(_plane, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(_plane, LV_OBJ_FLAG_SCROLLABLE);

    _hintLabel = lv_label_create(_viewport);
    lv_obj_set_style_text_font(_hintLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(_hintLabel, lv_color_hex(0xA8B8CA), 0);
    lv_obj_align(_hintLabel, LV_ALIGN_CENTER, 0, 14);
    lv_label_set_text(_hintLabel, "Waiting for location data...");

    lv_obj_set_scroll_dir(_viewport, LV_DIR_ALL);
    lv_obj_set_scrollbar_mode(_viewport, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_bg_opa(_viewport, LV_OPA_COVER, 0);

    _scaleBar = lv_obj_create(_layout);
    lv_obj_remove_style_all(_scaleBar);
    lv_obj_set_size(_scaleBar, 72, 3);
    lv_obj_align(_scaleBar, LV_ALIGN_BOTTOM_LEFT, 14, -10);
    lv_obj_set_style_bg_color(_scaleBar, lv_color_hex(0xDCE8F9), 0);
    lv_obj_set_style_bg_opa(_scaleBar, LV_OPA_80, 0);

    _scaleLabel = lv_label_create(_layout);
    lv_obj_set_style_text_font(_scaleLabel, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(_scaleLabel, lv_color_hex(0xDCE8F9), 0);
    lv_label_set_text(_scaleLabel, "");
    lv_obj_align_to(_scaleLabel, _scaleBar, LV_ALIGN_OUT_TOP_LEFT, 0, -4);

    _overlayZoomOutBtn = lv_btn_create(_layout);
    lv_obj_set_size(_overlayZoomOutBtn, 34, 34);
    lv_obj_align(_overlayZoomOutBtn, LV_ALIGN_BOTTOM_RIGHT, -98, -10);
    lv_obj_set_style_radius(_overlayZoomOutBtn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(_overlayZoomOutBtn, lv_color_hex(0x1A2734), 0);
    lv_obj_set_style_bg_opa(_overlayZoomOutBtn, LV_OPA_80, 0);
    lv_obj_set_style_border_width(_overlayZoomOutBtn, 1, 0);
    lv_obj_set_style_border_color(_overlayZoomOutBtn, lv_color_hex(0x57708A), 0);
    lv_obj_add_event_cb(_overlayZoomOutBtn, onZoomOut, LV_EVENT_CLICKED, this);
    lv_obj_t* overlayMinusLbl = lv_label_create(_overlayZoomOutBtn);
    lv_obj_set_style_text_color(overlayMinusLbl, lv_color_hex(0xDCE8F9), 0);
    lv_label_set_text(overlayMinusLbl, "-");
    lv_obj_center(overlayMinusLbl);

    _overlayZoomInBtn = lv_btn_create(_layout);
    lv_obj_set_size(_overlayZoomInBtn, 34, 34);
    lv_obj_align(_overlayZoomInBtn, LV_ALIGN_BOTTOM_RIGHT, -58, -10);
    lv_obj_set_style_radius(_overlayZoomInBtn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(_overlayZoomInBtn, lv_color_hex(0x1A2734), 0);
    lv_obj_set_style_bg_opa(_overlayZoomInBtn, LV_OPA_80, 0);
    lv_obj_set_style_border_width(_overlayZoomInBtn, 1, 0);
    lv_obj_set_style_border_color(_overlayZoomInBtn, lv_color_hex(0x57708A), 0);
    lv_obj_add_event_cb(_overlayZoomInBtn, onZoomIn, LV_EVENT_CLICKED, this);
    lv_obj_t* overlayPlusLbl = lv_label_create(_overlayZoomInBtn);
    lv_obj_set_style_text_color(overlayPlusLbl, lv_color_hex(0xDCE8F9), 0);
    lv_label_set_text(overlayPlusLbl, "+");
    lv_obj_center(overlayPlusLbl);

    lv_obj_move_foreground(_scaleBar);
    lv_obj_move_foreground(_scaleLabel);
    lv_obj_move_foreground(_overlayZoomOutBtn);
    lv_obj_move_foreground(_overlayZoomInBtn);
  }

  static double niceScaleValue(double raw) {
    if (raw <= 0.0) {
      return 0.0;
    }
    const double exp10 = floor(log10(raw));
    const double base = raw / pow(10.0, exp10);
    double step = 1.0;
    if (base < 1.5) {
      step = 1.0;
    } else if (base < 3.5) {
      step = 2.0;
    } else if (base < 7.5) {
      step = 5.0;
    } else {
      step = 10.0;
    }
    return step * pow(10.0, exp10);
  }

  void updateScaleOverlay(double pxPerKm) {
    if (_scaleBar == nullptr || _scaleLabel == nullptr) {
      return;
    }

    if (pxPerKm <= 0.0) {
      lv_obj_add_flag(_scaleBar, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(_scaleLabel, LV_OBJ_FLAG_HIDDEN);
      return;
    }

    constexpr double kTargetPx = 72.0;
    char label[24];
    int barPx = 72;

    if (_useMiles) {
      const double rawMi = (kTargetPx / pxPerKm) * 0.621371;
      const double niceMi = niceScaleValue(rawMi);
      const double kmForNice = niceMi / 0.621371;
      barPx = static_cast<int>(lround(kmForNice * pxPerKm));
      snprintf(label, sizeof(label), "%.2f mi", niceMi);
    } else {
      const double rawKm = kTargetPx / pxPerKm;
      const double niceKm = niceScaleValue(rawKm);
      barPx = static_cast<int>(lround(niceKm * pxPerKm));
      snprintf(label, sizeof(label), "%.2f km", niceKm);
    }

    barPx = std::max(32, std::min(140, barPx));
    lv_obj_set_size(_scaleBar, barPx, 3);
    lv_label_set_text(_scaleLabel, label);
    lv_obj_align(_scaleBar, LV_ALIGN_BOTTOM_LEFT, 14, -10);
    lv_obj_align_to(_scaleLabel, _scaleBar, LV_ALIGN_OUT_TOP_LEFT, 0, -4);
    lv_obj_clear_flag(_scaleBar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_scaleLabel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(_scaleBar);
    lv_obj_move_foreground(_scaleLabel);
    if (_overlayZoomOutBtn != nullptr) {
      lv_obj_move_foreground(_overlayZoomOutBtn);
    }
    if (_overlayZoomInBtn != nullptr) {
      lv_obj_move_foreground(_overlayZoomInBtn);
    }
  }

  void drawBackdrop() {
    if (_plane == nullptr) {
      return;
    }

    lv_obj_t* crossV = lv_obj_create(_plane);
    lv_obj_remove_style_all(crossV);
    lv_obj_set_size(crossV, 1, _planeSize - 80);
    lv_obj_align(crossV, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(crossV, lv_color_hex(0x233043), 0);
    lv_obj_set_style_bg_opa(crossV, LV_OPA_40, 0);

    lv_obj_t* crossH = lv_obj_create(_plane);
    lv_obj_remove_style_all(crossH);
    lv_obj_set_size(crossH, _planeSize - 80, 1);
    lv_obj_align(crossH, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(crossH, lv_color_hex(0x233043), 0);
    lv_obj_set_style_bg_opa(crossH, LV_OPA_40, 0);

    for (int i = 1; i <= 3; ++i) {
      lv_obj_t* ring = lv_obj_create(_plane);
      lv_obj_remove_style_all(ring);
      const lv_coord_t ringSize = static_cast<lv_coord_t>(i * 280);
      lv_obj_set_size(ring, ringSize, ringSize);
      lv_obj_align(ring, LV_ALIGN_CENTER, 0, 0);
      lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
      lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
      lv_obj_set_style_border_width(ring, 1, 0);
      lv_obj_set_style_border_color(ring, lv_color_hex(0x1B2737), 0);
      lv_obj_set_style_border_opa(ring, LV_OPA_20, 0);
    }

    _centerMarker = lv_obj_create(_plane);
    lv_obj_remove_style_all(_centerMarker);
    lv_obj_set_size(_centerMarker, 16, 16);
    lv_obj_set_style_radius(_centerMarker, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(_centerMarker, lv_color_hex(0x2B7FFF), 0);
    lv_obj_set_style_bg_opa(_centerMarker, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(_centerMarker, lv_color_hex(0xA9CCFF), 0);
    lv_obj_set_style_border_width(_centerMarker, 2, 0);
    lv_obj_align(_centerMarker, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t* meLabel = lv_label_create(_plane);
    lv_label_set_text(meLabel, "ME");
    lv_obj_set_style_text_font(meLabel, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(meLabel, lv_color_hex(0xA9CCFF), 0);
    lv_obj_align_to(meLabel, _centerMarker, LV_ALIGN_OUT_TOP_MID, 0, -4);
  }

  void updateToolbarText() {
    if (_zoomValueLabel != nullptr) {
      char zoomText[16];
      snprintf(zoomText, sizeof(zoomText), "%.2fx", static_cast<double>(_zoom));
      lv_label_set_text(_zoomValueLabel, zoomText);
    }
    if (_unitBtnLabel != nullptr) {
      lv_label_set_text(_unitBtnLabel, _useMiles ? "mi" : "km");
    }
  }

  void refreshMap(bool centerView) {
    if (_bridge == nullptr || _plane == nullptr || _viewport == nullptr) {
      return;
    }

    updateToolbarText();
    lv_obj_clean(_plane);
    _nodes.clear();
    drawBackdrop();

    const double myLat = sensors.node_lat;
    const double myLon = sensors.node_lon;
    if ((myLat == 0.0 && myLon == 0.0) || myLat > 90.0 || myLat < -90.0 || myLon > 180.0 || myLon < -180.0) {
      if (_hintLabel != nullptr) {
        lv_label_set_text(_hintLabel, "Set local GPS first to draw relative map");
        lv_obj_clear_flag(_hintLabel, LV_OBJ_FLAG_HIDDEN);
      }
      if (_scaleBar != nullptr) {
        lv_obj_add_flag(_scaleBar, LV_OBJ_FLAG_HIDDEN);
      }
      if (_scaleLabel != nullptr) {
        lv_obj_add_flag(_scaleLabel, LV_OBJ_FLAG_HIDDEN);
      }
      if (centerView || !_didInitialCenter) {
        centerViewport();
      }
      return;
    }

    auto contacts = _bridge->getContacts();
    const auto adverts = _bridge->getBootNodeAdverts();
    std::unordered_set<uint32_t> heardIds;
    heardIds.reserve(adverts.size());
    for (const auto& advert : adverts) {
      if (advert.contactId != 0) {
        heardIds.insert(advert.contactId);
      }
    }

    std::vector<MeshBridge::ContactSummary> valid;
    valid.reserve(std::min(contacts.size(), kMaxMapNodes));
    for (const auto& contact : contacts) {
      if (contact.gpsLat == 0 && contact.gpsLon == 0) {
        continue;
      }

      const bool heardNow = contact.heardRecently || (heardIds.find(contact.id) != heardIds.end());
      if (!heardNow) {
        continue;
      }

      const double lat = static_cast<double>(contact.gpsLat) / 1000000.0;
      const double lon = static_cast<double>(contact.gpsLon) / 1000000.0;
      if (lat > 90.0 || lat < -90.0 || lon > 180.0 || lon < -180.0) {
        continue;
      }
      valid.push_back(contact);

      if (valid.size() >= kMaxMapNodes) {
        break;
      }
    }

    if (valid.empty()) {
      if (_hintLabel != nullptr) {
        lv_label_set_text(_hintLabel, "No recently heard nodes with GPS adverts");
        lv_obj_clear_flag(_hintLabel, LV_OBJ_FLAG_HIDDEN);
      }
      if (_scaleBar != nullptr) {
        lv_obj_add_flag(_scaleBar, LV_OBJ_FLAG_HIDDEN);
      }
      if (_scaleLabel != nullptr) {
        lv_obj_add_flag(_scaleLabel, LV_OBJ_FLAG_HIDDEN);
      }
      if (centerView || !_didInitialCenter) {
        centerViewport();
      }
      return;
    }

    if (_hintLabel != nullptr) {
      lv_obj_add_flag(_hintLabel, LV_OBJ_FLAG_HIDDEN);
    }

    _nodes.resize(valid.size());
    double maxDistanceKm = 0.1;
    for (size_t i = 0; i < valid.size(); ++i) {
      const auto& contact = valid[i];
      const double lat = static_cast<double>(contact.gpsLat) / 1000000.0;
      const double lon = static_cast<double>(contact.gpsLon) / 1000000.0;
      const double distanceKm = haversineKm(myLat, myLon, lat, lon);
      const double bearing = bearingRad(myLat, myLon, lat, lon);

      _nodes[i].id = contact.id;
      _nodes[i].name = contact.name;
      _nodes[i].distanceKm = distanceKm;
      _nodes[i].bearing = bearing;
      _nodes[i].line = nullptr;
      _nodes[i].marker = nullptr;
      _nodes[i].nameLabel = nullptr;
      _nodes[i].distanceLabel = nullptr;
      if (distanceKm > maxDistanceKm) {
        maxDistanceKm = distanceKm;
      }
    }

    const double half = static_cast<double>(_centerPx - 56);
    const double basePxPerKm = half / maxDistanceKm;
    const double pxPerKm = std::max(0.2, basePxPerKm * _zoom);
    updateScaleOverlay(pxPerKm);
    const double visibleRadiusKm = half / pxPerKm;
    const double guideRadiusKm = visibleRadiusKm * 1.2;
    for (size_t i = 0; i < _nodes.size(); ++i) {
      auto& node = _nodes[i];
      const double xKm = node.distanceKm * sin(node.bearing);
      const double yKm = node.distanceKm * cos(node.bearing);
      const bool inView = node.distanceKm <= visibleRadiusKm;
      const bool guideOnly = !inView && node.distanceKm <= guideRadiusKm;
      if (!inView && !guideOnly) {
        continue;
      }

      double xPx = static_cast<double>(_centerPx) + xKm * pxPerKm;
      double yPx = static_cast<double>(_centerPx) - yKm * pxPerKm;
      if (guideOnly && node.distanceKm > 0.0) {
        const double scale = visibleRadiusKm / node.distanceKm;
        xPx = static_cast<double>(_centerPx) + xKm * pxPerKm * scale;
        yPx = static_cast<double>(_centerPx) - yKm * pxPerKm * scale;
      }
      const int x = static_cast<int>(std::lround(xPx));
      const int y = static_cast<int>(std::lround(yPx));

      node.linePts[0].x = _centerPx;
      node.linePts[0].y = _centerPx;
      node.linePts[1].x = static_cast<lv_coord_t>(x);
      node.linePts[1].y = static_cast<lv_coord_t>(y);

      node.line = lv_line_create(_plane);
      lv_line_set_points(node.line, node.linePts, 2);
      lv_obj_set_style_line_width(node.line, 1, 0);
      lv_obj_set_style_line_color(node.line, guideOnly ? lv_color_hex(0xB98A3D) : lv_color_hex(0x6E7F98), 0);
      lv_obj_set_style_line_opa(node.line, guideOnly ? LV_OPA_40 : LV_OPA_70, 0);

      if (inView) {
        node.marker = lv_btn_create(_plane);
        lv_obj_set_size(node.marker, 16, 16);
        lv_obj_set_style_radius(node.marker, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(node.marker, lv_color_hex(0xFFB74D), 0);
        lv_obj_set_style_bg_color(node.marker, lv_color_hex(0xF59E0B), LV_STATE_PRESSED);
        lv_obj_set_style_border_color(node.marker, lv_color_hex(0x4A2D00), 0);
        lv_obj_set_style_border_width(node.marker, 1, 0);
        lv_obj_set_style_pad_all(node.marker, 0, 0);
        lv_obj_set_pos(node.marker, x - 8, y - 8);
        lv_obj_add_event_cb(node.marker, onNodeClicked, LV_EVENT_CLICKED, this);

      } else {
        node.marker = lv_obj_create(_plane);
        lv_obj_remove_style_all(node.marker);
        lv_obj_set_size(node.marker, 6, 6);
        lv_obj_set_style_radius(node.marker, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(node.marker, lv_color_hex(0xB98A3D), 0);
        lv_obj_set_style_bg_opa(node.marker, LV_OPA_COVER, 0);
        lv_obj_set_pos(node.marker, x - 3, y - 3);
      }

      node.nameLabel = lv_label_create(_plane);
      lv_label_set_text(node.nameLabel, node.name.c_str());
      lv_obj_set_style_text_font(node.nameLabel, &lv_font_montserrat_12, 0);
      lv_obj_set_style_text_color(node.nameLabel,
                                  inView ? lv_color_hex(0xDDE8F8) : lv_color_hex(0xD5B277),
                                  0);
      lv_obj_align_to(node.nameLabel, node.marker, LV_ALIGN_OUT_TOP_MID, 0, -2);

      node.distanceLabel = lv_label_create(_plane);
      char distText[24];
      if (_useMiles) {
        snprintf(distText, sizeof(distText), "%.2f mi", node.distanceKm * 0.621371);
      } else {
        snprintf(distText, sizeof(distText), "%.2f km", node.distanceKm);
      }
      lv_label_set_text(node.distanceLabel, distText);
      lv_obj_set_style_text_font(node.distanceLabel, &lv_font_montserrat_10, 0);
      lv_obj_set_style_text_color(node.distanceLabel, lv_color_hex(0x95A8BF), 0);
      if (inView) {
        const int midX = (_centerPx + x) / 2;
        const int midY = (_centerPx + y) / 2;
        lv_obj_set_pos(node.distanceLabel, midX + 4, midY + 2);
      } else {
        lv_obj_set_pos(node.distanceLabel, x + 8, y + 2);
      }
    }

    if (centerView || !_didInitialCenter) {
      centerViewport();
    }
  }

  void centerViewport() {
    if (_viewport == nullptr) {
      return;
    }

    const lv_coord_t viewW = lv_obj_get_width(_viewport);
    const lv_coord_t viewH = lv_obj_get_height(_viewport);
    lv_obj_scroll_to(_viewport,
                     std::max<lv_coord_t>(0, _centerPx - viewW / 2),
                     std::max<lv_coord_t>(0, _centerPx - viewH / 2),
                     LV_ANIM_OFF);
    _didInitialCenter = true;
  }

  MeshBridge* _bridge;
  lv_obj_t* _layout;
  lv_obj_t* _toolbar;
  lv_obj_t* _hintLabel;
  lv_obj_t* _scaleBar;
  lv_obj_t* _scaleLabel;
  lv_obj_t* _overlayZoomOutBtn;
  lv_obj_t* _overlayZoomInBtn;
  lv_obj_t* _zoomValueLabel;
  lv_obj_t* _unitBtnLabel;
  lv_obj_t* _viewport;
  lv_obj_t* _plane;
  lv_obj_t* _centerMarker;
  std::vector<RelativeNode> _nodes;
  float _zoom;
  bool _useMiles;
  uint32_t _lastRefreshMs;
  const int _planeSize;
  const int _centerPx;
  bool _didInitialCenter;
};
}  // namespace

void registerMapApp(WindowManager& wm, MeshBridge& bridge) {
  wm.registerApp({"map", "MAP", LV_SYMBOL_IMAGE,
                  [&bridge]() -> MeshApp* { return WindowManager::createInPsram<MapApp>(&bridge); }});
}