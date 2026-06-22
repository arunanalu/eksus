#include "Comandos.h"
#include "Config.h"
#include "GeradorEnvelope.h"
#include "Seguranca.h"
#include "DriverHaptico.h"
#include <Arduino.h>

// --------------------------------------------------------
// Funções auxiliares de resposta
// --------------------------------------------------------

static void _ajuda() {
  Serial.println(F("\n=== Comandos disponíveis ==="));
  Serial.println(F("  v <freq> <intens%> [ms]   vibrar (frequência percebida via pulsos)"));
  Serial.println(F("                            ex: v 30 40 2000  (30 Hz, 40%, 2 s)"));
  Serial.println(F("                            ex: v 10 25       (10 Hz, 25%, contínuo)"));
  Serial.println(F("  s                         parar vibração atual"));
  Serial.println(F("  e                         PARADA DE EMERGÊNCIA (bloqueia tudo)"));
  Serial.println(F("  r                         retomar após emergência"));
  Serial.println(F("  ef <1-123>                tocar efeito built-in do DRV2605 (LRA)"));
  Serial.println(F("  status                    exibir estado atual"));
  Serial.println(F("  scan                      verificar DRV2605 no barramento I2C"));
  Serial.println(F("  h  ou  ?                  esta ajuda"));
  Serial.println(F("============================\n"));
}

static void _status() {
  Serial.println(F("\n--- Status ---"));
  Serial.printf("  Emergencia : %s\n",
                seguranca_emergencia_ativa() ? "ATIVA" : "OK");
  Serial.printf("  Envelope   : %s\n",
                envelope_ativo() ? "VIBRANDO" : "parado");
  Serial.printf("  Max intens.: %d%%\n", MAX_INTENSITY_PCT);
  Serial.printf("  Max duracao: %d ms\n", MAX_DURATION_MS);
  Serial.printf("  Cooldown   : %d ms\n", MIN_COOLDOWN_MS);
  Serial.println(F("--------------\n"));
}

// --------------------------------------------------------
// Handlers dos comandos
// --------------------------------------------------------

static void _cmd_vibrar(const char* args) {
  float         freq_hz   = DEFAULT_FREQ_HZ;
  int           intensity = DEFAULT_INTENSITY_PCT;
  unsigned long dur_ul    = 0;  // 0 = contínuo

  int n = sscanf(args, "%f %d %lu", &freq_hz, &intensity, &dur_ul);
  if (n < 2) {
    Serial.println(F("[ERRO] Uso: v <freq_hz> <intensidade%> [duracao_ms]"));
    Serial.println(F("       Exemplo: v 30 40 2000"));
    return;
  }

  if (!seguranca_cooldown_ok()) {
    Serial.printf("[AVISO] Cooldown ativo: aguarde %d ms entre ativacoes.\n",
                  MIN_COOLDOWN_MS);
    return;
  }

  ParametrosValidados p = seguranca_validar(freq_hz, intensity,
                                            (uint32_t)dur_ul, DEFAULT_DUTY_CYCLE);
  if (!p.valido) {
    Serial.println(F("[ERRO] Parametros invalidos."));
    return;
  }

  if (envelope_ativo()) {
    envelope_parar();
  }

  ParametrosEnvelope ep;
  ep.freq_hz    = p.freq_hz;
  ep.amplitude  = p.amplitude;
  ep.duracao_ms = p.duracao_ms;
  ep.duty_cycle = p.duty_cycle;
  envelope_iniciar(ep);

  if (p.duracao_ms == 0) {
    Serial.printf("[OK] Vibrando %.1f Hz | amplitude %d/127 | continuo\n",
                  p.freq_hz, p.amplitude);
  } else {
    Serial.printf("[OK] Vibrando %.1f Hz | amplitude %d/127 | %lu ms\n",
                  p.freq_hz, p.amplitude, (unsigned long)p.duracao_ms);
  }
}

static void _cmd_efeito(const char* args) {
  int effect = atoi(args);
  if (effect < 1 || effect > 123) {
    Serial.println(F("[ERRO] Efeito deve ser um numero entre 1 e 123."));
    return;
  }

  if (envelope_ativo()) {
    envelope_parar();
  }

  drv.selectLibrary(6);               // biblioteca LRA
  drv.setMode(DRV2605_MODE_INTTRIG);
  drv.setWaveform(0, (uint8_t)effect);
  drv.setWaveform(1, 0);              // fim da sequência de efeitos
  drv.go();

  Serial.printf("[OK] Efeito %d acionado. Use 's' para parar antes de terminar.\n",
                effect);
}

// --------------------------------------------------------
// Despachante central — baseado em tokens para evitar
// conflitos de prefixo (ex: "e" vs "ef").
// --------------------------------------------------------

static void _executar(const char* linha) {
  // Descarta espaços iniciais
  while (*linha == ' ') linha++;
  if (*linha == '\0') return;

  // Extrai o primeiro token (comando) em minúsculas
  char    cmd[16];
  uint8_t ci    = 0;
  const char* p = linha;
  while (*p && *p != ' ' && ci < 15) {
    cmd[ci++] = (char)tolower((unsigned char)*p++);
  }
  cmd[ci] = '\0';

  // Avança para os argumentos
  while (*p == ' ') p++;
  const char* args = p;

  // --------------------------------------------------
  // Comandos de emergência: sempre processados
  // --------------------------------------------------
  if (strcmp(cmd, "e") == 0) {
    seguranca_emergencia_ativar();
    envelope_parar();
    drv_parar();
    Serial.println(F("[EMERGENCIA] Sistema parado. Digite 'r' para retomar."));
    return;
  }

  if (strcmp(cmd, "r") == 0) {
    if (seguranca_emergencia_ativa()) {
      seguranca_emergencia_liberar();
      Serial.println(F("[OK] Emergencia liberada. Sistema ativo."));
    } else {
      Serial.println(F("[INFO] Nenhuma emergencia ativa."));
    }
    return;
  }

  // --------------------------------------------------
  // Demais comandos bloqueados durante emergência
  // --------------------------------------------------
  if (seguranca_emergencia_ativa()) {
    Serial.println(F("[EMERGENCIA] Bloqueado. Digite 'r' para retomar."));
    return;
  }

  // Rate limit silencioso
  if (!seguranca_rate_limit_ok()) return;
  seguranca_registrar_comando();

  // --------------------------------------------------
  // Despacho por comando
  // --------------------------------------------------
  if (strcmp(cmd, "v") == 0) {
    _cmd_vibrar(args);
    return;
  }

  if (strcmp(cmd, "s") == 0) {
    if (envelope_ativo()) {
      envelope_parar();
      Serial.println(F("[OK] Vibracao parada."));
    } else {
      drv_parar();
      Serial.println(F("[OK] Motor parado."));
    }
    return;
  }

  if (strcmp(cmd, "ef") == 0) {
    _cmd_efeito(args);
    return;
  }

  if (strcmp(cmd, "h") == 0 || strcmp(cmd, "?") == 0) {
    _ajuda();
    return;
  }

  if (strcmp(cmd, "status") == 0) {
    _status();
    return;
  }

  if (strcmp(cmd, "scan") == 0) {
    bool ok = drv_escanear_i2c();
    Serial.printf("[%s] DRV2605 (0x5A): %s\n",
                  ok ? "OK" : "ERRO",
                  ok ? "detectado" : "NAO encontrado — verifique ligacoes");
    return;
  }

  Serial.printf("[ERRO] Comando desconhecido: '%s'. Digite 'h' para ver ajuda.\n", cmd);
}

// --------------------------------------------------------
// Leitura Serial linha a linha (não-bloqueante)
// --------------------------------------------------------

void comandos_processar() {
  static char    buf[64];
  static uint8_t pos = 0;

  while (Serial.available()) {
    char c = (char)Serial.read();

    if (c == '\n' || c == '\r') {
      if (pos > 0) {
        buf[pos] = '\0';
        _executar(buf);
        pos = 0;
      }
    } else if (pos < sizeof(buf) - 1) {
      buf[pos++] = c;
    } else {
      // Buffer cheio: descarta a linha corrente
      pos = 0;
    }
  }
}
