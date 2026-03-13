#pragma once

#include <cstdint>

namespace grid::runtime {

uint32_t getScreenTimeoutSec();
void setScreenTimeoutSec(uint32_t sec);
void loadScreenTimeoutSec();
void saveScreenTimeoutSec();
bool consumeMapPinchScale(float& outScale);

}  // namespace grid::runtime
