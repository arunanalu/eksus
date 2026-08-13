#include "BleTransport.h"
#include "BleProtocol.h"
#include "Comandos.h"
#include "Config.h"
#include "MultiZoneScheduler.h"
#include "Seguranca.h"
#include "ZoneDriver.h"
#include "ZoneMap.h"
#include <Arduino.h>

#if EXUS_BLE_ENABLED
#include <NimBLEDevice.h>

static NimBLEServer* s_server = nullptr;
static NimBLECharacteristic* s_response = nullptr;
static NimBLECharacteristic* s_status = nullptr;
static char s_line[BLE_COMMAND_BUFFER_SIZE] = {0};
static char s_pending[BLE_COMMAND_BUFFER_SIZE] = {0};
static volatile uint8_t s_lineLength = 0;
static volatile bool s_lineOverflow = false;
static volatile bool s_pendingReady = false;
static volatile bool s_stopRequested = false;
static bool s_connected = false;
static bool s_knownBondAtConnect = false;
static bool s_controlAllowed = false;
static unsigned long s_pairWindowUntil = 0;
static unsigned long s_fragmentStartedAt = 0;
static unsigned long s_lastValidCommandAt = 0;
static portMUX_TYPE s_bleMux = portMUX_INITIALIZER_UNLOCKED;

static bool within(unsigned long until) { return until && (long)(until - millis()) > 0; }
static bool pairingEnabled() { return within(s_pairWindowUntil); }

class ResponsePrint : public Print {
 public:
  size_t write(uint8_t value) override {
    if (length_ + 1 < sizeof(buffer_)) buffer_[length_++] = (char)value;
    return 1;
  }
  const char* c_str() { buffer_[length_] = '\0'; return buffer_; }
 private:
  char buffer_[BLE_RESPONSE_BUFFER_SIZE] = {0};
  size_t length_ = 0;
};

static void indicate(const char* text) {
  if (!s_connected || !s_response) return;
  s_response->setValue(text);
  s_response->indicate();
}

static void notifyStatus(const char* event) {
  if (!s_connected || !s_status) return;
  char text[160];
  snprintf(text, sizeof(text), "event=%s emergency=%s ready=%u active=%u bonds=%d",
    event, seguranca_emergencia_ativa() ? "YES" : "NO", zone_driver_ready_count(),
    scheduler_active_count(), NimBLEDevice::getNumBonds());
  s_status->setValue(text);
  s_status->notify();
}

static void enqueueBytes(const std::string& value) {
  portENTER_CRITICAL(&s_bleMux);
  for (char c : value) {
    if (c == '\r') continue;
    if (c == '\n') {
      if (!s_lineOverflow && s_lineLength && !s_pendingReady) {
        s_line[s_lineLength] = '\0';
        memcpy(s_pending, s_line, s_lineLength + 1);
        s_pendingReady = true;
      }
      s_lineLength = 0;
      s_lineOverflow = false;
      s_fragmentStartedAt = 0;
    } else if (!s_lineOverflow && s_lineLength < sizeof(s_line) - 1) {
      if (!s_lineLength) s_fragmentStartedAt = millis();
      s_line[s_lineLength] = c;
      ++s_lineLength;
    } else {
      s_lineOverflow = true;
    }
  }
  portEXIT_CRITICAL(&s_bleMux);
}

class ExusServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* server, NimBLEConnInfo& info) override {
    if (s_connected) { server->disconnect(info.getConnHandle()); return; }
    s_connected = true;
    s_knownBondAtConnect = info.isBonded();
    s_controlAllowed = info.isEncrypted() && info.isBonded() && s_knownBondAtConnect;
    s_lastValidCommandAt = millis();
  }
  void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) override {
    s_connected = false;
    s_controlAllowed = false;
    s_stopRequested = true;
  }
  void onAuthenticationComplete(NimBLEConnInfo& info) override {
    const bool admitted = info.isEncrypted() && info.isBonded() && (s_knownBondAtConnect || pairingEnabled());
    if (!admitted) {
      NimBLEDevice::deleteBond(info.getIdAddress());
      if (s_server) s_server->disconnect(info.getConnHandle());
      return;
    }
    s_controlAllowed = true;
    s_pairWindowUntil = 0;
  }
};

class CommandCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& info) override {
    if (!s_controlAllowed || !info.isEncrypted() || !info.isBonded()) {
      indicate("N 0 link_not_authorized");
      return;
    }
    enqueueBytes(characteristic->getValue());
  }
};

class EmergencyCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic*, NimBLEConnInfo& info) override {
    if (info.isEncrypted() && info.isBonded() && s_controlAllowed) s_stopRequested = true;
  }
};

static bool extractSequence(char* line, unsigned long& sequence, char*& command) {
  // O cliente BLE sempre envia "@<sequência> <comando>\n". Isso evita reenvio
  // ambíguo e preserva a sintaxe dos comandos USB.
  if (line[0] != '@') return false;
  char* end = nullptr;
  sequence = strtoul(line + 1, &end, 10);
  if (end == line + 1 || *end != ' ') return false;
  while (*end == ' ') ++end;
  if (!*end) return false;
  command = end;
  return true;
}

void ble_transport_begin() {
  char name[20];
  const uint32_t id = (uint32_t)(ESP.getEfuseMac() & 0xFFFFFFULL);
  snprintf(name, sizeof(name), "Exus-%06lX", (unsigned long)id);
  NimBLEDevice::init(name);
  NimBLEDevice::setSecurityAuth(true, false, true); // bond + link cifrado + Secure Connections
  NimBLEDevice::setMTU(185);

  s_server = NimBLEDevice::createServer();
  s_server->setCallbacks(new ExusServerCallbacks());
  NimBLEService* service = s_server->createService(EXUS_BLE_SERVICE_UUID);
  NimBLECharacteristic* command = service->createCharacteristic(EXUS_BLE_COMMAND_UUID,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::WRITE_ENC);
  s_response = service->createCharacteristic(EXUS_BLE_RESPONSE_UUID, NIMBLE_PROPERTY::INDICATE);
  s_status = service->createCharacteristic(EXUS_BLE_STATUS_UUID, NIMBLE_PROPERTY::NOTIFY);
  NimBLECharacteristic* info = service->createCharacteristic(EXUS_BLE_DEVICE_INFO_UUID, NIMBLE_PROPERTY::READ);
  NimBLECharacteristic* emergency = service->createCharacteristic(EXUS_BLE_EMERGENCY_UUID,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_ENC);
  char deviceInfo[180];
  snprintf(deviceInfo, sizeof(deviceInfo), "protocol=%u firmware=ble-v1 device=%s zones=%u",
    EXUS_BLE_PROTOCOL_VERSION, name, zone_map_count());
  info->setValue(deviceInfo);
  command->setCallbacks(new CommandCallbacks());
  emergency->setCallbacks(new EmergencyCallbacks());
  NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(EXUS_BLE_SERVICE_UUID);
  advertising->start();
  Serial.printf("[BLE] Anunciando %s; use 'ble pair enable' para liberar primeiro pareamento.\n", name);
}

void ble_transport_enable_pairing() {
  s_pairWindowUntil = millis() + BLE_PAIR_WINDOW_MS;
}

bool ble_transport_clear_bonds() {
  if (s_connected) return false;
  return NimBLEDevice::deleteAllBonds();
}

bool ble_transport_is_connected() { return s_connected; }

void ble_transport_process() {
  if (s_stopRequested) {
    s_stopRequested = false;
    scheduler_stop_all();
    notifyStatus("stopped_link_or_emergency");
  }
  if (s_connected && scheduler_active_count() && millis() - s_lastValidCommandAt > BLE_COMMAND_WATCHDOG_MS) {
    scheduler_stop_all();
    notifyStatus("stopped_watchdog");
  }
  if (s_lineLength && s_fragmentStartedAt && millis() - s_fragmentStartedAt > BLE_FRAGMENT_TIMEOUT_MS) {
    portENTER_CRITICAL(&s_bleMux); s_lineLength = 0; s_lineOverflow = false; s_fragmentStartedAt = 0; portEXIT_CRITICAL(&s_bleMux);
    indicate("N 0 fragment_timeout");
  }
  if (!s_pendingReady) return;
  char line[BLE_COMMAND_BUFFER_SIZE];
  portENTER_CRITICAL(&s_bleMux); memcpy(line, s_pending, sizeof(line)); s_pendingReady = false; portEXIT_CRITICAL(&s_bleMux);
  unsigned long sequence = 0; char* command = nullptr;
  if (!extractSequence(line, sequence, command)) { indicate("N 0 invalid_frame"); return; }
  ResponsePrint response;
  const bool ok = comandos_executar(command, response);
  char framed[BLE_RESPONSE_BUFFER_SIZE + 24];
  snprintf(framed, sizeof(framed), "%c %lu %s", ok ? 'A' : 'N', sequence, response.c_str());
  indicate(framed);
  if (ok) { s_lastValidCommandAt = millis(); notifyStatus("command_ok"); }
}

#else
void ble_transport_begin() {}
void ble_transport_process() {}
void ble_transport_enable_pairing() {}
bool ble_transport_clear_bonds() { return false; }
bool ble_transport_is_connected() { return false; }
#endif
