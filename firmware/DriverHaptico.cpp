#include "DriverHaptico.h"
#include "Config.h"
#include <Adafruit_DRV2605.h>
#include <Wire.h>

static Adafruit_DRV2605 drv;

static void configureLra(uint8_t ratedVoltage, uint8_t overdriveClamp) {
  drv.useLRA();
  drv.selectLibrary(6);
  drv.writeRegister8(DRV_REG_RATED_V, ratedVoltage);
  drv.writeRegister8(DRV_REG_OD_CLAMP, overdriveClamp);
  drv.writeRegister8(DRV_REG_MODE, 0x40);
}

bool drv_escanear_i2c() {
  Wire.beginTransmission(DRV2605_ADDR);
  return Wire.endTransmission() == 0;
}

bool drv_iniciar(uint8_t ratedVoltage, uint8_t overdriveClamp) {
  if (!drv.begin()) return false;
  configureLra(ratedVoltage, overdriveClamp);
  return true;
}

bool drv_calibrar(uint8_t ratedVoltage, uint8_t overdriveClamp) {
  drv.writeRegister8(DRV_REG_FEEDBACK, 0xB4);
  drv.writeRegister8(DRV_REG_RATED_V, ratedVoltage);
  drv.writeRegister8(DRV_REG_OD_CLAMP, overdriveClamp);
  drv.setMode(DRV2605_MODE_AUTOCAL);
  drv.writeRegister8(DRV_REG_GO, 0x01);

  const uint32_t inicio = millis();
  while (drv.readRegister8(DRV_REG_GO) & 0x01) {
    if (millis() - inicio > 1500) {
      drv.writeRegister8(DRV_REG_GO, 0x00);
      configureLra(ratedVoltage, overdriveClamp);
      return false;
    }
    delay(10);  // permitido somente durante a calibração sequencial do boot
  }

  const bool ok = !(drv.readRegister8(DRV_REG_STATUS) & 0x08);
  configureLra(ratedVoltage, overdriveClamp);
  return ok;
}

void drv_set_rtp(uint8_t amplitude) {
  drv.setMode(DRV2605_MODE_REALTIME);
  drv.setRealtimeValue(amplitude);
}

void drv_tocar_efeito(uint8_t effect) {
  drv.selectLibrary(6);
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
