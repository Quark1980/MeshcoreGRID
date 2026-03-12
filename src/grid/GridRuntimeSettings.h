#pragma once

#include <cstdint>

namespace grid::runtime {

uint32_t getScreenTimeoutSec();
void setScreenTimeoutSec(uint32_t sec);
void loadScreenTimeoutSec();
void saveScreenTimeoutSec();

}  // namespace grid::runtime
