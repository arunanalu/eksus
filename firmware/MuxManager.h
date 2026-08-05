#pragma once
#include <Arduino.h>

void mux_begin();
bool mux_detect(uint8_t address);
bool mux_select_exclusive(uint8_t address, uint8_t channel);
void mux_disable_all();
int16_t mux_active_path();

