#include "DriverHaptico.h"
#include "Config.h"
#include <Wire.h>

Adafruit_DRV2605 drv;

bool drv_escanear_i2c() {
  Wire.beginTransmission(DRV2605_ADDR);
  return (Wire.endTransmission() == 0);
}

bool drv_iniciar() {
  if (!drv.begin()) {
    return false;
  }
  drv.useLRA();
  drv.selectLibrary(6);  // biblioteca de efeitos LRA (efeitos built-in)
  // Coloca em standby: modo 0 + bit STANDBY (0x40)
  drv.writeRegister8(DRV_REG_MODE, 0x40);
  return true;
}

bool drv_calibrar() {
  // FEEDBACK: ERM_LRA=1, FB_BRAKE_FACTOR=3, LOOP_GAIN=1, BEMF_GAIN=0 (auto)
  // 1 011 01 00 = 0xB4
  drv.writeRegister8(DRV_REG_FEEDBACK, 0xB4);
  drv.writeRegister8(DRV_REG_RATED_V,  LRA_RATED_VOLTAGE_REG);
  drv.writeRegister8(DRV_REG_OD_CLAMP, LRA_OD_CLAMP_REG);

  // Modo auto-calibração e dispara GO
  drv.setMode(DRV2605_MODE_AUTOCAL);
  drv.writeRegister8(DRV_REG_GO, 0x01);

  // Aguarda GO zerar (indica fim da calibração), com timeout de 1,5 s
  uint32_t inicio = millis();
  while (drv.readRegister8(DRV_REG_GO) & 0x01) {
    if (millis() - inicio > 1500) {
      drv.writeRegister8(DRV_REG_GO, 0x00);  // cancela
      drv.useLRA();
      drv.selectLibrary(6);
      return false;
    }
    delay(10);
  }

  // DIAG_RESULT (bit 3 do STATUS): 0 = passou, 1 = falhou
  bool ok = !(drv.readRegister8(DRV_REG_STATUS) & 0x08);

  // Restaura configuração LRA para operação normal
  drv.useLRA();
  drv.selectLibrary(6);
  drv.writeRegister8(DRV_REG_MODE, 0x40);  // standby

  return ok;
}

void drv_set_rtp(uint8_t amplitude) {
  drv.setMode(DRV2605_MODE_REALTIME);
  drv.setRealtimeValue(amplitude);
}

void drv_parar() {
  drv.setRealtimeValue(0);
  drv.writeRegister8(DRV_REG_MODE, 0x40);  // standby
}
