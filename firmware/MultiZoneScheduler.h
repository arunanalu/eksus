#pragma once
#include <Arduino.h>
#include "Config.h"

enum ZoneEffectMode : uint8_t { EFFECT_NONE, EFFECT_RTP, EFFECT_ROM };

struct ZoneRuntimeState {
  ZoneEffectMode mode;
  bool motorOn;
  uint8_t amplitude;
  uint8_t priority;
  float frequencyHz;
  float dutyCycle;
  uint32_t startedAt;
  uint32_t lastToggle;
  uint32_t durationMs;
  uint32_t missedDeadlines;
};

// Atualiza múltiplas zonas RTP sem reiniciar o cooldown. Usado por fluxos
// contínuos cujo TTL expira caso o host deixe de enviar atualizações.
struct StreamLevel {
  uint8_t zoneId;
  int intensityPct;
};

void scheduler_begin();
void scheduler_update();
bool scheduler_start_pulse(uint8_t zoneId, float frequencyHz, int intensityPct,
  uint32_t durationMs, float dutyCycle = DEFAULT_DUTY_CYCLE, uint8_t priority = 50);
bool scheduler_start_effect(uint8_t zoneId, uint8_t effect, uint8_t priority = 50);
bool scheduler_stream_levels(const StreamLevel* levels, uint8_t count, float frequencyHz,
  uint32_t ttlMs, uint8_t priority = 50);
uint8_t scheduler_start_mux_pulse(uint8_t muxNumber, float frequencyHz,
  int intensityPct, uint32_t durationMs, uint8_t priority = 50);
uint8_t scheduler_start_mux_effect(uint8_t muxNumber, uint8_t effect, uint8_t priority = 50);
void scheduler_stop_zone(uint8_t zoneId);
void scheduler_stop_all();
void scheduler_stop_mask(uint64_t mask);
bool scheduler_zone_active(uint8_t zoneId);
const ZoneRuntimeState* scheduler_state(uint8_t zoneId);
uint8_t scheduler_active_count();
uint16_t scheduler_amplitude_sum();
