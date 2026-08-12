#pragma once
#include <Arduino.h>
#include "ZoneMap.h"

#define DRV_REG_STATUS 0x00
#define DRV_REG_MODE 0x01
#define DRV_REG_GO 0x0C
#define DRV_REG_RATED_V 0x16
#define DRV_REG_OD_CLAMP 0x17
#define DRV_REG_FEEDBACK 0x1A
#define DRV_REG_CONTROL1 0x1B
#define DRV_REG_CONTROL3 0x1D

// API de baixo nível. ZoneDriver é o único módulo que deve chamá-la.
bool drv_escanear_i2c();
bool drv_iniciar(const MotorConfig& motor);
bool drv_calibrar(const MotorConfig& motor);
void drv_set_rtp(const MotorConfig& motor, uint8_t amplitude);
void drv_tocar_efeito(const MotorConfig& motor, uint8_t effect);
void drv_parar();
