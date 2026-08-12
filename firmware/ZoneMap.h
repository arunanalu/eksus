#pragma once
#include <Arduino.h>
#include "Config.h"

enum ZonePosition : uint8_t {
  ZONE_TESTA,
  ZONE_TEMPORA_ESQ,
  ZONE_TEMPORA_DIR,
  ZONE_BOCHECHA_ESQ,
  ZONE_BOCHECHA_DIR,
  ZONE_MANDIBULA_ESQ,
  ZONE_MANDIBULA_DIR,
  ZONE_NUCA
};

enum MotorType : uint8_t {
  MOTOR_ERM,
  MOTOR_LRA
};

struct MotorConfig {
  MotorType type;
  uint8_t ratedVoltage;
  uint8_t overdriveClamp;
  uint8_t lraDriveTime;
};

struct ZoneSafetyLimits {
  uint8_t maxIntensityPct;
  uint32_t maxDurationMs;
  uint32_t cooldownMs;
};

struct ZoneConfig {
  uint8_t id;
  uint8_t muxAddress;
  uint8_t muxChannel;
  ZonePosition position;
  MotorConfig motor;
  ZoneSafetyLimits limits;
  bool enabled;
};

uint8_t zone_map_count();
const ZoneConfig* zone_map_get(uint8_t id);
bool zone_map_uses_mux();
uint8_t zone_map_mux_number(uint8_t id);  // 1..8; 0 para ligação direta
