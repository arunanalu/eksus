#include "GeradorEnvelope.h"
#include "DriverHaptico.h"
#include "Seguranca.h"

// Estado interno do gerador de envelope
static ParametrosEnvelope _params;
static bool     _ativo         = false;
static bool     _motor_ligado  = false;
static uint32_t _ultimo_toggle = 0;
static uint32_t _inicio        = 0;

void envelope_iniciar(ParametrosEnvelope params) {
  _params        = params;
  _ativo         = true;
  _motor_ligado  = true;
  _inicio        = millis();
  _ultimo_toggle = millis();
  drv_set_rtp(params.amplitude);
}

void envelope_atualizar() {
  if (!_ativo) return;

  uint32_t agora = millis();

  // Encerra após a duração configurada (se duracao_ms > 0)
  if (_params.duracao_ms > 0 && (agora - _inicio) >= _params.duracao_ms) {
    envelope_parar();
    Serial.println(F("[INFO] Vibracao concluida."));
    return;
  }

  float    periodo_ms = 1000.0f / _params.freq_hz;
  uint32_t on_ms      = (uint32_t)(periodo_ms * _params.duty_cycle);
  uint32_t off_ms     = (uint32_t)(periodo_ms * (1.0f - _params.duty_cycle));

  // Garante pelo menos 1 ms em cada fase para não travar o loop
  if (on_ms  < 1) on_ms  = 1;
  if (off_ms < 1) off_ms = 1;

  uint32_t decorrido = agora - _ultimo_toggle;

  if (_motor_ligado && decorrido >= on_ms) {
    // Fase ON terminou → desliga
    drv_set_rtp(0);
    _motor_ligado  = false;
    _ultimo_toggle = agora;
  } else if (!_motor_ligado && decorrido >= off_ms) {
    // Fase OFF terminou → liga
    drv_set_rtp(_params.amplitude);
    _motor_ligado  = true;
    _ultimo_toggle = agora;
  }
}

void envelope_parar() {
  if (!_ativo) return;
  _ativo        = false;
  _motor_ligado = false;
  drv_parar();
  seguranca_registrar_parada();
}

bool envelope_ativo() {
  return _ativo;
}
