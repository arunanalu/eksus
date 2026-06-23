# Eksus Firmware — Guia de Início 

Firmware para **ESP32-C3 + DRV2605 + motor LRA** que cria sensações hápticas
controlando o ritmo de pulsos de vibração pela porta USB Serial

> Para entender *por que* as decisões foram tomadas desta forma, leia [`docs/PLAN.md`](docs/PLAN.md).

---

## Índice

1. [O que você vai precisar](#1-o-que-você-vai-precisar)
2. [Ligações de hardware](#2-ligações-de-hardware)
3. [Instalar o ambiente de software](#3-instalar-o-ambiente-de-software)
4. [Configurar o código para o seu motor](#4-configurar-o-código-para-o-seu-motor)
5. [Carregar o firmware na placa](#5-carregar-o-firmware-na-placa)
6. [Sequência de testes](#6-sequência-de-testes)
7. [Referência de comandos](#7-referência-de-comandos)
8. [Solução de problemas](#8-solução-de-problemas)

---

## 1. O que você vai precisar

| Item | Observação |
|------|-----------|
| Placa ESP32-C3 (dev board com USB) | Qualquer dev board ESP32-C3 com conector USB serve |
| Módulo DRV2605 ou DRV2605L (breakout) | O da **Adafruit** já vem com resistores I2C e facilita muito |
| Motor **LRA** moeda | Deve ser LRA — **não ERM**. Anote a tensão nominal e a frequência de ressonância do datasheet |
| Cabo USB de **dados** | Cabos só de carga não funcionam para programação |
| Protoboard + jumpers | Para os testes iniciais |

> **Motor LRA vs. ERM:** motores LRA são "ressonantes" (flat, como moeda).
> Motores ERM são cilíndricos com massa excêntrica. O código configura o DRV2605
> em modo LRA — com um motor ERM ele não funcionará corretamente.

---

## 2. Ligações de hardware

Faça toda a montagem com a placa **desconectada da USB**. Só conecte após conferir.

```
ESP32-C3        DRV2605 (Adafruit breakout)
─────────       ───────────────────────────
3V3        →    VIN
GND        →    GND          ← obrigatório para o I2C funcionar
GPIO 8     →    SDA
GPIO 9     →    SCL

DRV2605 OUT+ / OUT-  →  terminais do motor LRA
```

> **Se sua placa usar pinos I2C diferentes** de GPIO8/GPIO9, abra
> [`firmware/Config.h`](firmware/Config.h) e altere `SDA_PIN` e `SCL_PIN`.

**Cuidados importantes:**

- O GND **precisa ser comum** entre o ESP32 e o DRV2605. Sem isso o I2C não funciona.
- Os pinos I2C do ESP32-C3 operam em **3,3 V**. Não conecte 5 V neles.
- **Nunca ligue o motor diretamente ao ESP32** — sempre passe pelo DRV2605.
- Em protoboard a vibração solta jumpers com o tempo. Pressione os conectores
  antes de cada teste e, se possível, fixe a placa para sentir a vibração de verdade.

---

## 3. Instalar o ambiente de software

### 3.1 Arduino IDE

Baixe e instale a **Arduino IDE 2.x** em <https://www.arduino.cc/en/software>.

### 3.2 Suporte às placas ESP32

Dentro da Arduino IDE:

1. Abra **File → Preferences**.
2. Em *Additional boards manager URLs*, adicione:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Abra **Tools → Board → Boards Manager**, pesquise `esp32` e instale
   o pacote da **Espressif Systems**.

### 3.3 Biblioteca do DRV2605

1. Abra **Sketch → Include Library → Manage Libraries**.
2. Pesquise `Adafruit DRV2605` e instale a biblioteca **Adafruit DRV2605 Library**.
   - A dependência `Adafruit BusIO` será instalada automaticamente junto.

### 3.4 Selecionar a placa e a porta

1. **Tools → Board → esp32 → ESP32C3 Dev Module**
2. **Tools → Port** → selecione a porta COM que apareceu ao conectar o ESP32

---

## 4. Configurar o código para o seu motor

Abra [`firmware/Config.h`](firmware/Config.h). Este é o **único arquivo que você
precisa editar** para ajustar o comportamento básico.

### 4.1 Tensão do motor LRA (obrigatório)

Localize as duas linhas abaixo e substitua pelos valores do datasheet do **seu** motor:

```c
// Tensão nominal RMS do motor → estimativa: (V_rms / 1.8) × 90
#define LRA_RATED_VOLTAGE_REG   0x3E   // padrão: ~1,8 V rms

// Tensão de pico máxima → estimativa: 1,5× a 2× a tensão nominal
#define LRA_OD_CLAMP_REG        0x89   // padrão: ~3,2 V pico
```

Referência rápida de valores comuns:

| Tensão nominal do motor | `LRA_RATED_VOLTAGE_REG` | `LRA_OD_CLAMP_REG` |
|------------------------|------------------------|-------------------|
| ~1,8 V rms             | `0x3E`                 | `0x89`            |
| ~2,0 V rms             | `0x50`                 | `0x8C`            |
| ~2,5 V rms             | `0x64`                 | `0xA4`            |

> Se não souber os valores exatos, comece com os padrões. O código ainda
> funcionará; a auto-calibração pode reportar aviso, mas o motor deve vibrar.
> Ajuste progressivamente até a vibração ficar estável.

### 4.2 Limites de segurança (opcional — padrões conservadores)

```c
#define MAX_INTENSITY_PCT   50    // teto de intensidade em %
#define MAX_DURATION_MS     5000  // duração máxima por ativação (ms)
#define MIN_COOLDOWN_MS     300   // pausa mínima entre ativações (ms)
```

Comece com esses valores conservadores e suba `MAX_INTENSITY_PCT` só depois de
validar que a vibração está confortável na bancada.

---

## 5. Carregar o firmware na placa

1. Abra o arquivo **`firmware/firmware.ino`** na Arduino IDE.
   - A IDE abrirá automaticamente todos os outros arquivos da pasta (`Config.h`,
     `DriverHaptico.cpp`, etc.) como abas do mesmo sketch.

2. Clique em **Upload** (botão com seta →).

3. **Se o upload travar** (barra de progresso parada por mais de 10 s):
   - Segure o botão **BOOT** da placa, clique em Upload novamente e solte BOOT
     assim que a mensagem *"Connecting..."* aparecer no console.
   - Isso é normal em algumas versões do ESP32-C3.

4. Após o upload, abra o **Serial Monitor**:
   - **Tools → Serial Monitor**
   - Configure a velocidade para **115200 baud**
   - Pressione o botão **Reset** (ou **EN**) na placa

5. A saída esperada no Serial Monitor:

   ```
   ==========================================
     Exus Firmware — ESP32-C3 + DRV2605 LRA
   ==========================================

   [INFO] I2C iniciado: SDA=GPIO8  SCL=GPIO9
   [INFO] Procurando DRV2605 em 0x5A... OK
   [INFO] Inicializando DRV2605... OK
   [INFO] Calibrando motor LRA (ate 1.5s)... OK

   [OK] Sistema pronto. Digite 'h' para ver os comandos.
   ------------------------------------------
   ```

   Se aparecer `NAO ENCONTRADO` no scan I2C, vá para a seção
   [Solução de problemas](#8-solução-de-problemas).

---

## 6. Sequência de testes

Siga **essa ordem** para reduzir riscos. Não pule etapas.

### Teste 1 — Verificar comunicação I2C

No Serial Monitor, digite:

```
scan
```

Resultado esperado: `[OK] DRV2605 (0x5A): detectado`

Se não aparecer, o problema é de hardware — veja [Solução de problemas](#8-solução-de-problemas).

---

### Teste 2 — Verificar boot sem acionar o motor

Reinicie a placa (botão Reset) e observe as mensagens de inicialização.
O motor **não deve vibrar** durante o boot. Se vibrar, há algo errado na inicialização.

---

### Teste 3 — Primeiro pulso (intensidade mínima)

```
v 10 15 500
```

Isso aciona **10 Hz, 15% de intensidade, por 500 ms** — o mínimo suficiente
para sentir se o motor está funcionando. Coloque a placa sobre a mesa e toque
com o dedo para sentir a vibração.

O motor **não deve esquentar** após um pulso curto. Se esquentar anormalmente,
interrompa os testes e revise as ligações.

---

### Teste 4 — Frequências percebidas

Experimente diferentes frequências e anote suas impressões:

```
v 5 30 2000
v 10 30 2000
v 20 30 2000
v 30 30 2000
v 60 30 2000
```

- Em frequências baixas (5–15 Hz) você deve sentir pulsos **distintos** e lentos.
- Em frequências altas (50–60 Hz) os pulsos tendem a se fundir em vibração
  contínua (isso é esperado — é o limite físico do LRA).
- Documente qual faixa produz a sensação mais útil para o projeto.

---

### Teste 5 — Parada de emergência

1. Inicie uma vibração longa:
   ```
   v 20 30 0
   ```
   *(0 ms = contínuo)*

2. Sem esperar, envie o comando de emergência:
   ```
   e
   ```

3. O motor deve parar **imediatamente**. O Serial Monitor deve mostrar:
   ```
   [EMERGENCIA] Sistema parado. Digite 'r' para retomar.
   ```

4. Retome o sistema:
   ```
   r
   ```

Este teste é obrigatório antes de qualquer uso próximo ao corpo.

---

### Teste 6 — Estabilidade (sequências repetidas)

Execute 5 a 10 ativações consecutivas e observe:

```
v 30 40 1000
v 30 40 1000
v 30 40 1000
```

A placa não deve reiniciar sozinha entre os comandos. Se reiniciar, veja
[ESP32 reinicia ao vibrar](#esp32-reinicia-sozinho-ao-vibrar) na solução de problemas.

---

## 7. Referência de comandos

Todos os comandos são enviados pelo Serial Monitor (115200 baud), um por linha.

| Comando | Descrição | Exemplo |
|---------|-----------|---------|
| `v <freq> <intens%> [ms]` | Vibrar com frequência percebida. `ms` omitido = contínuo | `v 30 40 2000` |
| `s` | Parar a vibração atual | `s` |
| `e` | **Parada de emergência** — bloqueia tudo imediatamente | `e` |
| `r` | Retomar após emergência | `r` |
| `ef <1-123>` | Tocar efeito built-in da biblioteca do DRV2605 (LRA) | `ef 14` |
| `status` | Exibir estado atual (emergência, limites configurados) | `status` |
| `scan` | Re-verificar presença do DRV2605 no I2C | `scan` |
| `h` ou `?` | Mostrar ajuda na própria Serial | `h` |

**Parâmetros do comando `v`:**

| Parâmetro | Intervalo válido | Padrão se omitido |
|-----------|-----------------|-------------------|
| `freq` (Hz) | 1 – 100 | — (obrigatório) |
| `intens%` | 0 – 50* | — (obrigatório) |
| `ms` | 0 – 5000 | 0 (contínuo) |

*O teto de intensidade (`MAX_INTENSITY_PCT = 50`) pode ser ajustado em `Config.h`.
Valores acima do teto são automaticamente recortados.

---

## 8. Solução de problemas

### DRV2605 não encontrado (scan retorna erro)

Causa mais comum: problema de hardware.

1. Confirme que o **GND é comum** entre ESP32 e DRV2605. Essa é a causa #1.
2. Confira que `VIN` do DRV2605 está em **3,3 V** (não 5 V).
3. Tente inverter os fios SDA/SCL — é fácil cruzar.
4. Em protoboard, verifique se nenhum jumper saiu do lugar.
5. Tente outro cabo USB (alguns cabos têm mau contato).
6. Se tiver multímetro, meça a tensão entre VIN e GND do módulo: deve ser ~3,3 V.

---

### Calibração retorna AVISO

O motor ainda deve funcionar. O aviso significa que o DRV2605 não conseguiu
ajustar automaticamente para os parâmetros do motor com os valores atuais em
`Config.h`.

Ajuste `LRA_RATED_VOLTAGE_REG` e `LRA_OD_CLAMP_REG` com os valores do datasheet
do seu motor (veja a [tabela da seção 4.1](#41-tensão-do-motor-lra-obrigatório)).
Faça uploads de teste até a calibração passar sem aviso.

---

### Motor não vibra após comando `v`

1. Verifique se o comando `scan` passa antes — sem comunicação I2C nada funciona.
2. Confira que os terminais `OUT+` e `OUT-` do DRV2605 estão conectados ao motor.
   A polaridade não importa para motores LRA.
3. Tente intensidade mais alta para confirmar que não é só o motor "fraco":
   ```
   v 30 50 1000
   ```
4. Teste um efeito built-in para isolar se o problema é no modo RTP:
   ```
   ef 1
   ```
   Se o efeito built-in funcionar mas `v` não, o problema é na calibração/configuração.

---

### ESP32 reinicia sozinho ao vibrar

Causa: queda de tensão na alimentação USB.

1. Troque o cabo USB por um cabo mais grosso (menor resistência).
2. Use uma porta USB diretamente no computador (não em hub sem fonte).
3. Se persistir, adicione um **capacitor eletrolítico de 100–470 µF** entre 3V3 e GND
   na protoboard (amortece o pico de corrente do motor na partida).

---

### Upload falha ("Connecting..." trava)

1. Segure o botão **BOOT** da placa antes de clicar em Upload.
2. Solte BOOT assim que aparecer `Connecting...` no console da IDE.
3. Confirme que a porta COM correta está selecionada em **Tools → Port**.
4. Em Windows, às vezes é necessário instalar o driver CH340 ou CP210x
   (dependendo do chip USB da sua placa).

---

## Estrutura do firmware

```
firmware/
├── firmware.ino        ← ponto de entrada (setup + loop)
├── Config.h            ← todas as constantes editáveis
├── DriverHaptico.h/.cpp  ← comunicação com o DRV2605 via I2C
├── GeradorEnvelope.h/.cpp  ← gerador de pulsos não-bloqueante
├── Seguranca.h/.cpp    ← limites de segurança e emergência
└── Comandos.h/.cpp     ← interface USB Serial
```

Para entender a lógica completa e os próximos passos de evolução
(multiplexador, múltiplos motores, BLE), leia [`docs/PLAN.md`](docs/PLAN.md).
