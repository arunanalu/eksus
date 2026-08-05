#include "Seguranca.h"
#include "Config.h"
#include "ZoneMap.h"

static bool emergency = false;
static uint32_t lastStop[EXUS_MAX_ZONES];
static bool hasStopped[EXUS_MAX_ZONES];
static uint32_t lastCommand = 0;

ParametrosValidados seguranca_validar_zona(uint8_t zoneId, float freq_hz,
  int intensity_pct, uint32_t durationMs, float dutyCycle) {
  ParametrosValidados result = {freq_hz, 0, durationMs, dutyCycle, false};
  const ZoneConfig* config = zone_map_get(zoneId);
  if (!config || !config->enabled || emergency) return result;

  result.freq_hz = constrain(freq_hz, MIN_FREQ_HZ, MAX_FREQ_HZ);
  const uint8_t maxPct = min((uint8_t)MAX_INTENSITY_PCT, config->limits.maxIntensityPct);
  intensity_pct = constrain(intensity_pct, 0, maxPct);
  const uint32_t maxDuration = min((uint32_t)MAX_DURATION_MS, config->limits.maxDurationMs);
  if (durationMs == 0 || durationMs > maxDuration) result.duracao_ms = maxDuration;
  result.duty_cycle = constrain(dutyCycle, 0.1f, 0.9f);
  result.amplitude = (uint8_t)((intensity_pct / 100.0f) * 127.0f);
  result.valido = true;
  return result;
}

ParametrosValidados seguranca_validar(float freq_hz, int intensity_pct,
  uint32_t duracao_ms, float duty_cycle) {
  return seguranca_validar_zona(0, freq_hz, intensity_pct, duracao_ms, duty_cycle);
}

void seguranca_emergencia_ativar() { emergency = true; }
void seguranca_emergencia_liberar() { emergency = false; }
bool seguranca_emergencia_ativa() { return emergency; }

bool seguranca_cooldown_zona_ok(uint8_t zoneId) {
  const ZoneConfig* config = zone_map_get(zoneId);
  if (!config) return false;
  return !hasStopped[zoneId] || millis() - lastStop[zoneId] >= config->limits.cooldownMs;
}

void seguranca_registrar_parada_zona(uint8_t zoneId) {
  if (zoneId >= EXUS_MAX_ZONES) return;
  lastStop[zoneId] = millis();
  hasStopped[zoneId] = true;
}

bool seguranca_rate_limit_ok() {
  return millis() - lastCommand >= MIN_CMD_INTERVAL_MS;
}

void seguranca_registrar_comando() { lastCommand = millis(); }
