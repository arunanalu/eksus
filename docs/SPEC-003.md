# SPEC-003 — Transporte Bluetooth LE e atualização OTA do Projeto Exus

> **Status:** próxima etapa de implementação.
>
> **Depende de:** [SPEC-001](SPEC-001.md) e [SPEC-002](SPEC-002.md), já
> implementadas e validadas por USB Serial. A integração com jogos e a Exus
> Bridge pertencem à [SPEC-004](SPEC-004.md).
>
> **Objetivo:** permitir que um PC controle o ESP32-C3 por Bluetooth Low Energy
> (BLE), sem alterar a lógica háptica nem reduzir as proteções existentes.

---

## 1. Resumo para o grupo

O ESP32-C3 **já tem Bluetooth LE embutido**. Não é preciso soldar um módulo
Bluetooth adicional no protótipo. Será necessário instalar um novo firmware na
placa e usar um pequeno programa no PC para encontrá-la e enviar comandos.

Bluetooth substitui o **cabo de dados/comandos**; ele **não fornece energia**.
Portanto, a sequência correta é:

1. Manter o protótipo ligado ao PC por **cabo USB de dados**.
2. Gravar o firmware com BLE pelo USB e usar a Serial para confirmar que o boot,
   as zonas e a parada de emergência continuam corretos.
3. Com o USB ainda conectado, parear/conectar o PC por BLE e testar comandos
   seguros. Nesse momento o USB fornece energia e permanece como diagnóstico.
4. Só após o teste BLE passar, desconectar o USB e alimentar o protótipo por uma
   **bateria/fonte já validada**. O BLE então continua sendo o único canal de
   comando com o PC.

Não conectar uma bateria e o USB ao mesmo tempo sem um circuito de gerenciamento
de energia que tenha sido projetado para isso: uma fonte pode alimentar a outra.
Nesta SPEC, bateria, carregamento e proteção elétrica continuam fora de escopo;
ela só define quando é seguro trocar para uma alimentação que já funcione.

Se o PC não tiver Bluetooth LE, usar um adaptador USB explicitamente compatível
com **Bluetooth Low Energy**. Em Windows 10/11 normalmente não é necessário
instalar programa do fabricante: o adaptador usa o driver do próprio Windows.

---

## 2. O que fazer na prática — roteiro de bancada

### 2.1 Antes de começar

| No protótipo físico | No PC |
|---|---|
| Desligar a USB antes de mexer em fios. Conferir GND comum, DRV2605, TCA e motores conforme o README. | Ter Windows 10/11, Bluetooth LE ativo e Python 3.10+ instalado. Não é necessário instalar um "programa Bluetooth" separado. |
| Usar inicialmente **apenas o cabo USB de dados** como alimentação. Não usar a bateria nesta fase. | Instalar Arduino IDE, suporte ESP32 e biblioteca do DRV2605 conforme o README. Na Arduino IDE, instalar também **NimBLE-Arduino** pelo Library Manager quando o agente entregar o firmware BLE. Depois da implementação, instalar o cliente de bancada: `python -m pip install -r tools/requirements.txt`. |

O agente deverá criar `tools/requirements.txt` com `bleak` e uma ferramenta
`tools/exus_ble.py` (ou pacote equivalente). Assim, o grupo não precisa escrever
um aplicativo Bluetooth nem usar programas genéricos de celular.

### 2.2 Primeira conexão Bluetooth, passo a passo

1. Conectar o ESP32-C3 ao PC pelo **cabo USB de dados** e abrir o firmware na
   Arduino IDE.
2. Fazer upload do firmware BLE. Se o upload travar, aplicar o procedimento do
   botão **BOOT** descrito no README. Esta primeira gravação **sempre é por USB**.
3. Abrir o Serial Monitor em 115200 baud. Confirmar `zones`, `Q` e `emergency`.
   Nenhum motor deve estar ativo antes de continuar.
4. Habilitar a janela de pareamento na Serial com `ble pair enable`. Ela dura no
   máximo 60 s e só deve ser aberta com o protótipo na mesa, fora do corpo.
5. Em outro terminal do PC, executar `python tools/exus_ble.py scan`. O nome
   esperado é `Exus-<id-curto>`.
6. Executar `python tools/exus_ble.py connect --id <id-curto> info`. O cliente
   inicia o pareamento; aceitar a confirmação do Windows caso ela apareça. Não
   depender do menu **Configurações > Bluetooth** do Windows: dispositivos BLE
   GATT podem não aparecer ali como fone de ouvido, embora estejam funcionando.
7. Com o cabo USB ainda conectado, executar `status`, `Q`, um pulso mínimo e
   `emergency` pelo cliente BLE. Conferir na Serial que o mesmo comando foi
   recebido e que a parada interrompe todas as zonas.
8. Desconectar o cliente ou desligar o Bluetooth do PC. Todos os motores devem
   parar. Reconectar e repetir uma vez antes de considerar o BLE pronto.

Exemplos que o cliente de bancada deve oferecer após a implementação:

```powershell
python tools/exus_ble.py scan
python tools/exus_ble.py connect --id A1B2 info
python tools/exus_ble.py connect --id A1B2 command "pulse 0 15 500 10"
python tools/exus_ble.py connect --id A1B2 command emergency
```

O identificador é apenas um exemplo. O comando `scan` deve mostrar o valor real.

### 2.3 Passar a funcionar sem USB

Após os testes acima, **sim**, o cabo USB pode ser removido e o protótipo pode
ficar alimentado pela bateria, mas somente se todos estes itens forem verdadeiros:

- a bateria/fonte entrega a tensão e a corrente corretas para o ESP32 e para o
  pior caso de motores permitido pela SPEC-002;
- a bateria não é ligada crua ao pino `3V3`: ela usa a entrada/regulador e o
  circuito de proteção definidos para a placa escolhida;
- o GND da fonte, ESP32, TCA e todos os DRV2605 continua comum;
- a placa não reinicia ao vibrar e a tensão foi medida sob carga;
- o grupo já testou a parada por desconexão e a emergência com alimentação USB;
- a primeira utilização sem USB ocorre na mesa, não no corpo.

Para voltar a editar o firmware, apagar pareamentos ou recuperar uma atualização
interrompida, reconectar o USB. Ele continua sendo o caminho de recuperação.

---

## 3. Escopo e decisões

| Item | Decisão |
|---|---|
| Rádio | BLE 5.x/GATT; não usar Bluetooth Classic nem BLE Mesh. O ESP32-C3 não suporta Classic. |
| ESP32-C3 | *Peripheral* BLE; uma conexão de controle por vez. |
| PC | *Central* BLE; cliente de bancada Python com `bleak`. |
| Firmware | Arduino + NimBLE-Arduino, sem migração para ESP-IDF nesta fase. |
| Energia de bancada | USB. BLE não alimenta o protótipo. |
| Energia portátil | bateria/fonte externa já validada; seu projeto elétrico não entra nesta SPEC. |
| Recuperação | USB Serial obrigatório para logs, apagar bond e recuperar falha de OTA. |
| OTA | segundo marco, depois de BLE de controle estável. |

O firmware é a autoridade final: nenhum pacote BLE pode ultrapassar intensidade,
duração, *cooldown*, orçamento global ou bloqueio de emergência da SPEC-002.

---

## 4. Arquitetura e responsabilidades

```text
PC: tools/exus_ble.py (agora) / Exus Bridge (SPEC-004)
                    │ BLE GATT criptografado
ESP32-C3: BleTransport → CommandRouter → Segurança → Scheduler → ZoneDriver
                                                        │
                                               TCA9548A → DRV2605L → motores

USB Serial: SerialTransport ────────────────┘
           (upload, logs, diagnóstico e recuperação)
```

O agente deve extrair o parser e a execução que hoje estão em `Comandos.cpp` para
um `CommandRouter` independente do transporte. `SerialTransport` e
`BleTransport` apenas recebem/enviam bytes e respostas. Não duplicar parser,
limites, scheduler ou lógica de emergência.

O contrato externo BLE usa somente IDs lógicos de zona e máscaras de zona. O
comando físico `mux` pode permanecer como diagnóstico USB, mas não deve ser
oferecido ao futuro Bridge nem usado por jogos.

---

## 5. Serviço BLE de controle

Definir UUIDs de 128 bits constantes e versionados em um único arquivo
`BleProtocol.h`, compartilhado conceitualmente com o cliente PC. O anúncio usa
`Exus-<id-curto>` e inclui a versão do protocolo. Não usar MAC como identidade
confiável: ela pode ser aleatorizada.

| Característica | Direção | Propriedade | Conteúdo |
|---|---|---|---|
| `command` | PC → ESP32 | Write / Write Without Response | Uma linha ASCII UTF-8, terminada por `\n`, até 128 bytes. |
| `response` | ESP32 → PC | Indicate | ACK/NACK e resposta a consulta; confirmação obrigatória. |
| `status` | ESP32 → PC | Notify | conexão, zonas prontas, emergência, contadores e versão. |
| `device-info` | ESP32 → PC | Read | protocolo, firmware, ID, tipos de motor e capacidades. |
| `emergency` | PC → ESP32 | Write | parada imediata, sem depender do parser de comando. |

No MVP, `command` aceita:

```text
Q <seq>
pulse <zona> <intens%> <ms> [Hz]
effect <zona> <1-123>
group <máscara> <pulse|effect> ...
stop <zona|all>
status [zona]
emergency
resume
```

Toda solicitação que contiver `seq` recebe exatamente uma resposta indicada:
`A <seq> <resultado>` ou `N <seq> <motivo>`. O PC limita solicitações pendentes,
aplica *timeout* e **não reenvia automaticamente** `pulse`/`effect`, pois repetir
um deles pode gerar vibração duplicada. Mensagem acima do limite, sem `\n` no
prazo definido ou fora da gramática é descartada e gera NACK quando possível.

### 5.1 Conexão, desconexão e pareamento

- Anunciar somente quando não houver conexão ativa.
- Ao conectar, iniciar com todas as zonas paradas; o PC consulta `Q` antes de
  qualquer comando háptico. Quando o PC já possuir bond persistente, o
  periférico deve iniciar a recriptografia logo após `onConnect`; não usar o
  estado ainda não autenticado desse callback para revogar o bond.
- Ao desconectar, parar o scheduler, descartar fragmentos e comandos BLE já
  enfileirados e voltar a anunciar. Uma emergência também descarta a fila,
  para que um `pulse` recebido antes dela nunca volte a ligar um motor.
- Se o link ficar sem comando/heartbeat válido por 2 s durante saída contínua,
  parar tudo. Implementar esse *watchdog* antes de liberar comando contínuo BLE.
- Aceitar um único central no MVP.
- O primeiro pareamento requer presença local: `ble pair enable` pela Serial
  abre uma janela máxima de 60 s. Fora dela, novas tentativas são recusadas.
- Usar criptografia e *bonding* persistente em NVS; no MVP, manter apenas um PC
  autorizado. A decisão de aceitar o primeiro pareamento é congelada ao abrir
  o link, para a expiração da janela durante a negociação não gerar falha
  espúria. `ble bonds clear` existe exclusivamente na Serial e, ao trocar PC,
  requer também remover o dispositivo salvo no Windows.
- Exigir link criptografado para `command`, `emergency` e OTA. A emergência
  local/Serial continua possível mesmo sem BLE.

---

## 6. Atualização OTA por BLE

OTA não é pré-requisito para usar Bluetooth. Ela só começa quando controle BLE,
parada por desconexão e recuperação USB tiverem passado nos testes.

### 6.1 Preparação obrigatória

Antes de codificar, o agente deve medir o binário real e confirmar que a flash
da placa comporta `otadata` e **duas** partições de aplicação, cada uma maior que
o firmware. Criar e versionar a tabela de partições selecionada na Arduino IDE.
Se não couberem duas imagens, desabilitar OTA e manter atualização por USB;
nunca gravar uma atualização sobre o firmware que está em execução.

### 6.2 Serviço OTA

Criar um serviço GATT separado, acessível apenas em link criptografado:

| Característica | Conteúdo |
|---|---|
| `ota-control` | `BEGIN`, `ABORT`, `COMMIT`, versão, tamanho e SHA-256 esperado. |
| `ota-data` | bloco sequencial com número e carga limitada ao MTU negociado. |
| `ota-status` | indicação de progresso, próximo bloco, erro e resultado. |

Fluxo obrigatório:

1. O PC consulta identidade e versão e confirma que o protótipo tem alimentação
   estável. Bateria fraca não é condição aceitável para OTA.
2. `BEGIN` coloca o ESP32 em modo OTA: `scheduler_stop_all()`, DRVs em standby e
   bloqueio de comandos hápticos.
3. O PC envia blocos sequenciais; o ESP32 valida ordem e tamanho e grava somente
   na partição inativa.
4. Ao terminar, o ESP32 compara o SHA-256 calculado com o informado em `BEGIN`.
5. Só então `COMMIT` seleciona a nova partição e reinicia.
6. O cliente reconecta e confirma a nova versão. Qualquer falha deixa diagnóstico
   explícito e permite recuperação por USB.

O cliente deve oferecer `python tools/exus_ble.py flash firmware.bin`, com barra
de progresso e confirmação do dispositivo alvo. Nunca transportar imagem OTA em
`command`, nem aceitar controle háptico durante a atualização. Neste marco de
bancada, hash e partição inativa são mínimos; antes de distribuição externa,
avaliar assinatura de imagem, Secure Boot e flash encryption para a placa final.

---

## 7. Plano de implementação para o agente

### Fase 0 — preservar o baseline USB

- Compilar e testar o firmware atual por USB.
- Registrar `Q`, `zones`, `status`, `emergency` e pulsos das zonas 0, 1 e 2.
- Não alterar pinos, perfis ERM/LRA, ZoneMap, limites ou comportamento existente.

**Gate:** a regressão USB passa antes de introduzir BLE.

### Fase 1 — refatorar o caminho de comando

- Criar `CommandRouter` e uma interface de saída/resposta; adaptar `Comandos` a
  ela sem mudar a sintaxe Serial existente.
- Garantir que `emergency` pare tudo antes de qualquer resposta e que uma falha
  de transporte nunca deixe saída ativa.
- Adicionar testes unitários ou sketch de teste para parser, limite de 128 bytes,
  linhas inválidas e roteamento para a mesma segurança.

**Gate:** Serial preserva todos os comandos e resultados atuais usando o novo
roteador.

### Fase 2 — BLE de diagnóstico e segurança

- Adicionar NimBLE-Arduino, `BleProtocol`, anúncio e `device-info`/`status`.
- Implementar janela de pareamento Serial, criptografia, bond único e limpeza de
  bond apenas pela Serial.
- Criar `tools/requirements.txt`, `tools/exus_ble.py scan` e `info`.
- Documentar no README os pré-requisitos do PC e o passo a passo da seção 2.

**Gate:** apenas PC pareado lê informações; apagar o bond pelo USB revoga acesso.

### Fase 3 — comandos BLE

- Implementar as características `command`, `response` e `emergency`, framing,
  ACK/NACK e *timeout*.
- Conectar BLE ao mesmo `CommandRouter`; nenhum comando pode chamar scheduler ou
  driver diretamente.
- Implementar parada na desconexão e *watchdog* de 2 s.
- Adicionar ao cliente `connect`, `status`, `command` e testes automatizáveis de
  desconexão/entrada inválida.

**Gate:** `Q`, `pulse`, `stop`, `emergency` e `resume` funcionam por BLE; retirar
o PC, desligar Bluetooth ou enviar pacote inválido nunca deixa motor ativo.

### Fase 4 — OTA BLE

- Versionar a tabela de partições e verificar tamanho em build.
- Implementar serviço OTA, hash, escrita na partição inativa, confirmação após
  boot e a operação `flash` no cliente.
- Testar imagem corrompida, desconexão no meio, reinício no meio e imagem maior
  que a partição. Confirmar recuperação USB em todos os casos.

**Gate:** imagem válida atualiza; falhas não deixam motor ativo nem inutilizam o
caminho USB.

### Fase 5 — estabilidade e entrega documental

- Executar *soak test* de 30 min com comandos, reconexões e logs.
- Medir latência comando→vibração, perdas, RSSI e jitter com 1, 2 e 3 zonas.
- Repetir na distância e no ambiente da demonstração; registrar resultado.
- **Obrigatório:** atualizar o `README.md` principal com uma seção “Bluetooth:
  primeira conexão”, baseada na seção 2 desta SPEC, incluindo pré-requisitos do
  PC, comandos do cliente, ordem USB→BLE→bateria, cuidados de alimentação e
  recuperação por USB. Atualizar também o índice do README.

### Oportunidades após a validação da reconexão

- **Botão físico de pareamento:** elimina a dependência da Serial para
  autorizar uma troca controlada de PC, sem deixar o dispositivo aberto.
- **Telemetria de energia:** registrar motivo de reset/brownout e causa de
  desconexão BLE para distinguir falha de bond de queda da bateria durante a
  negociação do link.
- **Teste de regressão em hardware:** automatizar o ciclo parear → desligar
  USB → iniciar pela bateria → reconectar → desconectar, incluindo a garantia
  de que não há comando pendente após emergência.
- **OTA seguro:** iniciar esta fase somente depois desse ciclo estar estável;
  recuperação USB e validação de imagem continuam obrigatórias.

---

## 8. Critérios de aceite

- [ ] ESP32-C3 anuncia BLE e aceita um único PC pareado e criptografado.
- [ ] `Q` e `status` reportam zonas ERM 0/1 e LRA 2 por BLE.
- [ ] BLE e Serial usam o mesmo roteador e obedecem os mesmos limites locais.
- [ ] Desconexão, *timeout* ou pacote inválido param todos os atuadores.
- [ ] Emergência funciona por BLE e por USB; USB permanece independente do BLE.
- [ ] Após um primeiro pareamento, reiniciar pela bateria permite reconectar o
  mesmo PC sem reabrir a janela de pareamento.
- [ ] Desconexão ou emergência não executa comando BLE que já estava na fila.
- [ ] O roteiro de primeira conexão funciona com USB conectado, sem depender do
  painel Bluetooth do Windows.
- [ ] O README principal contém o passo a passo leigo da primeira conexão e da
  troca segura para bateria.
- [ ] OTA só escreve a partição inativa, valida SHA-256 e mantém recuperação USB.
- [ ] Nenhum teste corporal ocorre até os gates elétricos, mecânicos e de
  segurança das SPEC-001/002 passarem.

---

## 9. Relação com a SPEC-004

Esta SPEC entrega o transporte de dispositivo e o cliente de bancada. A
SPEC-004 consome o mesmo contrato lógico por `BleTransport` ou
`SerialTransport`, sem conhecer TCA, canal físico, ERM/LRA, bateria ou OTA.

O aplicativo visual para uso do grupo, que reutiliza o cliente/protocolo BLE sem
substituir o firmware, é detalhado na [SPEC-003.5](SPEC-003.5.md).

## Referências

- [ESP32-C3 Bluetooth LE — Espressif](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-guides/ble/overview.html)
- [Bluetooth API — Espressif](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/api-reference/bluetooth/index.html)
- [OTA e partições — Espressif](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/system/ota.html)
- [SPEC-002 — firmware multi-zona](SPEC-002.md)
- [SPEC-004 — Exus Bridge e demo jogável](SPEC-004.md)
