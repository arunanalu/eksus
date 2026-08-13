#pragma once

// UUIDs privados e estáveis do protocolo Exus BLE v1. Não os altere sem
// incrementar EXUS_BLE_PROTOCOL_VERSION e atualizar tools/exus_ble.py.
#define EXUS_BLE_PROTOCOL_VERSION 1
#define EXUS_BLE_SERVICE_UUID       "68e5d2c0-3df1-4b6a-8eb8-5ce08ad10001"
#define EXUS_BLE_COMMAND_UUID       "68e5d2c0-3df1-4b6a-8eb8-5ce08ad10002"
#define EXUS_BLE_RESPONSE_UUID      "68e5d2c0-3df1-4b6a-8eb8-5ce08ad10003"
#define EXUS_BLE_STATUS_UUID        "68e5d2c0-3df1-4b6a-8eb8-5ce08ad10004"
#define EXUS_BLE_DEVICE_INFO_UUID   "68e5d2c0-3df1-4b6a-8eb8-5ce08ad10005"
#define EXUS_BLE_EMERGENCY_UUID     "68e5d2c0-3df1-4b6a-8eb8-5ce08ad10006"
