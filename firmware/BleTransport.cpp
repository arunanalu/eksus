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
static NimBLEAdvertising* s_advertising = nullptr;
static NimBLECharacteristic* s_response = nullptr;
static NimBLECharacteristic* s_status = nullptr;
static char s_deviceName[20] = {0};
static char s_line[BLE_COMMAND_BUFFER_SIZE] = {0};
static char s_pending[BLE_COMMAND_BUFFER_SIZE] = {0};
static volatile uint8_t s_lineLength = 0;
static volatile bool s_lineOverflow = false;
static volatile bool s_pendingReady = false;
static volatile bool s_stopRequested = false;
static volatile bool s_connected = false;
static volatile bool s_controlAllowed = false;
static bool s_initialized = false;
static unsigned long s_fragmentStartedAt = 0;
static unsigned long s_lastValidCommandAt = 0;
static unsigned long s_lastAdvertisingAttemptAt = 0;
static uint32_t s_advertisingRestartCount = 0;
static int s_lastDisconnectReason = 0;
static portMUX_TYPE s_bleMux = portMUX_INITIALIZER_UNLOCKED;

// Chamado tanto pelos callbacks da pilha BLE como pelo loop Arduino. O lock
// impede que um pulso anterior a uma desconexao/emergencia seja executado.
static void discardPendingCommand() {
  portENTER_CRITICAL(&s_bleMux);
  s_lineLength = 0;
  s_lineOverflow = false;
  s_pendingReady = false;
  s_line[0] = '\0';
  s_pending[0] = '\0';
  s_fragmentStartedAt = 0;
  portEXIT_CRITICAL(&s_bleMux);
}

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
  char text[180];
  snprintf(text, sizeof(text),
    "event=%s access=OPEN emergency=%s ready=%u active=%u bonds=%d",
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
    if (s_connected) {
      server->disconnect(info.getConnHandle());
      return;
    }
    s_connected = true;
    s_controlAllowed = true;
    s_lastValidCommandAt = millis();
    Serial.printf("[BLE] Link aberto de %s; controle autorizado sem pareamento.\n",
      info.getIdAddress().toString().c_str());
  }

  void onDisconnect(NimBLEServer* server, NimBLEConnInfo&, int reason) override {
    // Uma conexao secundaria recusada nao pode derrubar a sessao principal.
    if (server && server->getConnectedCount() > 0) return;
    s_connected = false;
    s_controlAllowed = false;
    s_lastDisconnectReason = reason;
    discardPendingCommand();
    s_stopRequested = true;
    Serial.printf("[BLE] Link desconectado (reason=%d); motores e fila BLE parados.\n", reason);
  }

  void onAuthenticationComplete(NimBLEConnInfo& info) override {
    // O protocolo nao depende de autenticacao. Alguns sistemas podem tentar
    // criptografar por conta de um cache legado; isso nao muda a autorizacao.
    Serial.printf("[BLE] Autenticacao opcional: encrypted=%d bonded=%d.\n",
      info.isEncrypted(), info.isBonded());
  }
};

class CommandCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo&) override {
    if (!s_controlAllowed) {
      indicate("N 0 link_not_authorized");
      return;
    }
    enqueueBytes(characteristic->getValue());
  }
};

class EmergencyCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic*, NimBLEConnInfo&) override {
    if (s_controlAllowed) {
      discardPendingCommand();
      s_stopRequested = true;
    }
  }
};

static bool extractSequence(char* line, unsigned long& sequence, char*& command) {
  // O cliente BLE envia "@<sequencia> <comando>\n". Isso evita reenvio
  // ambiguo e preserva a sintaxe dos comandos USB.
  if (line[0] != '@') return false;
  char* end = nullptr;
  sequence = strtoul(line + 1, &end, 10);
  if (end == line + 1 || *end != ' ') return false;
  while (*end == ' ') ++end;
  if (!*end) return false;
  command = end;
  return true;
}

static bool ensureAdvertising(bool force) {
  if (!s_initialized || !s_advertising || s_connected) return false;
  if (s_advertising->isAdvertising()) return true;
  const unsigned long now = millis();
  if (!force && now - s_lastAdvertisingAttemptAt < BLE_ADVERTISING_RETRY_MS) return false;
  s_lastAdvertisingAttemptAt = now;
  const bool started = s_advertising->start(0); // zero = sem limite de tempo
  if (started) {
    ++s_advertisingRestartCount;
    Serial.printf("[BLE] Advertising aberto ativo (reinicios=%lu).\n",
      (unsigned long)s_advertisingRestartCount);
  } else {
    Serial.println(F("[AVISO] BLE nao anunciou; nova tentativa automatica em 1 s."));
  }
  return started;
}

void ble_transport_begin() {
  const uint32_t id = (uint32_t)(ESP.getEfuseMac() & 0xFFFFFFULL);
  snprintf(s_deviceName, sizeof(s_deviceName), "Exus-%06lX", (unsigned long)id);
  if (!NimBLEDevice::init(s_deviceName)) {
    Serial.println(F("[ERRO] Falha ao inicializar NimBLE."));
    return;
  }

  // Acesso deliberadamente aberto: nao cria bond, nao exige PIN e nao exige
  // criptografia GATT. Assim qualquer computador ou telefone BLE pode entrar.
  NimBLEDevice::setSecurityAuth(false, false, false);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
  NimBLEDevice::setMTU(185);
  const int legacyBonds = NimBLEDevice::getNumBonds();
  if (legacyBonds > 0) {
    // Preserve chaves existentes durante a migracao. Elas nao autorizam nem
    // bloqueiam o GATT aberto, mas podem evitar conflito no primeiro acesso de
    // um Windows que ainda tenha o vinculo antigo em cache.
    Serial.printf("[BLE] Bonds legados preservados=%d; acesso aberto independe deles.\n",
      legacyBonds);
  }

  s_server = NimBLEDevice::createServer();
  if (!s_server) {
    Serial.println(F("[ERRO] Falha ao criar servidor BLE."));
    return;
  }
  s_server->setCallbacks(new ExusServerCallbacks());
  s_server->advertiseOnDisconnect(true);

  NimBLEService* service = s_server->createService(EXUS_BLE_SERVICE_UUID);
  NimBLECharacteristic* command = service->createCharacteristic(EXUS_BLE_COMMAND_UUID,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  s_response = service->createCharacteristic(EXUS_BLE_RESPONSE_UUID, NIMBLE_PROPERTY::INDICATE);
  s_status = service->createCharacteristic(EXUS_BLE_STATUS_UUID, NIMBLE_PROPERTY::NOTIFY);
  NimBLECharacteristic* info = service->createCharacteristic(
    EXUS_BLE_DEVICE_INFO_UUID, NIMBLE_PROPERTY::READ);
  NimBLECharacteristic* emergency = service->createCharacteristic(EXUS_BLE_EMERGENCY_UUID,
    NIMBLE_PROPERTY::WRITE);

  char deviceInfo[200];
  snprintf(deviceInfo, sizeof(deviceInfo),
    "protocol=%u firmware=ble-open-v2 device=%s zones=%u access=open nimble=%s",
    EXUS_BLE_PROTOCOL_VERSION, s_deviceName, zone_map_count(), NimBLEDevice::getVersion());
  info->setValue(deviceInfo);
  command->setCallbacks(new CommandCallbacks());
  emergency->setCallbacks(new EmergencyCallbacks());
  if (!service->start()) {
    Serial.println(F("[ERRO] Falha ao iniciar servico GATT Exus."));
    return;
  }

  s_advertising = NimBLEDevice::getAdvertising();
  // O pacote principal leva o nome; a resposta de scan leva o UUID de 128 bits.
  NimBLEAdvertisementData advertisement;
  const bool flagsOk = advertisement.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
  const bool nameOk = advertisement.setName(s_deviceName);
  NimBLEAdvertisementData scanResponse;
  const bool serviceOk = scanResponse.addServiceUUID(EXUS_BLE_SERVICE_UUID);
  s_advertising->reset();
  const bool connectableOk = s_advertising->setConnectableMode(BLE_GAP_CONN_MODE_UND);
  const bool discoverableOk = s_advertising->setDiscoverableMode(BLE_GAP_DISC_MODE_GEN);
  const bool mainDataOk = s_advertising->setAdvertisementData(advertisement);
  const bool scanDataOk = s_advertising->setScanResponseData(scanResponse);
  s_advertising->enableScanResponse(true);
  s_initialized = flagsOk && nameOk && serviceOk && connectableOk && discoverableOk &&
    mainDataOk && scanDataOk;
  if (!s_initialized) {
    Serial.printf("[ERRO] BLE configuracao invalida (flags=%d name=%d service=%d connect=%d discover=%d main=%d scan=%d).\n",
      flagsOk, nameOk, serviceOk, connectableOk, discoverableOk, mainDataOk, scanDataOk);
    return;
  }

  if (ensureAdvertising(true)) {
    Serial.printf("[BLE] %s sempre disponivel; pareamento nao e necessario.\n", s_deviceName);
  }
}

bool ble_transport_enable_pairing() {
  // Mantido para scripts antigos: o modo aberto nao possui janela a habilitar.
  return s_initialized;
}

bool ble_transport_clear_bonds() {
  if (!s_initialized || s_connected) return false;
  if (NimBLEDevice::getNumBonds() == 0) return true;
  return NimBLEDevice::deleteAllBonds();
}

bool ble_transport_is_connected() { return s_connected; }

void ble_transport_print_status(Print& output) {
  const int bonds = s_initialized ? NimBLEDevice::getNumBonds() : 0;
  output.printf("[BLE] device=%s initialized=%s connected=%s control=%s advertising=%s access=OPEN bonds=%d ad_restarts=%lu last_disconnect=%d\n",
    s_deviceName[0] ? s_deviceName : "unavailable", s_initialized ? "YES" : "NO",
    s_connected ? "YES" : "NO", s_controlAllowed ? "YES" : "NO",
    s_advertising && s_advertising->isAdvertising() ? "YES" : "NO", bonds,
    (unsigned long)s_advertisingRestartCount, s_lastDisconnectReason);
}

void ble_transport_process() {
  if (!s_connected) ensureAdvertising(false);
  if (s_stopRequested) {
    s_stopRequested = false;
    discardPendingCommand();
    scheduler_stop_all();
    notifyStatus("stopped_link_or_emergency");
    return;
  }
  if (s_connected && scheduler_active_count() &&
      millis() - s_lastValidCommandAt > BLE_COMMAND_WATCHDOG_MS) {
    scheduler_stop_all();
    notifyStatus("stopped_watchdog");
  }
  if (s_lineLength && s_fragmentStartedAt &&
      millis() - s_fragmentStartedAt > BLE_FRAGMENT_TIMEOUT_MS) {
    portENTER_CRITICAL(&s_bleMux);
    s_lineLength = 0;
    s_lineOverflow = false;
    s_fragmentStartedAt = 0;
    portEXIT_CRITICAL(&s_bleMux);
    indicate("N 0 fragment_timeout");
  }
  if (!s_pendingReady) return;
  char line[BLE_COMMAND_BUFFER_SIZE];
  portENTER_CRITICAL(&s_bleMux);
  memcpy(line, s_pending, sizeof(line));
  s_pendingReady = false;
  portEXIT_CRITICAL(&s_bleMux);
  if (s_stopRequested || !s_connected || !s_controlAllowed) {
    discardPendingCommand();
    return;
  }
  unsigned long sequence = 0;
  char* command = nullptr;
  if (!extractSequence(line, sequence, command)) {
    indicate("N 0 invalid_frame");
    return;
  }
  ResponsePrint response;
  const bool ok = comandos_executar(command, response);
  char framed[BLE_RESPONSE_BUFFER_SIZE + 24];
  snprintf(framed, sizeof(framed), "%c %lu %s", ok ? 'A' : 'N', sequence, response.c_str());
  indicate(framed);
  if (ok) {
    s_lastValidCommandAt = millis();
    notifyStatus("command_ok");
  }
}

#else
void ble_transport_begin() {}
void ble_transport_process() {}
bool ble_transport_enable_pairing() { return false; }
bool ble_transport_clear_bonds() { return false; }
bool ble_transport_is_connected() { return false; }
void ble_transport_print_status(Print& output) { output.println(F("[BLE] Desabilitado em Config.h.")); }
#endif
