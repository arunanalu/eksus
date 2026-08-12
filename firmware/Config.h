#pragma once

// Configuração geral do firmware Exus.

#define SDA_PIN 8
#define SCL_PIN 9
#define I2C_CLOCK_HZ 400000UL

#define DRV2605_ADDR 0x5A
#define TCA9548A_FIRST_ADDR 0x70
#define TCA9548A_LAST_ADDR 0x77
#define DIRECT_MUX_ADDRESS 0xFF

// 0 preserva o hardware da SPEC-001 (DRV ligado diretamente ao ESP32).
// 1 descobre automaticamente TCAs em 0x70..0x77 e os DRVs de cada canal.
#ifndef EXUS_USE_TCA9548A
#define EXUS_USE_TCA9548A 0
#endif

#define EXUS_MAX_MUXES 8
#define EXUS_MAX_ZONES 64
#define CALIBRATE_ON_BOOT 1

// Somente bancada: mantem uma zona utilizavel apos falha de auto-calibracao,
// com limites RTP conservadores. Mude para 0 antes de teste na mascara/corpo.
#define ALLOW_UNCALIBRATED_ZONES 1
#define UNCALIBRATED_MAX_INTENSITY_PCT 15
#define UNCALIBRATED_MAX_DURATION_MS 500UL

// Perfil ERM 1020: moeda 10 x 2 mm, 3 V, 10.000 rpm. Em modo aberto, o
// DRV2605L usa OD_CLAMP como referencia de amplitude; RATED_VOLTAGE e' ignorado.
#define ERM_COIN_RATED_VOLTAGE_REG 0x8E
#define ERM_COIN_OD_CLAMP_REG      0x8B

// Perfil LRA 0619AAC: 1,2 Vrms, 170 +/- 5 Hz. Os valores assumem SAMPLE_TIME
// padrao de 300 us: RATED=0x32 (~1,2 Vrms), CLAMP=0x50 (~1,70 Vp, conservador)
// e DRIVE_TIME=0x18 (~2,9 ms, proximo de metade do periodo a 170 Hz).
#define BAR_LRA_RATED_VOLTAGE_REG 0x32
#define BAR_LRA_OD_CLAMP_REG      0x50
#define BAR_LRA_DRIVE_TIME_REG    0x18

// Perfil de reserva LRA para zonas ainda nao mapeadas abaixo.
#define LRA_RATED_VOLTAGE_REG 0x3E
#define LRA_OD_CLAMP_REG 0x89
#define LRA_DRIVE_TIME_REG 0x13

// Limites globais. Os limites mais restritivos do mapa de zona sempre vencem.
#define MAX_INTENSITY_PCT 50
#define MAX_DURATION_MS 5000UL
#define MIN_COOLDOWN_MS 300UL
#define MIN_CMD_INTERVAL_MS 100UL
#define MAX_SIMULTANEOUS_ZONES 8
#define MAX_GLOBAL_AMPLITUDE 320

#define MIN_FREQ_HZ 1.0f
#define MAX_FREQ_HZ 100.0f
#define DEFAULT_FREQ_HZ 30.0f
#define DEFAULT_INTENSITY_PCT 30
#define DEFAULT_DURATION_MS 1000UL
#define DEFAULT_DUTY_CYCLE 0.5f
#define ROM_EFFECT_TIMEOUT_MS 1500UL

#define SERIAL_BAUD 115200
#define SERIAL_BUFFER_SIZE 128
