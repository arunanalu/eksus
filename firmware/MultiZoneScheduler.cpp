#include "MultiZoneScheduler.h"
#include "Seguranca.h"
#include "ZoneDriver.h"
#include "ZoneMap.h"

static ZoneRuntimeState states[EXUS_MAX_ZONES];

static void clearState(uint8_t zoneId) {
  states[zoneId] = {EFFECT_NONE, false, 0, 0, 0, 0, 0, 0, 0, 0};
}

void scheduler_begin() {
  for (uint8_t id = 0; id < EXUS_MAX_ZONES; ++id) clearState(id);
}

uint8_t scheduler_active_count() {
  uint8_t count = 0;
  for (uint8_t id = 0; id < zone_map_count(); ++id) if (states[id].mode != EFFECT_NONE) ++count;
  return count;
}

uint16_t scheduler_amplitude_sum() {
  uint16_t sum = 0;
  for (uint8_t id = 0; id < zone_map_count(); ++id) {
    if (states[id].mode != EFFECT_NONE) sum += states[id].amplitude;
  }
  return sum;
}

static bool budgetAllows(uint8_t zoneId, uint8_t amplitude, uint8_t priority) {
  const ZoneRuntimeState& current = states[zoneId];
  if (current.mode != EFFECT_NONE && priority < current.priority) return false;
  uint8_t count = scheduler_active_count();
  uint16_t sum = scheduler_amplitude_sum();
  if (current.mode != EFFECT_NONE) {
    --count;
    sum -= current.amplitude;
  }
  return count < MAX_SIMULTANEOUS_ZONES && sum + amplitude <= MAX_GLOBAL_AMPLITUDE;
}

bool scheduler_start_pulse(uint8_t zoneId, float frequencyHz, int intensityPct,
  uint32_t durationMs, float dutyCycle, uint8_t priority) {
  if (!zone_driver_ready(zoneId) || !seguranca_cooldown_zona_ok(zoneId)) return false;
  ParametrosValidados p = seguranca_validar_zona(zoneId, frequencyHz, intensityPct,
    durationMs, dutyCycle);
  if (!p.valido || !budgetAllows(zoneId, p.amplitude, priority)) return false;
  if (states[zoneId].mode != EFFECT_NONE) scheduler_stop_zone(zoneId);
  if (!zone_driver_set_rtp(zoneId, p.amplitude)) return false;
  const uint32_t now = millis();
  states[zoneId] = {EFFECT_RTP, true, p.amplitude, priority, p.freq_hz,
    p.duty_cycle, now, now, p.duracao_ms, 0};
  return true;
}

bool scheduler_start_effect(uint8_t zoneId, uint8_t effect, uint8_t priority) {
  if (effect < 1 || effect > 123 || !zone_driver_ready(zoneId) ||
      !seguranca_cooldown_zona_ok(zoneId) || zone_driver_uncalibrated(zoneId)) return false;
  ParametrosValidados p = seguranca_validar_zona(zoneId, DEFAULT_FREQ_HZ,
    DEFAULT_INTENSITY_PCT, ROM_EFFECT_TIMEOUT_MS, DEFAULT_DUTY_CYCLE);
  if (!p.valido || !budgetAllows(zoneId, p.amplitude, priority)) return false;
  if (states[zoneId].mode != EFFECT_NONE) scheduler_stop_zone(zoneId);
  if (!zone_driver_play_effect(zoneId, effect)) return false;
  const uint32_t now = millis();
  states[zoneId] = {EFFECT_ROM, true, p.amplitude, priority, 0, 0,
    now, now, ROM_EFFECT_TIMEOUT_MS, 0};
  return true;
}

bool scheduler_stream_levels(const StreamLevel* levels, uint8_t count, float frequencyHz,
  uint32_t ttlMs, uint8_t priority) {
  if (!levels || count == 0 || count > EXUS_MAX_ZONES) return false;
  const uint8_t zoneCount = zone_map_count();
  bool selected[EXUS_MAX_ZONES] = {};
  ParametrosValidados validated[EXUS_MAX_ZONES];
  uint8_t activeCount = scheduler_active_count();
  uint16_t amplitudeSum = scheduler_amplitude_sum();

  // Valida o conjunto e seu orçamento antes de atualizar os drivers.
  for (uint8_t index = 0; index < count; ++index) {
    const StreamLevel& level = levels[index];
    if (level.zoneId >= zoneCount || level.intensityPct < 0 || selected[level.zoneId]) return false;
    selected[level.zoneId] = true;
    const ZoneRuntimeState& current = states[level.zoneId];
    if (current.mode != EFFECT_NONE) {
      if (priority < current.priority) return false;
      --activeCount;
      amplitudeSum -= current.amplitude;
    }
    if (level.intensityPct == 0) continue;
    if (!zone_driver_ready(level.zoneId)) return false;
    // Atualização de zona já ativa não entra em cooldown; uma zona que foi
    // realmente parada continua obedecendo ao intervalo de segurança.
    if (current.mode == EFFECT_NONE && !seguranca_cooldown_zona_ok(level.zoneId)) return false;
    validated[index] = seguranca_validar_zona(level.zoneId, frequencyHz, level.intensityPct,
      ttlMs, DEFAULT_DUTY_CYCLE);
    if (!validated[index].valido) return false;
    ++activeCount;
    amplitudeSum += validated[index].amplitude;
  }
  if (activeCount > MAX_SIMULTANEOUS_ZONES || amplitudeSum > MAX_GLOBAL_AMPLITUDE) return false;

  const uint32_t now = millis();
  for (uint8_t index = 0; index < count; ++index) {
    const StreamLevel& level = levels[index];
    if (level.intensityPct == 0) {
      scheduler_stop_zone(level.zoneId);
      continue;
    }
    const ParametrosValidados& p = validated[index];
    if (!zone_driver_set_rtp(level.zoneId, p.amplitude)) return false;
    // Ao contrário de pulse, não chama scheduler_stop_zone: preserva o fluxo
    // contínuo e não cria cooldown entre pacotes de atualização.
    states[level.zoneId] = {EFFECT_RTP, true, p.amplitude, priority, p.freq_hz,
      p.duty_cycle, now, now, p.duracao_ms, 0};
  }
  return true;
}

uint8_t scheduler_start_mux_pulse(uint8_t muxNumber, float frequencyHz,
  int intensityPct, uint32_t durationMs, uint8_t priority) {
  if (!zone_driver_mux_present(muxNumber)) return 0;
  uint8_t accepted = 0;
  const uint8_t first = (uint8_t)((muxNumber - 1) * 8);
  for (uint8_t id = first; id < first + 8; ++id) {
    if (scheduler_start_pulse(id, frequencyHz, intensityPct, durationMs,
      DEFAULT_DUTY_CYCLE, priority)) ++accepted;
  }
  return accepted;
}

uint8_t scheduler_start_mux_effect(uint8_t muxNumber, uint8_t effect, uint8_t priority) {
  if (!zone_driver_mux_present(muxNumber)) return 0;
  uint8_t accepted = 0;
  const uint8_t first = (uint8_t)((muxNumber - 1) * 8);
  for (uint8_t id = first; id < first + 8; ++id) {
    if (scheduler_start_effect(id, effect, priority)) ++accepted;
  }
  return accepted;
}

void scheduler_stop_zone(uint8_t zoneId) {
  if (zoneId >= zone_map_count() || states[zoneId].mode == EFFECT_NONE) return;
  if (!zone_driver_stop(zoneId)) return;  // mantém estado para nova tentativa
  clearState(zoneId);
  seguranca_registrar_parada_zona(zoneId);
}

void scheduler_stop_all() {
  for (uint8_t id = 0; id < zone_map_count(); ++id) scheduler_stop_zone(id);
}

void scheduler_stop_mask(uint64_t mask) {
  for (uint8_t id = 0; id < zone_map_count(); ++id) {
    if (mask & (UINT64_C(1) << id)) scheduler_stop_zone(id);
  }
}

void scheduler_update() {
  const uint32_t now = millis();
  for (uint8_t id = 0; id < zone_map_count(); ++id) {
    ZoneRuntimeState& state = states[id];
    if (state.mode == EFFECT_NONE) continue;
    if (state.durationMs > 0 && now - state.startedAt >= state.durationMs) {
      scheduler_stop_zone(id);
      continue;
    }
    if (state.mode != EFFECT_RTP) continue;

    const float periodMs = 1000.0f / state.frequencyHz;
    uint32_t phaseMs = (uint32_t)(periodMs * (state.motorOn ? state.dutyCycle : 1.0f - state.dutyCycle));
    if (phaseMs < 1) phaseMs = 1;
    const uint32_t elapsed = now - state.lastToggle;
    if (elapsed < phaseMs) continue;
    if (elapsed > phaseMs * 2) ++state.missedDeadlines;
    state.motorOn = !state.motorOn;
    if (!zone_driver_set_rtp(id, state.motorOn ? state.amplitude : 0)) {
      scheduler_stop_zone(id);
      continue;
    }
    state.lastToggle = now;
  }
}

bool scheduler_zone_active(uint8_t zoneId) {
  return zoneId < zone_map_count() && states[zoneId].mode != EFFECT_NONE;
}

const ZoneRuntimeState* scheduler_state(uint8_t zoneId) {
  return zoneId < zone_map_count() ? &states[zoneId] : nullptr;
}
