#include "GeradorEnvelope.h"
#include "MultiZoneScheduler.h"

void envelope_iniciar(ParametrosEnvelope params) {
  const int pct = (int)((params.amplitude / 127.0f) * 100.0f);
  scheduler_start_pulse(0, params.freq_hz, pct, params.duracao_ms, params.duty_cycle);
}

void envelope_atualizar() { scheduler_update(); }
void envelope_parar() { scheduler_stop_zone(0); }
bool envelope_ativo() { return scheduler_zone_active(0); }
