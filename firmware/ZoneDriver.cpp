#include "ZoneDriver.h"
#include "DriverHaptico.h"
#include "MuxManager.h"
#include "ZoneMap.h"

static ZoneHardwareStatus statuses[EXUS_MAX_ZONES];
static bool muxPresent[EXUS_MAX_MUXES];
static uint32_t i2cFailures = 0;

static bool selectConfig(const ZoneConfig& config) {
  if (!mux_select_exclusive(config.muxAddress, config.muxChannel)) {
    ++i2cFailures;
    return false;
  }
  return true;
}

void zone_driver_begin() {
  for (uint8_t i = 0; i < EXUS_MAX_ZONES; ++i) statuses[i] = ZONE_DISABLED;
  for (uint8_t i = 0; i < EXUS_MAX_MUXES; ++i) muxPresent[i] = false;
  i2cFailures = 0;
  mux_begin();

  if (zone_map_uses_mux()) {
    for (uint8_t i = 0; i < EXUS_MAX_MUXES; ++i) {
      muxPresent[i] = mux_detect((uint8_t)(TCA9548A_FIRST_ADDR + i));
    }
  }

  for (uint8_t id = 0; id < zone_map_count(); ++id) {
    const ZoneConfig config = *zone_map_get(id);
    if (!config.enabled) continue;

    if (zone_map_uses_mux() && !muxPresent[zone_map_mux_number(id) - 1]) {
      statuses[id] = ZONE_MUX_MISSING;
      continue;
    }
    if (!selectConfig(config)) {
      statuses[id] = ZONE_I2C_FAILED;
      continue;
    }
    if (!drv_escanear_i2c()) {
      statuses[id] = ZONE_DRV_MISSING;
      continue;
    }
    if (!drv_iniciar(config.motor)) {
      statuses[id] = ZONE_I2C_FAILED;
      ++i2cFailures;
      continue;
    }
#if CALIBRATE_ON_BOOT
    if (config.motor.type == MOTOR_LRA && !drv_calibrar(config.motor)) {
      drv_parar();
#if ALLOW_UNCALIBRATED_ZONES
      statuses[id] = ZONE_READY_UNCALIBRATED;
      Serial.printf("[AVISO] zone=%u calibration failed; RTP limitado para bancada.\n", id);
#else
      statuses[id] = ZONE_CALIBRATION_FAILED;
#endif
      continue;
    }
#endif
    drv_parar();
    statuses[id] = ZONE_READY;
  }
  mux_disable_all();
}

bool zone_driver_select(uint8_t zoneId) {
  if (zoneId >= zone_map_count() || !zone_driver_ready(zoneId)) return false;
  const ZoneConfig config = *zone_map_get(zoneId);
  if (!selectConfig(config)) {
    statuses[zoneId] = ZONE_I2C_FAILED;
    return false;
  }
  return true;
}

bool zone_driver_set_rtp(uint8_t zoneId, uint8_t amplitude) {
  if (!zone_driver_select(zoneId)) return false;
  const ZoneConfig config = *zone_map_get(zoneId);
  drv_set_rtp(config.motor, amplitude);
  return true;
}

bool zone_driver_play_effect(uint8_t zoneId, uint8_t effect) {
  if (!zone_driver_select(zoneId) || effect < 1 || effect > 123) return false;
  const ZoneConfig config = *zone_map_get(zoneId);
  drv_tocar_efeito(config.motor, effect);
  return true;
}

bool zone_driver_stop(uint8_t zoneId) {
  if (zoneId >= zone_map_count()) return false;
  const ZoneHardwareStatus previous = statuses[zoneId];
  if (previous == ZONE_DISABLED || previous == ZONE_MUX_MISSING ||
      previous == ZONE_DRV_MISSING || previous == ZONE_CALIBRATION_FAILED) return false;
  const ZoneConfig config = *zone_map_get(zoneId);
  // Parada também tenta acessar uma zona marcada com falha transitória: não se
  // pode tornar o comando de segurança impossível por causa do primeiro NACK.
  if (!selectConfig(config)) return false;
  drv_parar();
  statuses[zoneId] = previous == ZONE_READY_UNCALIBRATED
    ? ZONE_READY_UNCALIBRATED : ZONE_READY;
  return true;
}

bool zone_driver_ready(uint8_t zoneId) {
  return zoneId < zone_map_count() &&
    (statuses[zoneId] == ZONE_READY || statuses[zoneId] == ZONE_READY_UNCALIBRATED);
}

bool zone_driver_uncalibrated(uint8_t zoneId) {
  return zoneId < zone_map_count() && statuses[zoneId] == ZONE_READY_UNCALIBRATED;
}

ZoneHardwareStatus zone_driver_status(uint8_t zoneId) {
  return zoneId < zone_map_count() ? statuses[zoneId] : ZONE_DISABLED;
}

const char* zone_driver_status_name(ZoneHardwareStatus status) {
  switch (status) {
    case ZONE_READY: return "READY";
    case ZONE_MUX_MISSING: return "MUX_MISSING";
    case ZONE_DRV_MISSING: return "DRV_MISSING";
    case ZONE_CALIBRATION_FAILED: return "CALIBRATION_FAILED";
    case ZONE_I2C_FAILED: return "I2C_FAILED";
    case ZONE_READY_UNCALIBRATED: return "READY_UNCALIBRATED";
    default: return "DISABLED";
  }
}

uint8_t zone_driver_ready_count() {
  uint8_t count = 0;
  for (uint8_t id = 0; id < zone_map_count(); ++id) if (zone_driver_ready(id)) ++count;
  return count;
}

bool zone_driver_mux_present(uint8_t muxNumber) {
  return muxNumber >= 1 && muxNumber <= EXUS_MAX_MUXES && muxPresent[muxNumber - 1];
}

uint32_t zone_driver_i2c_failures() {
  return i2cFailures;
}

void zone_driver_scan_report() {
  if (!zone_map_uses_mux()) {
    Serial.printf("zone=0 direct status=%s\n", zone_driver_status_name(statuses[0]));
    return;
  }
  for (uint8_t mux = 1; mux <= EXUS_MAX_MUXES; ++mux) {
    Serial.printf("mux=%u address=0x%02X status=%s\n", mux,
      TCA9548A_FIRST_ADDR + mux - 1, muxPresent[mux - 1] ? "PRESENT" : "MISSING");
    if (!muxPresent[mux - 1]) continue;
    for (uint8_t channel = 0; channel < 8; ++channel) {
      const uint8_t id = (uint8_t)((mux - 1) * 8 + channel);
      Serial.printf("  zone=%u channel=%u status=%s\n", id, channel,
        zone_driver_status_name(statuses[id]));
    }
  }
}
