#pragma once
#include <Arduino.h>
#include <Adafruit_DRV2605.h>

// Registradores do DRV2605 não expostos pela biblioteca Adafruit
#define DRV_REG_STATUS    0x00
#define DRV_REG_MODE      0x01
#define DRV_REG_RTPIN     0x02
#define DRV_REG_GO        0x0C
#define DRV_REG_RATED_V   0x16
#define DRV_REG_OD_CLAMP  0x17
#define DRV_REG_FEEDBACK  0x1A
#define DRV_REG_CTRL3     0x1D

// Instância global do driver (definida em DriverHaptico.cpp)
extern Adafruit_DRV2605 drv;

// Varre o barramento I2C procurando 0x5A.
// Retorna true se o DRV2605 respondeu.
bool drv_escanear_i2c();

// Inicializa a biblioteca e configura o chip para modo LRA.
// Deve ser chamado em setup() após Wire.begin().
// Retorna true em sucesso.
bool drv_iniciar();

// Executa a rotina de auto-calibração do DRV2605 para o motor LRA.
// Bloqueia até 1,5 s (chamada somente em setup()).
// Retorna true se o chip reportou calibração bem-sucedida.
bool drv_calibrar();

// Define a amplitude em modo RTP (Real-Time Playback).
// amplitude: 0 = motor parado, 1-127 = intensidade crescente.
void drv_set_rtp(uint8_t amplitude);

// Para o motor imediatamente e coloca o chip em standby.
void drv_parar();
