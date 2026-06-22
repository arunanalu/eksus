// =========================================================
// firmware.ino — Projeto Exus | ESP32-C3 + DRV2605 + LRA
//
// Estrutura:
//   Config.h          — constantes editáveis
//   DriverHaptico     — comunicação com o DRV2605 via I2C
//   GeradorEnvelope   — pulsos não-bloqueantes (frequência percebida)
//   Seguranca         — limites de intensidade, duração, cooldown, emergência
//   Comandos          — interface USB Serial
// =========================================================

#include <Wire.h>
#include <Adafruit_DRV2605.h>
#include "Config.h"
#include "DriverHaptico.h"
#include "GeradorEnvelope.h"
#include "Seguranca.h"
#include "Comandos.h"

// =========================================================
// setup() — roda uma vez ao ligar ou após reset
// =========================================================
void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(600);  // tempo para USB CDC estabilizar

  Serial.println(F("\n=========================================="));
  Serial.println(F("  Exus Firmware — ESP32-C3 + DRV2605 LRA  "));
  Serial.println(F("==========================================\n"));

  // 1. Iniciar I2C nos pinos configurados em Config.h
  Wire.begin(SDA_PIN, SCL_PIN);
  Serial.printf("[INFO] I2C iniciado: SDA=GPIO%d  SCL=GPIO%d\n", SDA_PIN, SCL_PIN);

  // 2. Verificar presença do DRV2605 (Teste da Secao 10.1 do plano)
  Serial.print(F("[INFO] Procurando DRV2605 em 0x5A... "));
  if (!drv_escanear_i2c()) {
    Serial.println(F("NAO ENCONTRADO"));
    Serial.println(F("[ERRO] Verifique: GND comum, SDA/SCL trocados, VIN=3.3V."));
    Serial.println(F("[ERRO] Sistema bloqueado. Corrija o hardware e reinicie."));
    seguranca_emergencia_ativar();
    while (true) { delay(1000); }
  }
  Serial.println(F("OK"));

  // 3. Inicializar biblioteca e configurar para LRA
  Serial.print(F("[INFO] Inicializando DRV2605... "));
  if (!drv_iniciar()) {
    Serial.println(F("FALHA"));
    Serial.println(F("[ERRO] Biblioteca nao inicializou. Reinicie."));
    seguranca_emergencia_ativar();
    while (true) { delay(1000); }
  }
  Serial.println(F("OK"));

  // 4. Auto-calibração do motor LRA
  Serial.print(F("[INFO] Calibrando motor LRA (ate 1.5s)... "));
  bool cal_ok = drv_calibrar();
  if (cal_ok) {
    Serial.println(F("OK"));
  } else {
    Serial.println(F("AVISO — calibracao retornou erro"));
    Serial.println(F("[AVISO] O motor ainda pode funcionar com os valores default."));
    Serial.println(F("[AVISO] Para melhorar: ajuste LRA_RATED_VOLTAGE_REG e"));
    Serial.println(F("[AVISO] LRA_OD_CLAMP_REG em Config.h com dados do datasheet."));
  }

  // 5. Pronto — motor em standby
  drv_parar();
  Serial.println(F("\n[OK] Sistema pronto. Digite 'h' para ver os comandos."));
  Serial.println(F("------------------------------------------\n"));
}

// =========================================================
// loop() — roda continuamente
// =========================================================
void loop() {
  // Comandos sempre processados (inclui 'e' e 'r' mesmo em emergência)
  comandos_processar();

  // Emergência: garante motor parado e exibe lembrete periódico
  if (seguranca_emergencia_ativa()) {
    envelope_parar();
    drv_parar();

    static uint32_t t_msg = 0;
    if (millis() - t_msg >= 4000) {
      Serial.println(F("[EMERGENCIA] Sistema parado. Digite 'r' + Enter para retomar."));
      t_msg = millis();
    }
    return;
  }

  // Atualiza o gerador de envelope (pulsos não-bloqueantes)
  envelope_atualizar();
}
