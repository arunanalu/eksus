#include "Seguranca.h"
#include "Config.h"

static bool     _emergencia     = false;
static uint32_t _ultima_parada  = 0;
static uint32_t _ultimo_cmd     = 0;
static bool     _primeira_ativacao = true;  // evita cooldown no primeiro uso

// Converte porcentagem de intensidade (0-100) para escala do DRV2605 (0-127),
// já respeitando o teto MAX_INTENSITY_PCT.
static uint8_t _pct_para_amplitude(int pct) {
  int limitado = constrain(pct, 0, MAX_INTENSITY_PCT);
  return (uint8_t)((limitado / 100.0f) * 127.0f);
}

ParametrosValidados seguranca_validar(float freq_hz, int intensity_pct,
                                      uint32_t duracao_ms, float duty_cycle) {
  ParametrosValidados r;
  r.valido = false;

  if (freq_hz < MIN_FREQ_HZ) {
    Serial.printf("[AVISO] Frequencia %.1f Hz abaixo do minimo (%.0f Hz). Ajustando.\n",
                  freq_hz, MIN_FREQ_HZ);
    freq_hz = MIN_FREQ_HZ;
  } else if (freq_hz > MAX_FREQ_HZ) {
    Serial.printf("[AVISO] Frequencia %.1f Hz acima do maximo (%.0f Hz). Ajustando.\n",
                  freq_hz, MAX_FREQ_HZ);
    freq_hz = MAX_FREQ_HZ;
  }

  if (intensity_pct > MAX_INTENSITY_PCT) {
    Serial.printf("[AVISO] Intensidade %d%% acima do limite (%d%%). Limitando.\n",
                  intensity_pct, MAX_INTENSITY_PCT);
    intensity_pct = MAX_INTENSITY_PCT;
  } else if (intensity_pct < 0) {
    intensity_pct = 0;
  }

  if (duracao_ms > MAX_DURATION_MS && duracao_ms != 0) {
    Serial.printf("[AVISO] Duracao %lu ms acima do limite (%d ms). Limitando.\n",
                  (unsigned long)duracao_ms, MAX_DURATION_MS);
    duracao_ms = MAX_DURATION_MS;
  }

  // Duty cycle: mantém entre 10% e 90% para garantir pulsos visíveis
  if (duty_cycle < 0.1f) duty_cycle = 0.1f;
  if (duty_cycle > 0.9f) duty_cycle = 0.9f;

  r.freq_hz    = freq_hz;
  r.amplitude  = _pct_para_amplitude(intensity_pct);
  r.duracao_ms = duracao_ms;
  r.duty_cycle = duty_cycle;
  r.valido     = true;
  return r;
}

void seguranca_emergencia_ativar() {
  _emergencia = true;
}

void seguranca_emergencia_liberar() {
  _emergencia = false;
}

bool seguranca_emergencia_ativa() {
  return _emergencia;
}

bool seguranca_cooldown_ok() {
  if (_primeira_ativacao) return true;
  return (millis() - _ultima_parada) >= MIN_COOLDOWN_MS;
}

void seguranca_registrar_parada() {
  _ultima_parada       = millis();
  _primeira_ativacao   = false;
}

bool seguranca_rate_limit_ok() {
  return (millis() - _ultimo_cmd) >= MIN_CMD_INTERVAL_MS;
}

void seguranca_registrar_comando() {
  _ultimo_cmd = millis();
}
