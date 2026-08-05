#pragma once
#include <Arduino.h>

struct ParametrosValidados {
  float freq_hz;
  uint8_t amplitude;
  uint32_t duracao_ms;
  float duty_cycle;
  bool valido;
};

ParametrosValidados seguranca_validar_zona(uint8_t zoneId, float freq_hz,
  int intensity_pct, uint32_t duracao_ms, float duty_cycle);
ParametrosValidados seguranca_validar(float freq_hz, int intensity_pct,
  uint32_t duracao_ms, float duty_cycle);
void seguranca_emergencia_ativar();
void seguranca_emergencia_liberar();
bool seguranca_emergencia_ativa();
bool seguranca_cooldown_zona_ok(uint8_t zoneId);
void seguranca_registrar_parada_zona(uint8_t zoneId);
bool seguranca_rate_limit_ok();
void seguranca_registrar_comando();
