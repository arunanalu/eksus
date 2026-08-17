# Eksus Firmware — Guia de Início 

Firmware para **ESP32-C3 + TCA9548A + DRV2605L + motores LRA e ERM**. Funciona tanto
com uma zona ligada diretamente quanto com uma quantidade descoberta dinamicamente
de multiplexadores e atuadores, controlados pela USB Serial ou por Bluetooth LE.

> Para entender *por que* as decisões foram tomadas desta forma, leia a
> [`SPEC-001`](docs/SPEC-001.md). A evolução do firmware para múltiplas zonas
> está na [`SPEC-002`](docs/SPEC-002.md). O transporte Bluetooth LE e OTA está
> na [`SPEC-003`](docs/SPEC-003.md). O aplicativo visual de controle BLE está
> na [`SPEC-003.5`](docs/SPEC-003.5.md) e sua evolução para ponte de jogos está
> na [`SPEC-004`](docs/SPEC-004.md). A demo Godot está na
> [`SPEC-005`](docs/SPEC-005.md).

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
9. [Bluetooth: primeira conexão](#9-bluetooth-primeira-conexão)

---

## 1. O que você vai precisar

| Item | Observação |
|------|-----------|
| Placa ESP32-C3 (dev board com USB) | Qualquer dev board ESP32-C3 com conector USB serve |
| Módulo DRV2605 ou DRV2605L (breakout) | O da **Adafruit** já vem com resistores I2C e facilita muito |
| Motor háptico por zona | LRA ou ERM, conforme o perfil configurado. Anote tensão nominal e, para LRA, frequência de ressonância |
| TCA9548A (opcional) | Um multiplexador oferece 8 canais; endereços `0x70` a `0x77` permitem descobrir até 8 multiplexadores |
| Cabo USB de **dados** | Cabos só de carga não funcionam para programação |
| Protoboard + jumpers | Para os testes iniciais |

> **Motor LRA vs. ERM:** LRA é ressonante; ERM usa massa excêntrica. O firmware
> seleciona o modo por zona. Na montagem atual, as zonas 0 e 1 são moedas ERM
> 1020 (3 V) e a zona 2 é o bastão LRA 0619AAC (1,2 Vrms, 170 Hz).

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

Para múltiplas zonas, ligue o barramento principal ao TCA9548A e, em cada canal,
um DRV2605L com seu LRA. Cada canal comporta um atuador independente. Os pinos
`A0/A1/A2` do TCA definem os endereços `0x70` a `0x77`; não repita endereços.
O firmware varre todos esses endereços e todos os canais no boot.

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

### 3.4 Biblioteca Bluetooth

1. Abra **Sketch → Include Library → Manage Libraries**.
2. Pesquise `NimBLE-Arduino` e instale a biblioteca de mesmo nome, versão 2.x.

Ela é necessária para compilar a versão do firmware que anuncia Bluetooth LE.

### 3.5 Selecionar a placa e a porta

1. **Tools → Board → esp32 → ESP32C3 Dev Module**
2. **Tools → Port** → selecione a porta COM que apareceu ao conectar o ESP32

---

## 4. Configurar o código para o seu motor

Abra [`firmware/Config.h`](firmware/Config.h). Este é o **único arquivo que você
precisa editar** para ajustar o comportamento básico.

O padrão do protótipo atual é TCA9548A e **não precisa ser alterado**:

```c
#define EXUS_USE_TCA9548A 1  // descobre TCAs 0x70..0x77 e até 64 canais
```

Use `0` somente para uma bancada de uma zona, com o DRV ligado diretamente ao
ESP32 e sem TCA9548A.

Com multiplexadores, o ID lógico é estável: `zone = (mux - 1) * 8 + canal`.
Assim, mux 1/canal 0 é zona 0 e mux 4/canal 2 é zona 26. Um mux ou canal ausente
fica marcado como indisponível e nunca é redirecionado para outra zona.

### 4.1 Perfis dos motores (obrigatório)

Em `Config.h`, os perfis atuais já distinguem as moedas ERM das zonas 0 e 1 e o
bastão LRA da zona 2. Para ERM em modo aberto, `OD_CLAMP` limita a amplitude;
para LRA, tensão RMS, clamp e `DRIVE_TIME` participam da calibração. Confirme os
valores na bancada antes de qualquer uso corporal.

```c
// ERM: referência de amplitude em modo aberto (aprox. 3 V)
#define ERM_COIN_OD_CLAMP_REG   0x8B   // moedas ERM 1020, 3 V

// LRA: tensão RMS, clamp de pico e tempo inicial de acionamento
#define BAR_LRA_RATED_VOLTAGE_REG 0x32 // bastão LRA 0619AAC, 1,2 Vrms
#define BAR_LRA_OD_CLAMP_REG      0x50 // clamp conservador: ~1,70 Vp
#define BAR_LRA_DRIVE_TIME_REG    0x18 // estimativa inicial para 170 Hz
```

Referência rápida de valores comuns:

| Zonas | Tipo/modelo | Perfil |
|-------|-------------|--------|
| 0 e 1 | ERM moeda 1020, 3 V | `ERM_COIN_OD_CLAMP_REG=0x8B`; modo aberto, sem auto-calibração LRA |
| 2 | LRA bastão 0619AAC, 1,2 Vrms / 170 Hz | `BAR_LRA_*`; calibração LRA sequencial |

> Para o LRA, a auto-calibração pode reportar aviso enquanto a configuração ou
> fixação ainda não estiver correta; ajuste somente em bancada. Para o ERM,
> valide `OD_CLAMP` com pulsos curtos, pois ele opera em modo aberto.

### 4.2 Limites de segurança (opcional — padrões conservadores)

```c
#define MAX_INTENSITY_PCT   50    // teto de intensidade em %
#define MAX_DURATION_MS     5000  // duração máxima por ativação (ms)
#define MIN_COOLDOWN_MS     300   // pausa mínima entre ativações (ms)
#define MAX_SIMULTANEOUS_ZONES 8  // teto de zonas ativas
#define MAX_GLOBAL_AMPLITUDE 320  // soma máxima de amplitudes RTP
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
   === Exus Firmware multi-zona ===
   [INFO] I2C SDA=8 SCL=9 clock=400000 Hz
   [INFO] Topologia=TCA9548A dinamico; descobrindo hardware e calibrando sequencialmente...
   [INFO] Descoberta concluida: 3 de 64 zonas prontas.
   [OK] Digite 'zones' para diagnostico ou 'h' para ajuda.
   ```

   A contagem varia com a montagem. Se nenhuma zona ficar `READY`, vá para a seção
   [Solução de problemas](#8-solução-de-problemas).

---

## 6. Sequência de testes

Siga **essa ordem** para reduzir riscos. Não pule etapas.

### Teste 1 — Verificar comunicação I2C

No Serial Monitor, digite:

```
zones
```

Resultado esperado: cada TCA presente e seus canais com `READY`, `DRV_MISSING`
ou uma falha explícita. Em ligação direta, deve aparecer `zone=0 direct status=READY`.

Se não aparecer, o problema é de hardware — veja [Solução de problemas](#8-solução-de-problemas).

---

### Teste 2 — Verificar boot sem acionar o motor

Reinicie a placa fora do corpo e observe as mensagens. Com `CALIBRATE_ON_BOOT=1`,
cada motor presente pode vibrar brevemente, sempre de modo sequencial. Depois da
descoberta, todos devem permanecer parados até um comando.

---

### Teste 3 — Primeiro pulso (intensidade mínima)

```
pulse 0 15 500 10
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
pulse 0 30 2000 5
pulse 0 30 2000 10
pulse 0 30 2000 20
pulse 0 30 2000 30
pulse 0 30 2000 60
```

- Em frequências baixas (5–15 Hz) você deve sentir pulsos **distintos** e lentos.
- Em frequências altas (50–60 Hz) os pulsos tendem a se fundir em vibração
  contínua (isso é esperado — é o limite físico do LRA).
- Documente qual faixa produz a sensação mais útil para o projeto.

---

### Teste 5 — Parada de emergência

1. Inicie uma vibração longa:
   ```
   pulse 0 30 5000 20
   ```

2. Sem esperar, envie o comando de emergência:
   ```
   emergency
   ```

3. O motor deve parar **imediatamente**. O Serial Monitor deve mostrar:
   ```
   [EMERGENCIA] Todas as zonas paradas.
   ```

4. Retome o sistema:
   ```
   resume
   ```

Este teste é obrigatório antes de qualquer uso próximo ao corpo.

---

### Teste 6 — Estabilidade (sequências repetidas)

Execute 5 a 10 ativações consecutivas e observe:

```
pulse 0 40 1000 30
pulse 0 40 1000 30
pulse 0 40 1000 30
```

A placa não deve reiniciar sozinha entre os comandos. Se reiniciar, veja
[ESP32 reinicia ao vibrar](#esp32-reinicia-sozinho-ao-vibrar) na solução de problemas.

### Teste 7 — Descoberta e isolamento multi-zona

Com `EXUS_USE_TCA9548A=1`, execute `zones` e confira a correspondência física de
cada zona `READY` com pulsos individuais. Depois teste um mux presente e outro
ausente:

```
mux 1 pulse 20 300 20
mux 4 pulse 20 300 20
```

O primeiro aciona somente os canais prontos do mux 1. Se o mux 4 não existir, a
resposta informa `nenhum atuador acionado` e nenhum outro motor pode vibrar.
Repita removendo um DRV por vez e confirme que as zonas saudáveis continuam
endereçáveis. Antes de uso corporal, ainda são obrigatórios os ensaios incrementais
de corrente, temperatura, jitter, crosstalk e emergência definidos na SPEC-002.

---

## 7. Referência de comandos

Todos os comandos são enviados pelo Serial Monitor (115200 baud), um por linha.

| Comando | Descrição | Exemplo |
|---------|-----------|---------|
| `zones` ou `scan all` | Listar muxes, canais e estados descobertos | `zones` |
| `pulse <zona> <intens%> <ms> [Hz]` | Envelope RTP em uma zona | `pulse 10 30 800 25` |
| `effect <zona> <1-123>` | Efeito ROM em uma zona | `effect 10 14` |
| `mux <1-8> pulse <intens%> <ms> [Hz]` | Pulso em todos os atuadores prontos do mux | `mux 2 pulse 25 500 30` |
| `mux <1-8> effect <1-123>` | Efeito em todos os atuadores prontos do mux | `mux 2 effect 14` |
| `group <máscara> ...` | Acionar uma máscara lógica de até 64 zonas | `group 0x03 effect 14` |
| `stop <zona\|mux:N\|all>` | Parar uma zona, mux ou tudo | `stop mux:2` |
| `emergency` / `resume` | Parada global e liberação após inspeção | `emergency` |
| `status [zona]` | Diagnóstico global ou detalhado | `status 10` |
| `Q [seq]` | Capacidades e lista de zonas prontas | `Q 42` |
| `h` ou `?` | Mostrar ajuda na própria Serial | `h` |

Os aliases antigos `v`, `ef`, `s`, `e` e `r` continuam disponíveis e operam na
zona 0. Toda ativação expira no teto configurado, inclusive quando a duração
enviada é zero. Pedidos para muxes ou zonas ausentes não acionam outro motor.

**Parâmetros de pulsos RTP:**

| Parâmetro | Intervalo válido | Padrão se omitido |
|-----------|-----------------|-------------------|
| `freq` (Hz) | 1 – 100 | — (obrigatório) |
| `intens%` | 0 – 50* | — (obrigatório) |
| `ms` | 1 – 5000 | obrigatório nos comandos novos |

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

## 9. Bluetooth: primeira conexão

O ESP32-C3 já possui Bluetooth LE. Não conecte outro módulo Bluetooth nem altere
os fios I2C para usar esta função. Bluetooth troca apenas o cabo de **dados**:
ele não alimenta o protótipo.

### Preparar o PC que ficará com o protótipo

O PC precisa de Windows 10/11 com Bluetooth LE ativo (ou adaptador USB BLE) e
das bibliotecas Arduino já descritas, incluindo **NimBLE-Arduino**. Não é
necessário instalar um programa Bluetooth adicional no Windows.

Para o uso normal, entregue a pasta `Exus-Control` gerada pelo projeto e peça
para a pessoa abrir `Exus-Control.exe`. Nesse caso, **não é necessário instalar
Python, `bleak` ou usar terminal**. A instalação de NimBLE-Arduino é diferente:
ela serve para compilar o firmware do ESP32 e só é necessária no computador que
fará upload pela Arduino IDE.

Caso ainda não exista a pasta do aplicativo, alguém do time de desenvolvimento
deve gerá-la uma vez, no repositório:

```powershell
powershell -ExecutionPolicy Bypass -File exus_control\build_windows.ps1
```

Depois, copiar ou zipar toda a pasta `exus_control\dist\Exus-Control` — não apenas o
arquivo `.exe` — e levá-la ao PC que ficará com o protótipo.

### Primeira conexão pelo aplicativo Exus Control — recomendada

1. Mantenha o ESP32 conectado ao PC por **cabo USB de dados**. Faça upload do
   firmware normalmente pela Arduino IDE; esta primeira gravação é por USB.
2. Abra o Serial Monitor em **115200 baud**. Execute `zones`, `Q` e `emergency`;
   confirme que todos os motores estão parados antes de seguir.
3. No Serial Monitor, envie `ble pair enable`. Isso libera o primeiro
   pareamento por 60 s. Deixe o protótipo na mesa, fora do corpo.
4. Abra `Exus-Control.exe` por duplo clique. Se o Windows pedir permissão para
   Bluetooth, permita.
5. Clique em **Procurar protótipos**. Selecione o item `Exus-XXXXXX` encontrado
   e clique em **Conectar**. Aceite a confirmação de pareamento do Windows, se
   ela aparecer.
6. A tela deve mudar para **CONECTADO — pronto para teste** e mostrar as zonas
   prontas. Se aparecer “nenhuma zona pronta”, pare e revise a montagem/Serial.
7. Marque **uma** zona. Deixe os valores iniciais seguros (15%, 500 ms e 10 Hz)
   e clique em **Testar zonas marcadas**. Confirme a vibração na mesa.
8. Clique em **PARAR TUDO** e depois em **EMERGÊNCIA — PARAR AGORA**. Em ambos
   os casos, todos os motores precisam parar imediatamente.
9. Para testar duas ou mais zonas juntas, marque mais de uma caixa e clique em
   **Testar zonas marcadas**. O aplicativo envia uma única solicitação de grupo;
   o firmware mantém a decisão final de segurança e pode limitar/recusar o
   pedido conforme o orçamento de energia.
10. Clique em **Desconectar** ou desligue o Bluetooth do PC. A tela deve indicar
    a perda de conexão e todos os motores devem parar. Só então o BLE está
    aprovado para o próximo teste.

### Reconectar depois de reiniciar ou usar bateria

O bond criado no primeiro pareamento fica salvo no PC e no ESP32. Portanto,
depois de desligar USB, ligar a bateria ou reiniciar a placa, basta abrir o
Exus Control, procurar `Exus-XXXXXX` e clicar em **Conectar**. **Não** execute
`ble pair enable` novamente: a janela é apenas para o primeiro PC e sempre
fecha no boot. O firmware reativa a criptografia do bond persistente antes de
aceitar comandos.

Se o aplicativo encontrar o Exus, mas a conexão falhar ou mostrar
`Unreachable`, faça a recuperação uma única vez com USB conectado:

1. Feche Exus Control, cliente de terminal e qualquer scanner BLE.
2. No Windows, remova/esqueça `Exus-XXXXXX` em **Configurações > Bluetooth e
   dispositivos**, se ele estiver listado.
3. No Serial Monitor, envie `ble bonds clear`, reinicie a placa e envie
   `ble pair enable`.
4. Dentro de 60 s, conecte pelo Exus Control e conclua o pareamento.

`ble pair enable` só abre quando não há conexão nem bond salvo. Essa regra
impede que outro PC substitua silenciosamente o computador autorizado. Para
trocar de PC, execute a recuperação acima de propósito.

> Um dispositivo BLE pode não aparecer no menu **Configurações > Bluetooth** do
> Windows como um fone de ouvido. Isso não é falha: use a busca dentro do Exus
> Control.

> Se a busca não listar nenhum `Exus-XXXXXX`, reconecte a USB e confirme no
> Serial Monitor a mensagem `[BLE] Anunciando Exus-...` e envie `ble status`.
> O resultado deve conter `advertising=YES`. Se aparecer `[ERRO] BLE nao iniciou
> anuncio` ou `advertising=NO`, copie a linha completa. Atualize primeiro o
> firmware e depois recrie/obtenha a versão atual do Exus Control; ambos precisam
> estar na mesma versão do repositório.

### Alternativa: cliente de terminal (somente bancada/desenvolvimento)

O cliente abaixo é útil para diagnóstico técnico, mas o grupo deve preferir o
aplicativo visual acima. Para esta alternativa, Python 3.10+ precisa estar
instalado e as dependências precisam ser adicionadas uma vez:

Depois de baixar o repositório, abrir PowerShell na pasta do projeto e executar:

```powershell
python -m pip install -r exus_control\requirements.txt
```

Isso instala o cliente `bleak` que procura e envia comandos BLE. Não basta
instalar NimBLE-Arduino, pois a biblioteca Arduino fica no ESP32 e o cliente
Python roda no PC.

Depois, com o pareamento já liberado pela Serial, execute:

```powershell
python -m exus_control.cli scan
python -m exus_control.cli connect --id A1B2C3 info
python -m exus_control.cli connect --id A1B2C3 command "pulse 0 15 500 10"
python -m exus_control.cli connect --id A1B2C3 command emergency
```

Substitua `A1B2C3` pelo identificador mostrado pelo comando `scan`.

### Usar bateria depois do teste

Depois de o BLE funcionar com USB, o cabo pode ser removido e o protótipo pode
ser alimentado por uma bateria/fonte **já validada**. Não conecte USB e bateria
ao mesmo tempo sem circuito de gerenciamento de energia. Nunca ligue uma bateria
crua no pino `3V3`; use a entrada/regulador e a proteção próprios da placa.

A fonte precisa suportar a corrente dos motores, manter GND comum com ESP32/TCA/
DRV2605 e não causar reinício durante vibração. O primeiro teste com bateria é
sempre na mesa. Após o primeiro pareamento bem-sucedido, reinicie a placa pela
bateria e confirme que o Exus Control reconecta sem `ble pair enable`; execute
um pulso mínimo, **PARAR TUDO**, emergência e uma desconexão deliberada. Para
atualizar firmware, apagar pareamentos (`ble bonds clear`) ou recuperar uma
falha, reconecte a USB.

---

## Estrutura do firmware

```
firmware/
├── firmware.ino        ← ponto de entrada (setup + loop)
├── Config.h            ← todas as constantes editáveis
├── MuxManager.h/.cpp   ← descoberta e seleção exclusiva de TCA/canal
├── ZoneMap.h/.cpp      ← IDs lógicos e topologia direta/dinâmica
├── ZoneDriver.h/.cpp   ← roteamento e diagnóstico por atuador
├── MultiZoneScheduler.h/.cpp ← RTP/ROM independente por zona
├── DriverHaptico.h/.cpp  ← acesso de baixo nível ao DRV2605
├── GeradorEnvelope.h/.cpp  ← compatibilidade da API de uma zona
├── Seguranca.h/.cpp    ← limites zonais/globais e emergência
├── Comandos.h/.cpp     ← parser comum para USB Serial e BLE
├── BleProtocol.h       ← UUIDs e versão do protocolo BLE
└── BleTransport.h/.cpp ← GATT, pareamento, watchdog e parada por desconexão
```

O cliente de bancada faz parte de `exus_control/`; execute
`python -m exus_control.cli`. Suas dependências estão em
`exus_control/requirements.txt`.

O aplicativo visual está em `exus_control/`; para desenvolvimento, execute
`python exus_control_app.py`. Para gerar uma versão distribuível para Windows,
execute `powershell -ExecutionPolicy Bypass -File exus_control\build_windows.ps1`.
O arquivo a entregar é `exus_control\dist\Exus-Control\Exus-Control.exe`; o
usuário final abre esse programa sem instalar Python nem usar terminal.

Para entender a lógica completa e os próximos passos de evolução, consulte:

- [`docs/SPEC-001.md`](docs/SPEC-001.md) — firmware, hardware e segurança do MVP;
- [`docs/SPEC-002.md`](docs/SPEC-002.md) — evolução independente do firmware para multiplexadores e múltiplas zonas;
- [`docs/SPEC-003.md`](docs/SPEC-003.md) — transporte Bluetooth LE, comandos e OTA;
- [`docs/SPEC-003.5.md`](docs/SPEC-003.5.md) — aplicativo visual Windows para controle BLE;
- [`docs/SPEC-004.md`](docs/SPEC-004.md) — evolução do Exus Control para ponte de jogos.
- [`docs/SPEC-005.md`](docs/SPEC-005.md) — demo Godot desenvolvida e testada primeiro sem hardware.
