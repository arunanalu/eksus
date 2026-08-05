#include "MuxManager.h"
#include "Config.h"
#include <Wire.h>

static int8_t activeAddress = -1;
static int8_t activeChannel = -1;

static bool writeControl(uint8_t address, uint8_t value) {
  Wire.beginTransmission(address);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool mux_detect(uint8_t address) {
  if (address < TCA9548A_FIRST_ADDR || address > TCA9548A_LAST_ADDR) return false;
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

void mux_begin() {
  activeAddress = -1;
  activeChannel = -1;
  mux_disable_all();
}

void mux_disable_all() {
  for (uint8_t address = TCA9548A_FIRST_ADDR; address <= TCA9548A_LAST_ADDR; ++address) {
    if (mux_detect(address)) writeControl(address, 0x00);
  }
  activeAddress = -1;
  activeChannel = -1;
}

bool mux_select_exclusive(uint8_t address, uint8_t channel) {
  if (address == DIRECT_MUX_ADDRESS) {
    if (activeAddress >= 0) mux_disable_all();
    return true;
  }
  if (channel > 7 || !mux_detect(address)) return false;
  if (activeAddress == (int8_t)address && activeChannel == (int8_t)channel) return true;

  if (activeAddress >= 0 && !writeControl((uint8_t)activeAddress, 0x00)) {
    activeAddress = -1;
    activeChannel = -1;
    return false;
  }
  activeAddress = -1;
  activeChannel = -1;

  if (!writeControl(address, (uint8_t)(1U << channel))) return false;
  activeAddress = (int8_t)address;
  activeChannel = (int8_t)channel;
  return true;
}

int16_t mux_active_path() {
  if (activeAddress < 0) return -1;
  return ((int16_t)(uint8_t)activeAddress << 8) | (uint8_t)activeChannel;
}
