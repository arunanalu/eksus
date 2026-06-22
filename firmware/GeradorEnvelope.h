#pragma once
#include <Arduino.h>

struct ParametrosEnvelope {
  float    freq_hz;     // frequência percebida (Hz) — controla o ritmo de pulsos
  uint8_t  amplitude;   // intensidade mapeada para 0-127 (escala do DRV2605)
  uint32_t duracao_ms;  // duração total: 0 = contínuo até envelope_parar()
  float    duty_cycle;  // fração de tempo ligado por ciclo (0.0–1.0)
};

// Inicia uma sequência de pulsos com os parâmetros fornecidos.
void envelope_iniciar(ParametrosEnvelope params);

// Atualiza o estado do envelope. Deve ser chamado a cada iteração do loop().
// Não usa delay() — usa millis() para temporização não-bloqueante.
void envelope_atualizar();

// Para o envelope imediatamente e desliga o motor.
void envelope_parar();

// Retorna true se o envelope está ativo (motor pulsando ou em pausa de ciclo).
bool envelope_ativo();
