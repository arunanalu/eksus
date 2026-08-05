#pragma once
#include <Arduino.h>
#include "Config.h"

enum ZoneHardwareStatus : uint8_t {
  ZONE_DISABLED,
  ZONE_MUX_MISSING,
  ZONE_DRV_MISSING,
  ZONE_CALIBRATION_FAILED,
  ZONE_READY,
  ZONE_I2C_FAILED
};

void zone_driver_begin();
bool zone_driver_select(uint8_t zoneId);
bool zone_driver_set_rtp(uint8_t zoneId, uint8_t amplitude);
bool zone_driver_play_effect(uint8_t zoneId, uint8_t effect);
bool zone_driver_stop(uint8_t zoneId);
bool zone_driver_ready(uint8_t zoneId);
ZoneHardwareStatus zone_driver_status(uint8_t zoneId);
const char* zone_driver_status_name(ZoneHardwareStatus status);
uint8_t zone_driver_ready_count();
bool zone_driver_mux_present(uint8_t muxNumber);
uint32_t zone_driver_i2c_failures();
void zone_driver_scan_report();

