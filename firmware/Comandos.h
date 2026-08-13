#pragma once

#include <Arduino.h>

// Lê e processa comandos chegando pela USB Serial.
// Deve ser chamado a cada iteração do loop().
void comandos_processar();

// Executa uma linha vinda de qualquer transporte. O chamador é responsável por
// limitar/fracionar a linha antes desta chamada. Retorna false para sintaxe ou
// pedido rejeitado; a razão é escrita em output.
bool comandos_executar(const char* linha, Print& output);
