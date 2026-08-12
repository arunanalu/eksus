#include "DriverHaptico.h"
#include "Config.h"
#include <Adafruit_DRV2605.h>
#include <Wire.h>

static Adafruit_DRV2605 drv;

static void configureMotor(const MotorConfig& motor) {
  drv.writeRegister8(DRV_REG_RATED_V, motor.ratedVoltage);
  drv.writeRegister8(DRV_REG_OD_CLAMP, motor.overdriveClamp);
  if (motor.type == MOTOR_LRA) {
    drv.useLRA();
    drv.selectLibrary(6);
    const uint8_t control1 = drv.readRegister8(DRV_REG_CONTROL1);
    drv.writeRegister8(DRV_REG_CONTROL1, (control1 & 0xE0) | (motor.lraDriveTime & 0x1F));
    const uint8_t control3 = drv.readRegister8(DRV_REG_CONTROL3);
    drv.writeRegister8(DRV_REG_CONTROL3, control3 & (uint8_t)~0x21);
  } else {
    drv.useERM();
    drv.selectLibrary(1);
    const uint8_t control3 = drv.readRegister8(DRV_REG_CONTROL3);
    drv.writeRegister8(DRV_REG_CONTROL3, control3 | 0x20);  // ERM open-loop
  }
  drv.writeRegister8(DRV_REG_MODE, 0x40);
}

bool drv_escanear_i2c() {
  Wire.beginTransmission(DRV2605_ADDR);
  return Wire.endTransmission() == 0;
}

bool drv_iniciar(const MotorConfig& motor) {
  if (!drv.begin()) return false;
  configureMotor(motor);
  return true;
}

bool drv_calibrar(const MotorConfig& motor) {
  if (motor.type != MOTOR_LRA) return true;
  drv.writeRegister8(DRV_REG_FEEDBACK, 0xB4);
  drv.writeRegister8(DRV_REG_RATED_V, motor.ratedVoltage);
  drv.writeRegister8(DRV_REG_OD_CLAMP, motor.overdriveClamp);
  drv.setMode(DRV2605_MODE_AUTOCAL);
  drv.writeRegister8(DRV_REG_GO, 0x01);

  const uint32_t inicio = millis();
  while (drv.readRegister8(DRV_REG_GO) & 0x01) {
    if (millis() - inicio > 1500) {
      drv.writeRegister8(DRV_REG_GO, 0x00);
      configureMotor(motor);
      return false;
    }
    delay(10);  // permitido somente durante a calibração sequencial do boot
  }

  const bool ok = !(drv.readRegister8(DRV_REG_STATUS) & 0x08);
  configureMotor(motor);
  return ok;
}

void drv_set_rtp(const MotorConfig& motor, uint8_t amplitude) {
  drv.setMode(DRV2605_MODE_REALTIME);
  // ERM em modo aberto usa RTP bidirecional: 0x80 e' repouso, e 0xFF e'
  // acionamento positivo maximo. LRA em malha fechada mantem 0 como repouso.
  const uint8_t rtp = motor.type == MOTOR_ERM
    ? (amplitude ? (uint8_t)(0x80 + amplitude) : 0x80)
    : amplitude;
  drv.setRealtimeValue(rtp);
}

void drv_tocar_efeito(const MotorConfig& motor, uint8_t effect) {
  drv.selectLibrary(motor.type == MOTOR_LRA ? 6 : 1);
  drv.setMode(DRV2605_MODE_INTTRIG);
  drv.setWaveform(0, effect);
  drv.setWaveform(1, 0);
  drv.go();
}

void drv_parar() {
  drv.setRealtimeValue(0);
  drv.writeRegister8(DRV_REG_GO, 0x00);
  drv.writeRegister8(DRV_REG_MODE, 0x40);
}
