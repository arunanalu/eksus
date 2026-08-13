#pragma once

// Transporte BLE/GATT. Todas as mutações hápticas ocorrem em
// ble_transport_process(), no loop Arduino — nunca no callback da pilha BLE.
void ble_transport_begin();
void ble_transport_process();
void ble_transport_enable_pairing();
bool ble_transport_clear_bonds();
bool ble_transport_is_connected();
