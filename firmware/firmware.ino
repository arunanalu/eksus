// Exus — firmware multi-zona para ESP32-C3, TCA9548A e DRV2605L/LRA.

#include <Wire.h>
#include "BleTransport.h"
#include "Comandos.h"
#include "Config.h"
#include "MultiZoneScheduler.h"
#include "Seguranca.h"
#include "ZoneDriver.h"
#include "ZoneMap.h"
#include <esp_system.h>

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(600);
  Serial.println(F("\n=== Exus Firmware multi-zona ==="));
  Serial.printf("[INFO] Reset reason=%d\n", (int)esp_reset_reason());

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(I2C_CLOCK_HZ);
  Serial.printf("[INFO] I2C SDA=%d SCL=%d clock=%lu Hz\n",
    SDA_PIN, SCL_PIN, (unsigned long)I2C_CLOCK_HZ);
  Serial.printf("[INFO] Topologia=%s; descobrindo hardware e calibrando sequencialmente...\n",
    zone_map_uses_mux() ? "TCA9548A dinamico" : "direct_single_zone");

  scheduler_begin();
  // A pilha BLE sobe antes da calibracao: alimentado por bateria ou fonte, o
  // dispositivo ja pode ser descoberto enquanto o hardware haptico inicia.
  ble_transport_begin();
  zone_driver_begin();
  const uint8_t ready = zone_driver_ready_count();
  Serial.printf("[INFO] Descoberta concluida: %u de %u zonas prontas.\n",
    ready, zone_map_count());
  if (!ready) {
    Serial.println(F("[AVISO] Nenhuma zona pronta; comandos hapticos serao ignorados com seguranca."));
  }
  Serial.println(F("[OK] Digite 'zones' para diagnostico ou 'h' para ajuda."));
}

void loop() {
  comandos_processar();
  ble_transport_process();
  if (seguranca_emergencia_ativa()) {
    scheduler_stop_all();  // repete a parada se houve NACK transitório
    return;
  }
  scheduler_update();
}
