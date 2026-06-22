#pragma once
#include <Arduino.h>

// Resultado de seguranca_validar(): parâmetros revisados e prontos para uso.
struct ParametrosValidados {
  float    freq_hz;
  uint8_t  amplitude;    // mapeado para 0-127
  uint32_t duracao_ms;
  float    duty_cycle;
  bool     valido;
};

// Valida e recorta os parâmetros recebidos, aplicando os limites de Config.h.
// Imprime avisos no Serial para cada parâmetro ajustado.
ParametrosValidados seguranca_validar(float freq_hz, int intensity_pct,
                                      uint32_t duracao_ms, float duty_cycle);

// Ativa a parada de emergência (bloqueia novos acionamentos).
void seguranca_emergencia_ativar();

// Libera a parada de emergência.
void seguranca_emergencia_liberar();

// Retorna true enquanto a parada de emergência está ativa.
bool seguranca_emergencia_ativa();

// Retorna true se o cooldown mínimo entre ativações já passou.
bool seguranca_cooldown_ok();

// Registra o instante de parada (inicia o cooldown).
void seguranca_registrar_parada();

// Retorna true se já passou o intervalo mínimo entre comandos (rate limit).
bool seguranca_rate_limit_ok();

// Registra o instante do último comando (atualiza o rate limit).
void seguranca_registrar_comando();
