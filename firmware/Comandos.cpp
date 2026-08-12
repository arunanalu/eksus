#include "Comandos.h"
#include "Config.h"
#include "MultiZoneScheduler.h"
#include "Seguranca.h"
#include "ZoneDriver.h"
#include "ZoneMap.h"
#include <Arduino.h>
#include <stdlib.h>

static void help() {
  Serial.println(F("\n=== Exus multi-zona ==="));
  Serial.println(F("zones | scan all                 mapa e descoberta do hardware"));
  Serial.println(F("pulse <zone> <intens%> <ms> [Hz] envelope RTP por zona"));
  Serial.println(F("effect <zone> <1-123>            efeito ROM por zona"));
  Serial.println(F("mux <1-8> pulse <intens%> <ms> [Hz]"));
  Serial.println(F("mux <1-8> effect <1-123>         aciona zonas calibradas do mux"));
  Serial.println(F("group <mask> pulse <intens%> <ms> [Hz]"));
  Serial.println(F("group <mask> effect <1-123>       mascara de 64 bits (ex.: 0x03)"));
  Serial.println(F("stop <zone|mux:N|all> | status [zone] | Q [seq]"));
  Serial.println(F("emergency | resume"));
  Serial.println(F("Legado: v <Hz> <intens%> [ms], ef <1-123>, s, e, r, h"));
}

static void capabilities(unsigned long seq) {
  Serial.printf("A %lu CAP {\"protocol\":1,\"zones_configured\":%u,\"zones_ready\":[",
    seq, zone_map_count());
  bool first = true;
  for (uint8_t id = 0; id < zone_map_count(); ++id) {
    if (!zone_driver_ready(id)) continue;
    if (!first) Serial.print(',');
    Serial.print(id);
    first = false;
  }
  Serial.printf("],\"max_group_size\":%u,\"features\":[\"rom\",\"rtp\",\"per_zone_limits\",\"dynamic_mux_discovery\"]}\n",
    MAX_SIMULTANEOUS_ZONES);
}

static void status(const char* args) {
  int zone = -1;
  if (*args) zone = atoi(args);
  Serial.printf("emergency=%s ready=%u active=%u amplitude_sum=%u i2c_failures=%lu\n",
    seguranca_emergencia_ativa() ? "YES" : "NO", zone_driver_ready_count(),
    scheduler_active_count(), scheduler_amplitude_sum(),
    (unsigned long)zone_driver_i2c_failures());
  if (zone >= 0) {
    if (zone >= zone_map_count()) {
      Serial.println(F("[ERRO] Zona fora do mapa."));
      return;
    }
    const ZoneRuntimeState* state = scheduler_state((uint8_t)zone);
    Serial.printf("zone=%d hw=%s active=%s mode=%u amplitude=%u missed_deadlines=%lu\n",
      zone, zone_driver_status_name(zone_driver_status(zone)),
      scheduler_zone_active(zone) ? "YES" : "NO", state->mode, state->amplitude,
      (unsigned long)state->missedDeadlines);
    if (zone_driver_uncalibrated((uint8_t)zone)) {
      Serial.printf("[AVISO] Somente bancada: RTP limitado a %u%% por %lums; efeitos ROM bloqueados.\n",
        UNCALIBRATED_MAX_INTENSITY_PCT, (unsigned long)UNCALIBRATED_MAX_DURATION_MS);
    }
  }
}

static void commandPulse(const char* args) {
  int zone, intensity;
  unsigned long duration;
  float frequency = DEFAULT_FREQ_HZ;
  if (sscanf(args, "%d %d %lu %f", &zone, &intensity, &duration, &frequency) < 3) {
    Serial.println(F("[ERRO] Uso: pulse <zone> <intens%> <ms> [Hz]"));
    return;
  }
  const bool ok = zone >= 0 && zone < zone_map_count() &&
    scheduler_start_pulse((uint8_t)zone, frequency, intensity, duration);
  Serial.printf("[%s] pulse zone=%d\n", ok ? "OK" : "IGNORADO", zone);
}

static void commandEffect(const char* args) {
  int zone, effect;
  if (sscanf(args, "%d %d", &zone, &effect) != 2) {
    Serial.println(F("[ERRO] Uso: effect <zone> <1-123>"));
    return;
  }
  const bool ok = zone >= 0 && zone < zone_map_count() &&
    scheduler_start_effect((uint8_t)zone, (uint8_t)effect);
  Serial.printf("[%s] effect zone=%d\n", ok ? "OK" : "IGNORADO", zone);
}

static void commandMux(const char* args) {
  int mux = 0;
  char mode[12] = {0};
  int consumed = 0;
  if (sscanf(args, "%d %11s%n", &mux, mode, &consumed) < 2 || mux < 1 || mux > EXUS_MAX_MUXES) {
    Serial.println(F("[ERRO] Uso: mux <1-8> <pulse|effect> ..."));
    return;
  }
  if (!zone_driver_mux_present((uint8_t)mux)) {
    Serial.printf("[INFO] mux %d ausente; nenhum atuador acionado.\n", mux);
    return;
  }
  const char* tail = args + consumed;
  while (*tail == ' ') ++tail;
  uint8_t accepted = 0;
  if (strcmp(mode, "pulse") == 0) {
    int intensity;
    unsigned long duration;
    float frequency = DEFAULT_FREQ_HZ;
    if (sscanf(tail, "%d %lu %f", &intensity, &duration, &frequency) < 2) {
      Serial.println(F("[ERRO] Uso: mux <1-8> pulse <intens%> <ms> [Hz]"));
      return;
    }
    accepted = scheduler_start_mux_pulse((uint8_t)mux, frequency, intensity, duration);
  } else if (strcmp(mode, "effect") == 0) {
    const int effect = atoi(tail);
    if (effect < 1 || effect > 123) {
      Serial.println(F("[ERRO] Efeito deve estar entre 1 e 123."));
      return;
    }
    accepted = scheduler_start_mux_effect((uint8_t)mux, (uint8_t)effect);
  } else {
    Serial.println(F("[ERRO] Modo deve ser pulse ou effect."));
    return;
  }
  Serial.printf("[%s] mux=%d zonas_acionadas=%u\n", accepted ? "OK" : "IGNORADO", mux, accepted);
}

static void commandGroup(const char* args) {
  char maskText[20] = {0};
  char mode[12] = {0};
  int consumed = 0;
  if (sscanf(args, "%19s %11s%n", maskText, mode, &consumed) < 2) {
    Serial.println(F("[ERRO] Uso: group <mask> <pulse|effect> ..."));
    return;
  }
  const uint64_t mask = strtoull(maskText, nullptr, 0);
  const char* tail = args + consumed;
  while (*tail == ' ') ++tail;
  int intensity = 0, effect = 0;
  unsigned long duration = 0;
  float frequency = DEFAULT_FREQ_HZ;
  if (strcmp(mode, "pulse") == 0) {
    if (sscanf(tail, "%d %lu %f", &intensity, &duration, &frequency) < 2) return;
  } else if (strcmp(mode, "effect") == 0) {
    effect = atoi(tail);
    if (effect < 1 || effect > 123) return;
  } else return;

  uint8_t accepted = 0;
  for (uint8_t id = 0; id < zone_map_count(); ++id) {
    if (!(mask & (UINT64_C(1) << id))) continue;
    const bool ok = strcmp(mode, "pulse") == 0
      ? scheduler_start_pulse(id, frequency, intensity, duration)
      : scheduler_start_effect(id, (uint8_t)effect);
    if (ok) ++accepted;
  }
  Serial.printf("[%s] group zonas_acionadas=%u\n", accepted ? "OK" : "IGNORADO", accepted);
}

static void commandStop(const char* args) {
  if (!*args || strcmp(args, "all") == 0) {
    scheduler_stop_all();
  } else if (strncmp(args, "mux:", 4) == 0) {
    const int mux = atoi(args + 4);
    if (mux >= 1 && mux <= EXUS_MAX_MUXES) {
      const uint8_t first = (uint8_t)((mux - 1) * 8);
      for (uint8_t id = first; id < first + 8; ++id) scheduler_stop_zone(id);
    }
  } else {
    const int zone = atoi(args);
    if (zone >= 0 && zone < zone_map_count()) scheduler_stop_zone((uint8_t)zone);
  }
  Serial.println(F("[OK] stop processado."));
}

static void execute(const char* line) {
  while (*line == ' ') ++line;
  if (!*line) return;
  char command[16] = {0};
  uint8_t pos = 0;
  while (*line && *line != ' ' && pos < sizeof(command) - 1) command[pos++] = tolower(*line++);
  while (*line == ' ') ++line;
  const char* args = line;

  if (!strcmp(command, "emergency") || !strcmp(command, "e")) {
    seguranca_emergencia_ativar();
    scheduler_stop_all();
    Serial.println(F("[EMERGENCIA] Todas as zonas paradas."));
    return;
  }
  if (!strcmp(command, "resume") || !strcmp(command, "r")) {
    scheduler_stop_all();
    if (scheduler_active_count() != 0) {
      Serial.println(F("[EMERGENCIA] Falha ao confirmar parada de todas as zonas; bloqueio mantido."));
      return;
    }
    seguranca_emergencia_liberar();
    Serial.println(F("[OK] Emergencia liberada apos inspecao."));
    return;
  }
  if (!strcmp(command, "stop") || !strcmp(command, "s")) { commandStop(args); return; }
  if (!strcmp(command, "status")) { status(args); return; }
  if (!strcmp(command, "zones") || !strcmp(command, "scan")) { zone_driver_scan_report(); return; }
  if (!strcmp(command, "q")) { capabilities(*args ? strtoul(args, nullptr, 10) : 0); return; }
  if (!strcmp(command, "h") || !strcmp(command, "?")) { help(); return; }
  if (seguranca_emergencia_ativa()) {
    Serial.println(F("[EMERGENCIA] Comando bloqueado."));
    return;
  }
  if (!seguranca_rate_limit_ok()) return;
  seguranca_registrar_comando();

  if (!strcmp(command, "pulse")) commandPulse(args);
  else if (!strcmp(command, "effect")) commandEffect(args);
  else if (!strcmp(command, "mux")) commandMux(args);
  else if (!strcmp(command, "group")) commandGroup(args);
  else if (!strcmp(command, "v")) {
    float frequency; int intensity; unsigned long duration = 0;
    if (sscanf(args, "%f %d %lu", &frequency, &intensity, &duration) >= 2)
      Serial.printf("[%s] legacy zone=0\n", scheduler_start_pulse(0, frequency, intensity, duration) ? "OK" : "IGNORADO");
  } else if (!strcmp(command, "ef")) {
    Serial.printf("[%s] legacy zone=0\n", scheduler_start_effect(0, (uint8_t)atoi(args)) ? "OK" : "IGNORADO");
  } else Serial.println(F("[ERRO] Comando desconhecido. Digite h."));
}

void comandos_processar() {
  static char buffer[SERIAL_BUFFER_SIZE];
  static uint8_t pos = 0;
  static bool overflow = false;
  while (Serial.available()) {
    const char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (overflow) Serial.println(F("[ERRO] Linha Serial excedeu o buffer e foi descartada."));
      else if (pos) { buffer[pos] = '\0'; execute(buffer); }
      pos = 0;
      overflow = false;
    } else if (!overflow && pos < sizeof(buffer) - 1) buffer[pos++] = c;
    else overflow = true;
  }
}
