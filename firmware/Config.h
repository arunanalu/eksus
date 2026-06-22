#pragma once

// =========================================================
// Config.h — Constantes editáveis do projeto Exus
// Altere aqui para ajustar o comportamento sem tocar no resto do código.
// =========================================================

// --- Pinos I2C do ESP32-C3 ---
#define SDA_PIN  8
#define SCL_PIN  9

// --- Endereço I2C do DRV2605 (fixo por hardware) ---
#define DRV2605_ADDR  0x5A

// --- Parâmetros de calibração do motor LRA ---
// IMPORTANTE: Esses valores dependem do motor específico.
// Consulte o datasheet do seu motor LRA e atualize abaixo.
//
// LRA_RATED_VOLTAGE_REG: tensão nominal RMS do motor em valor de registrador.
// Estimativa simples: (V_rms / 1.8) * 90
// Exemplos comuns:  0x3E (~1.8 V rms)  0x50 (~2.0 V rms)  0x64 (~2.5 V rms)
#define LRA_RATED_VOLTAGE_REG   0x3E

// LRA_OD_CLAMP_REG: tensão de pico máxima (overdrive clamp).
// Tipicamente 1.5x a 2x a tensão nominal.
// Exemplos comuns:  0x89 (~3.2 V)  0x8C (~3.4 V)  0xA4 (~4.0 V)
#define LRA_OD_CLAMP_REG        0x89

// --- Limites de segurança ---
// Comece conservador; aumente só após validar conforto.
#define MAX_INTENSITY_PCT     50    // % máxima de intensidade (0-100)
#define MAX_DURATION_MS       5000  // duração máxima por ativação (ms)
#define MIN_COOLDOWN_MS       300   // pausa mínima entre ativações (ms)
#define MIN_CMD_INTERVAL_MS   100   // intervalo mínimo entre comandos (ms)

// --- Limites de frequência percebida ---
#define MIN_FREQ_HZ   1.0f
#define MAX_FREQ_HZ   100.0f

// --- Parâmetros padrão do envelope ---
#define DEFAULT_FREQ_HZ         30.0f
#define DEFAULT_INTENSITY_PCT   30
#define DEFAULT_DURATION_MS     1000
#define DEFAULT_DUTY_CYCLE      0.5f   // 50% ligado, 50% desligado

// --- Serial ---
#define SERIAL_BAUD  115200
