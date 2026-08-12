#include "ZoneMap.h"

static ZoneConfig generated;

uint8_t zone_map_count() {
  return EXUS_USE_TCA9548A ? EXUS_MAX_ZONES : 1;
}

const ZoneConfig* zone_map_get(uint8_t id) {
  if (id >= zone_map_count()) return nullptr;

  generated.id = id;
  generated.muxAddress = EXUS_USE_TCA9548A
    ? (uint8_t)(TCA9548A_FIRST_ADDR + id / 8)
    : DIRECT_MUX_ADDRESS;
  generated.muxChannel = EXUS_USE_TCA9548A ? (uint8_t)(id % 8) : 0;
  generated.position = (ZonePosition)(id % 8);
  // Mux 1: zonas 0 e 1 usam ERM moeda; zona 2 usa LRA bastao 0619AAC.
  // A topologia direta preserva o perfil LRA generico da SPEC-001.
  if (EXUS_USE_TCA9548A && (id == 0 || id == 1)) {
    generated.motor = {MOTOR_ERM, ERM_COIN_RATED_VOLTAGE_REG,
      ERM_COIN_OD_CLAMP_REG, 0};
  } else if (EXUS_USE_TCA9548A && id == 2) {
    generated.motor = {MOTOR_LRA, BAR_LRA_RATED_VOLTAGE_REG,
      BAR_LRA_OD_CLAMP_REG, BAR_LRA_DRIVE_TIME_REG};
  } else {
    generated.motor = {MOTOR_LRA, LRA_RATED_VOLTAGE_REG,
      LRA_OD_CLAMP_REG, LRA_DRIVE_TIME_REG};
  }
  generated.limits = {MAX_INTENSITY_PCT, MAX_DURATION_MS, MIN_COOLDOWN_MS};
  generated.enabled = true;
  return &generated;
}

bool zone_map_uses_mux() {
  return EXUS_USE_TCA9548A != 0;
}

uint8_t zone_map_mux_number(uint8_t id) {
  if (!EXUS_USE_TCA9548A || id >= zone_map_count()) return 0;
  return (uint8_t)(id / 8 + 1);
}
