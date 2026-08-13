#include "Comandos.h"
#include "BleTransport.h"
#include "Config.h"
#include "MultiZoneScheduler.h"
#include "Seguranca.h"
#include "ZoneDriver.h"
#include "ZoneMap.h"
#include <stdlib.h>

static Print* g_output = &Serial;
static Print& out() { return *g_output; }

static void help() {
  out().println(F("\n=== Exus multi-zona ==="));
  out().println(F("zones | scan all | status [zone] | Q [seq]"));
  out().println(F("pulse <zone> <intens%> <ms> [Hz] | effect <zone> <1-123>"));
  out().println(F("group <mask> <pulse|effect> ... | stop <zone|mux:N|all>"));
  out().println(F("emergency | resume | ble status | ble pair enable | ble bonds clear"));
}

static void capabilities(unsigned long seq) {
  out().printf("A %lu CAP {\"protocol\":1,\"zones_configured\":%u,\"zones_ready\":[",
    seq, zone_map_count());
  bool first = true;
  for (uint8_t id = 0; id < zone_map_count(); ++id) {
    if (!zone_driver_ready(id)) continue;
    if (!first) out().print(',');
    out().print(id);
    first = false;
  }
  out().printf("],\"max_group_size\":%u,\"features\":[\"rom\",\"rtp\",\"per_zone_limits\",\"dynamic_mux_discovery\"]}\n",
    MAX_SIMULTANEOUS_ZONES);
}

static bool commandStatus(const char* args) {
  int zone = -1;
  if (*args) zone = atoi(args);
  out().printf("emergency=%s ready=%u active=%u amplitude_sum=%u i2c_failures=%lu\n",
    seguranca_emergencia_ativa() ? "YES" : "NO", zone_driver_ready_count(),
    scheduler_active_count(), scheduler_amplitude_sum(),
    (unsigned long)zone_driver_i2c_failures());
  if (zone < 0) return true;
  if (zone >= zone_map_count()) { out().println(F("[ERRO] Zona fora do mapa.")); return false; }
  const ZoneRuntimeState* state = scheduler_state((uint8_t)zone);
  out().printf("zone=%d hw=%s active=%s mode=%u amplitude=%u missed_deadlines=%lu\n",
    zone, zone_driver_status_name(zone_driver_status(zone)), scheduler_zone_active(zone) ? "YES" : "NO",
    state->mode, state->amplitude, (unsigned long)state->missedDeadlines);
  return true;
}

static bool commandPulse(const char* args) {
  int zone, intensity; unsigned long duration; float frequency = DEFAULT_FREQ_HZ;
  if (sscanf(args, "%d %d %lu %f", &zone, &intensity, &duration, &frequency) < 3) {
    out().println(F("[ERRO] Uso: pulse <zone> <intens%> <ms> [Hz]")); return false;
  }
  const bool ok = zone >= 0 && zone < zone_map_count() &&
    scheduler_start_pulse((uint8_t)zone, frequency, intensity, duration);
  out().printf("[%s] pulse zone=%d\n", ok ? "OK" : "IGNORADO", zone);
  return ok;
}

static bool commandEffect(const char* args) {
  int zone, effect;
  if (sscanf(args, "%d %d", &zone, &effect) != 2) {
    out().println(F("[ERRO] Uso: effect <zone> <1-123>")); return false;
  }
  const bool ok = zone >= 0 && zone < zone_map_count() && scheduler_start_effect((uint8_t)zone, (uint8_t)effect);
  out().printf("[%s] effect zone=%d\n", ok ? "OK" : "IGNORADO", zone);
  return ok;
}

static bool commandMux(const char* args) {
  int mux = 0, consumed = 0; char mode[12] = {0};
  if (sscanf(args, "%d %11s%n", &mux, mode, &consumed) < 2 || mux < 1 || mux > EXUS_MAX_MUXES) {
    out().println(F("[ERRO] Uso: mux <1-8> <pulse|effect> ...")); return false;
  }
  if (!zone_driver_mux_present((uint8_t)mux)) { out().printf("[INFO] mux %d ausente; nenhum atuador acionado.\n", mux); return false; }
  const char* tail = args + consumed; while (*tail == ' ') ++tail;
  uint8_t accepted = 0;
  if (!strcmp(mode, "pulse")) {
    int intensity; unsigned long duration; float frequency = DEFAULT_FREQ_HZ;
    if (sscanf(tail, "%d %lu %f", &intensity, &duration, &frequency) < 2) { out().println(F("[ERRO] Uso: mux <1-8> pulse <intens%> <ms> [Hz]")); return false; }
    accepted = scheduler_start_mux_pulse((uint8_t)mux, frequency, intensity, duration);
  } else if (!strcmp(mode, "effect")) {
    const int effect = atoi(tail);
    if (effect < 1 || effect > 123) { out().println(F("[ERRO] Efeito deve estar entre 1 e 123.")); return false; }
    accepted = scheduler_start_mux_effect((uint8_t)mux, (uint8_t)effect);
  } else { out().println(F("[ERRO] Modo deve ser pulse ou effect.")); return false; }
  out().printf("[%s] mux=%d zonas_acionadas=%u\n", accepted ? "OK" : "IGNORADO", mux, accepted);
  return accepted != 0;
}

static bool commandGroup(const char* args) {
  char maskText[20] = {0}, mode[12] = {0}; int consumed = 0;
  if (sscanf(args, "%19s %11s%n", maskText, mode, &consumed) < 2) { out().println(F("[ERRO] Uso: group <mask> <pulse|effect> ...")); return false; }
  const uint64_t mask = strtoull(maskText, nullptr, 0); const char* tail = args + consumed; while (*tail == ' ') ++tail;
  int intensity = 0, effect = 0; unsigned long duration = 0; float frequency = DEFAULT_FREQ_HZ;
  if (!strcmp(mode, "pulse")) { if (sscanf(tail, "%d %lu %f", &intensity, &duration, &frequency) < 2) return false; }
  else if (!strcmp(mode, "effect")) { effect = atoi(tail); if (effect < 1 || effect > 123) return false; }
  else return false;
  uint8_t accepted = 0;
  for (uint8_t id = 0; id < zone_map_count(); ++id) if (mask & (UINT64_C(1) << id)) {
    const bool ok = !strcmp(mode, "pulse") ? scheduler_start_pulse(id, frequency, intensity, duration) : scheduler_start_effect(id, (uint8_t)effect);
    if (ok) ++accepted;
  }
  out().printf("[%s] group zonas_acionadas=%u\n", accepted ? "OK" : "IGNORADO", accepted);
  return accepted != 0;
}

static bool commandStop(const char* args) {
  if (!*args || !strcmp(args, "all")) scheduler_stop_all();
  else if (!strncmp(args, "mux:", 4)) {
    const int mux = atoi(args + 4); if (mux < 1 || mux > EXUS_MAX_MUXES) return false;
    const uint8_t first = (uint8_t)((mux - 1) * 8); for (uint8_t id = first; id < first + 8; ++id) scheduler_stop_zone(id);
  } else { const int zone = atoi(args); if (zone < 0 || zone >= zone_map_count()) return false; scheduler_stop_zone((uint8_t)zone); }
  out().println(F("[OK] stop processado.")); return true;
}

static bool executeInternal(const char* line) {
  while (*line == ' ') ++line;
  if (!*line) return false;
  char command[16] = {0}; uint8_t pos = 0;
  while (*line && *line != ' ' && pos < sizeof(command) - 1) command[pos++] = tolower(*line++);
  while (*line == ' ') ++line; const char* args = line;
  if (!strcmp(command, "emergency") || !strcmp(command, "e")) { seguranca_emergencia_ativar(); scheduler_stop_all(); out().println(F("[EMERGENCIA] Todas as zonas paradas.")); return true; }
  if (!strcmp(command, "resume") || !strcmp(command, "r")) { scheduler_stop_all(); if (scheduler_active_count()) { out().println(F("[EMERGENCIA] Falha ao confirmar parada; bloqueio mantido.")); return false; } seguranca_emergencia_liberar(); out().println(F("[OK] Emergencia liberada apos inspecao.")); return true; }
  if (!strcmp(command, "ble")) {
    if (!strcmp(args, "pair enable")) {
      const bool ok = ble_transport_enable_pairing();
      out().println(ok ? F("[OK] Pareamento BLE liberado por 60 s.") :
        F("[ERRO] Desconecte o PC e apague os bonds antes de liberar novo pareamento."));
      return ok;
    }
    if (!strcmp(args, "bonds clear")) { const bool ok = ble_transport_clear_bonds(); out().println(ok ? F("[OK] Bonds BLE apagados.") : F("[ERRO] Nao foi possivel apagar bonds BLE.")); return ok; }
    if (!strcmp(args, "status")) { ble_transport_print_status(out()); return true; }
    out().println(F("[ERRO] Uso: ble status | ble pair enable | ble bonds clear")); return false;
  }
  if (!strcmp(command, "stop") || !strcmp(command, "s")) return commandStop(args);
  if (!strcmp(command, "status")) return commandStatus(args);
  if (!strcmp(command, "zones") || !strcmp(command, "scan")) { zone_driver_scan_report(); return true; }
  if (!strcmp(command, "q")) { capabilities(*args ? strtoul(args, nullptr, 10) : 0); return true; }
  if (!strcmp(command, "h") || !strcmp(command, "?")) { help(); return true; }
  if (seguranca_emergencia_ativa()) { out().println(F("[EMERGENCIA] Comando bloqueado.")); return false; }
  if (!seguranca_rate_limit_ok()) return false;
  seguranca_registrar_comando();
  if (!strcmp(command, "pulse")) return commandPulse(args);
  if (!strcmp(command, "effect")) return commandEffect(args);
  if (!strcmp(command, "mux")) return commandMux(args);
  if (!strcmp(command, "group")) return commandGroup(args);
  if (!strcmp(command, "v")) { float frequency; int intensity; unsigned long duration = 0; const bool ok = sscanf(args, "%f %d %lu", &frequency, &intensity, &duration) >= 2 && scheduler_start_pulse(0, frequency, intensity, duration); out().printf("[%s] legacy zone=0\n", ok ? "OK" : "IGNORADO"); return ok; }
  if (!strcmp(command, "ef")) { const bool ok = scheduler_start_effect(0, (uint8_t)atoi(args)); out().printf("[%s] legacy zone=0\n", ok ? "OK" : "IGNORADO"); return ok; }
  out().println(F("[ERRO] Comando desconhecido. Digite h.")); return false;
}

bool comandos_executar(const char* linha, Print& output) {
  Print* previous = g_output; g_output = &output; const bool ok = executeInternal(linha); g_output = previous; return ok;
}

void comandos_processar() {
  static char buffer[SERIAL_BUFFER_SIZE]; static uint8_t pos = 0; static bool overflow = false;
  while (Serial.available()) {
    const char c = (char)Serial.read();
    if (c == '\n' || c == '\r') { if (overflow) Serial.println(F("[ERRO] Linha Serial excedeu o buffer e foi descartada.")); else if (pos) { buffer[pos] = '\0'; comandos_executar(buffer, Serial); } pos = 0; overflow = false; }
    else if (!overflow && pos < sizeof(buffer) - 1) buffer[pos++] = c; else overflow = true;
  }
}
