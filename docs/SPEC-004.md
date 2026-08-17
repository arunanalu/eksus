# SPEC-004 — Exus Control como ponte entre jogos e o protótipo

> **Status:** plano técnico pronto para implementação incremental.
>
> **Atualizada em:** 16 de agosto de 2026.
>
> **Depende de:** [SPEC-003](SPEC-003.md) para estabilização do BLE e
> [SPEC-003.5](SPEC-003.5.md) para a base visual já implementada do Exus Control.
>
> **Relacionada:** [SPEC-005](SPEC-005.md) define a demo jogável em Godot.
>
> **Objetivo:** evoluir o aplicativo **Exus Control** já existente para também
> receber eventos de jogos, transformá-los em comandos hápticos e enviá-los ao
> protótipo pela conexão BLE que ele já administra.

---

## 1. Decisão executiva

O produto no PC continuará se chamando **Exus Control**. “Bridge” será o nome de
uma função interna, não de um segundo aplicativo. Assim, o usuário abre apenas:

1. o jogo compatível;
2. o `Exus-Control.exe`.

O Exus Control terá duas áreas no mesmo processo:

- **Protótipo:** procurar, parear, conectar, testar zonas, parar e diagnosticar;
- **Ponte de jogo:** escutar eventos locais do jogo, validar, mapear, registrar e,
  quando autorizado, encaminhar comandos ao protótipo conectado.

```text
Demo Godot
   │ UDP local: evento de jogo em JSON
   ▼
Exus Control
   ├── valida, mapeia, arbitra e registra
   ├── modo simulado: gera WOULD_SEND, sem hardware
   └── modo real: envia pelo cliente BLE já existente
                         │
                         ▼
ESP32-C3 → segurança → zonas → motores
```

Não será criado um `Exus Bridge.exe` separado. O jogo também não acessará BLE,
porta COM, TCA9548A ou DRV2605L diretamente.

---

## 2. Resultado da exploração do repositório

### 2.1. O que já existe

| Camada | Implementação atual | Situação |
|---|---|---|
| Firmware multi-zona | `firmware/ZoneMap.*`, `ZoneDriver.*`, `MultiZoneScheduler.*` e segurança | implementado; validação elétrica/mecânica continua pendente |
| BLE no ESP32-C3 | `firmware/BleProtocol.h` e `BleTransport.*` | implementado; equipe está ajustando estabilidade e reconexão |
| Roteador comum | `comandos_executar()` em `firmware/Comandos.*` | Serial e BLE chegam à mesma lógica e aos mesmos limites |
| Cliente BLE reutilizável | `tools/exus_ble_client.py` | implementado com scan, conexão, ACK/NACK, status e emergência |
| Cliente de bancada | `tools/exus_ble.py` | implementado |
| Aplicativo visual | `tools/exus_control.py` | MVP implementado; pendente de validação física |
| Executável Windows | `tools/build_windows.ps1` | empacota `Exus-Control.exe` com PyInstaller |
| Integração com jogo | inexistente | objeto desta SPEC e da SPEC-005 |
| Testes automatizados no PC | inexistentes no repositório | primeira lacuna a corrigir |

O protocolo BLE atual já aceita comandos como `Q`, `pulse`, `group`, `stop`,
`emergency` e `resume` e devolve respostas correlacionadas por sequência. Isso é
suficiente para o primeiro ponta a ponta. O protocolo compacto `H/A/N/X/K/Q`
antes proposto não é pré-requisito do MVP e só deverá ser criado se medições
mostrarem uma limitação real do enquadramento atual.

### 2.2. Conclusão prática

É correto desenvolver jogo e ponte **sem o protótipo**. A separação será obtida
por dois limites estáveis:

- jogo → Exus Control: evento canônico por UDP local;
- Exus Control → dispositivo: interface `HapticTransport`, inicialmente com
  `MockTransport` e `BleTransport`.

O colega pode estabilizar BLE/firmware enquanto a outra trilha implementa evento,
mapeamento, logs e jogo. O encontro posterior não deve exigir alteração nas
mecânicas do jogo.

---

## 3. Regras de segurança e modo simulado

### 3.1. Saída real começa sempre desabilitada

O Exus Control inicia cada execução com `hardware_output_enabled = false`. Esse
estado não é restaurado automaticamente de uma sessão anterior.

Enquanto a saída real estiver desabilitada, o pipeline permanece completo:

1. recebe o datagrama;
2. valida o schema;
3. calcula zonas, padrão, intensidade e duração;
4. aplica prioridade, cooldown e TTL;
5. gera no log o comando final como `WOULD_SEND`;
6. responde ao jogo com estado `simulated`.

Somente a última chamada ao transporte físico é omitida. Assim, os testes provam
o funcionamento da integração sem precisar do ESP32.

### 3.2. Dupla autorização

Uma vibração real exige simultaneamente:

- o jogo enviar `output_requested: true` — opção **Vibrações reais**, desligada
  por padrão no jogo;
- o operador habilitar **Permitir saída para o protótipo** no Exus Control,
  também desligado por padrão;
- BLE conectado, link autorizado e capabilities carregadas;
- firmware fora de emergência e com ao menos uma zona pronta.

Se qualquer condição falhar, o evento vira `WOULD_SEND`, nunca fica em fila para
execução futura.

### 3.3. Transições obrigatórias

- Desabilitar a saída real envia `stop all` antes de mudar a interface.
- Perder BLE desabilita a saída real e descarta comandos pendentes.
- Reconectar não reabilita a saída real.
- Fechar o Exus Control tenta enviar `stop all` e desconecta.
- Emergência ignora rate limit, interrompe a fila e permanece disponível sempre
  que o link BLE existir.
- O firmware continua sendo a autoridade final para intensidade, duração,
  simultaneidade, cooldown e emergência.

---

## 4. Arquitetura alvo do Exus Control

O código atual em `tools/` deverá migrar gradualmente para um pacote, sem quebrar
a CLI nem o executável durante a transição:

```text
tools/
├── exus_ble.py                   # CLI de bancada preservada
├── exus_control.py               # entrada do aplicativo preservada
├── exus_ble_client.py            # compatibilidade durante a migração
├── exus_control_core/
│   ├── events.py                 # schema e validação
│   ├── bridge_server.py          # UDP localhost e respostas ao jogo
│   ├── haptic_mapper.py          # evento → intenção háptica
│   ├── arbitration.py            # prioridade, TTL, cooldown e coalescência
│   ├── session.py                # dupla autorização e estado da sessão
│   ├── logging.py                # JSON Lines e contadores
│   └── transports/
│       ├── base.py               # contrato HapticTransport
│       ├── mock.py               # captura comandos sem hardware
│       └── ble.py                # adapta ExusBleClient
├── tests/
│   ├── unit/
│   ├── integration/
│   └── fixtures/
└── build_windows.ps1
```

Interface mínima do transporte:

```python
class HapticTransport(Protocol):
    async def connect(self) -> Capabilities: ...
    async def send(self, command: str) -> CommandResult: ...
    async def stop_all(self) -> None: ...
    async def emergency(self) -> None: ...
    async def disconnect(self) -> None: ...
    @property
    def state(self) -> TransportState: ...
```

O mapper e o servidor UDP não podem importar `bleak`. Somente `transports/ble.py`
conhece BLE. Isso permite executar todos os testes de jogo em qualquer PC.

---

## 5. Contrato jogo → Exus Control

### 5.1. Transporte local

- UDP IPv4 em `127.0.0.1:4242` por padrão;
- nunca escutar em `0.0.0.0` no MVP;
- um datagrama contém um documento JSON UTF-8;
- tamanho máximo inicial: 2 KiB;
- schema desconhecido, JSON inválido, números não finitos e pacote acima do
  limite são rejeitados e contabilizados;
- a porta é configurável no Exus Control, com `4242` como padrão.

O UDP local evita dependência da internet e permite ao Godot usar
`PacketPeerUDP`, sem plugin nativo.

### 5.2. Evento canônico v1

```json
{
  "schema": "exus.game-event/1",
  "session_id": "6d1d0fd2",
  "seq": 1042,
  "sent_at_ms": 82133,
  "event": "damage",
  "state": "oneshot",
  "stream_id": null,
  "azimuth_deg": -65.0,
  "magnitude": 0.42,
  "duration_ms": 80,
  "source": "projectile",
  "output_requested": false
}
```

Regras:

- `session_id` muda a cada execução do jogo;
- `seq` é crescente dentro da sessão;
- `magnitude` fica entre `0.0` e `1.0` e nunca representa corrente elétrica;
- azimute: `0` frente, `-90` esquerda, `+90` direita e `±180` atrás;
- `oneshot` representa impacto pontual;
- `start`, `update` e `stop` exigem `stream_id` para vento ou ameaça contínua;
- `duration_ms` é obrigatório para `oneshot` e limitado pelo Control antes do
  firmware aplicar seus próprios limites;
- `output_requested` é `false` por padrão.

### 5.3. Resposta do Control ao jogo

O Control responde à porta de origem para permitir um indicador no jogo:

```json
{
  "schema": "exus.bridge-result/1",
  "session_id": "6d1d0fd2",
  "seq": 1042,
  "result": "simulated",
  "device": "disconnected",
  "command": "group 0x02 pulse 21 80 30",
  "reason": "hardware_output_disabled"
}
```

Resultados possíveis: `simulated`, `sent`, `rejected`, `expired` e `dropped`.
O jogo não deve pausar nem falhar se a resposta não chegar.

---

## 6. Tradução de evento para comando

Tabela inicial, mantida em configuração validada e versionada:

| Evento | Intenção | Regra multi-zona | Degradação de uma zona | Prioridade |
|---|---|---|---|---:|
| `damage` | impacto localizado | setor do azimute | pulso duplo/assimétrico conforme lado | 90 |
| `explosion` | impacto amplo curto | até 3 zonas voltadas à origem | pulso forte com queda curta | 100 |
| `wind` | estado leve contínuo | frente e bochechas | envelope leve renovado por TTL | 20 |
| `threat` | presença direcional | zona mais próxima do azimute | padrão temporal por direção | 50 |
| `weapon_fire` | retorno curto | mandíbula/lado configurado | click curto | 30 |

O Control consulta `zones_ready` antes de mapear. Não contém endereço de TCA,
canal I²C ou tipo elétrico de motor. Se uma direção não tiver zona pronta, a
política configurada deve ser explícita: degradar para zona única ou registrar
`rejected`; nunca redirecionar silenciosamente para outra posição.

Arbitragem mínima:

- emergência e `stop` preemptam tudo;
- eventos expirados não são enviados;
- explosão/dano podem interromper vento na mesma zona;
- eventos idênticos próximos são coalescidos sem somar amplitude indefinidamente;
- fila tem tamanho máximo e descarte observável;
- um efeito contínuo expira sem `update` dentro do TTL;
- não há repetição automática de `pulse` após timeout/NACK.

---

## 7. Interface do Exus Control

Preservar a tela atual e adicionar uma área **Ponte de jogo** com:

- estado: `PARADA`, `ESCUTANDO`, `JOGO ATIVO` ou `ERRO`;
- endereço fixo visível: `127.0.0.1:4242`;
- último evento e último comando calculado;
- contadores recebidos, simulados, enviados, rejeitados e expirados;
- chave **Permitir saída para o protótipo**, desligada no início;
- indicação inequívoca `MODO SIMULADO` ou `SAÍDA REAL HABILITADA`;
- botão para gerar eventos manuais sem abrir o jogo;
- log somente leitura, com exportação da sessão.

O modo simulado funciona sem procurar dispositivo. Conectar BLE e testar zonas
continua funcionando sem abrir a ponte de jogo.

---

## 8. Logs e observabilidade

Cada evento produz uma linha JSON em arquivo de sessão com:

- timestamp monotônico e de parede;
- `session_id`, `seq`, evento e origem;
- validação e motivo de rejeição;
- comando calculado e zonas lógicas;
- modo `simulated` ou `hardware`;
- ACK/NACK/timeout do firmware, quando aplicável;
- latência jogo → Control e Control → ACK;
- estado BLE e versão das capabilities.

Não registrar nome de usuário, conta do jogo, endereço BLE completo ou chaves de
pareamento. O log deve permitir replay em `MockTransport`, mas replay nunca
habilita saída real automaticamente.

---

## 9. Plano em pequenos entregáveis

Cada item deve ser implementado e aceito separadamente. **Nenhum entregável de
E04-01 a E04-08 exige o protótipo físico.** A implementação termina contra um
dispositivo virtual e contratos congelados; o protótipo entra somente na fase
posterior de validação física da Seção 10.

### E04-01 — Baseline e testes do código atual

- adicionar `pytest` e uma primeira suíte para `parse_capabilities` e framing;
- preservar CLI e interface visual existentes;
- criar fixtures de resposta `Q`, ACK, NACK, timeout e desconexão;
- documentar como executar testes com um único comando.

**Aceite:** testes executam sem BLE e sem Godot; build do executável continua
encontrando os módulos atuais.

### E04-02 — Interface de transporte e modo simulado

- criar `HapticTransport`, `BleTransportAdapter` e `MockTransport`;
- mover a sessão para fora da classe Tkinter;
- garantir que `MockTransport` capture exatamente o comando que seria enviado;
- manter `ExusBleClient` como implementação BLE, sem duplicá-lo.

**Aceite:** o mesmo caso de teste produz o mesmo comando no mock e no adaptador
BLE simulado; nenhum import de `bleak` é necessário para testar mapper/sessão.

### E04-03 — Evento canônico, mapper e arbitragem

- implementar validação estrita de `exus.game-event/1`;
- criar tabela inicial dos cinco eventos;
- implementar sequência, TTL, stream contínuo, prioridade e fila limitada;
- cobrir zona única e capabilities multi-zona por fixtures.

**Aceite:** 10.000 eventos sintéticos não travam nem fazem a fila crescer sem
limite; duplicados, atrasados e inválidos têm resultado determinístico.

### E04-04 — Servidor local da ponte

- escutar exclusivamente `127.0.0.1:4242`;
- receber eventos e devolver `exus.bridge-result/1`;
- detectar sessão ativa do jogo e expiração de heartbeat/atividade;
- incluir gerador/replay de eventos para teste sem Godot.

**Aceite:** um script local envia quatro eventos, recebe quatro resultados e o
log mostra quatro `WOULD_SEND`, com saída real impossível.

### E04-05 — Integração da ponte na interface

- adicionar painel, estados, contadores e eventos manuais;
- implementar a dupla autorização;
- sempre iniciar em modo simulado;
- ao desabilitar, desconectar ou fechar, parar e descartar pendências.

**Aceite:** uma pessoa leiga abre o app, inicia a ponte e visualiza eventos
simulados sem instalar Godot nem conectar hardware.

### E04-06 — Empacotamento e operação sem Python

- atualizar `build_windows.ps1` para incluir o novo pacote e configurações;
- adicionar teste de inicialização do executável;
- documentar firewall: por usar somente loopback, não deve ser necessário abrir
  acesso de rede pública;
- validar em segundo Windows sem Python.

**Aceite:** um único `Exus-Control.exe` oferece bancada BLE e ponte de jogo.

### E04-07 — Dispositivo virtual compatível com o firmware

- criar `VirtualExusTransport` com capabilities de uma e várias zonas;
- simular ACK, NACK, timeout, desconexão, emergência e watchdog;
- tornar a fábrica do cliente BLE injetável para testar o adapter sem rádio;
- versionar fixtures do protocolo atual: `Q`, `pulse`, `group`, `stop`,
  `emergency` e limites de 128 bytes;
- executar a mesma suíte contra `MockTransport`, adapter BLE virtual e replay.

**Aceite:** todos os caminhos do Exus Control, inclusive estados de erro e
reconexão, são exercitados sem `bleak` acessar um adaptador Bluetooth real.

### E04-08 — Ponta a ponta virtual com a demo Godot

- usar a entrega correspondente da SPEC-005;
- executar o jogo e o Exus Control empacotados no mesmo Windows;
- usar `VirtualExusTransport` no lugar do protótipo;
- testar quatro eventos, zona única, múltiplas zonas, NACK, timeout, desconexão,
  dupla autorização, `stop` e emergência;
- gerar relatório final com logs correlacionados por `session_id/seq`;
- congelar schema, tabela háptica e contrato de transporte como candidato ao
  primeiro teste físico.

**Aceite:** jogo → UDP → Exus Control → dispositivo virtual conclui toda a
sessão, incluindo falhas, sem hardware e sem deixar comandos pendentes.

---

## 10. Validação física posterior — fora dos entregáveis

Somente depois de E04-01 a E04-08 e E05-01 a E05-08 aceitos será feito o teste
com o protótipo. Essa fase valida pressupostos externos que mocks não conseguem
provar: pareamento do Windows, rádio, MTU, latência, energia e vibração mecânica.

Procedimento:

1. criar uma tag do candidato testado integralmente em modo virtual;
2. conectar o protótipo em bancada e executar `Q`, stop e emergência antes de
   qualquer pulso;
3. trocar somente `VirtualExusTransport` pelo adapter BLE real;
4. testar um evento mínimo, depois os quatro eventos do roteiro;
5. comparar respostas reais com as fixtures congeladas;
6. se funcionar, registrar o aceite físico sem alterar jogo ou mapper;
7. se falhar, registrar log e caso reproduzível e abrir imediatamente uma branch
   `fix/hardware-integration-<sintoma>` a partir do candidato;
8. corrigir preferencialmente adapter BLE/firmware, adicionar uma fixture de
   regressão e repetir primeiro a suíte virtual, depois o teste físico.

Uma incompatibilidade física não autoriza remendar a mecânica do jogo. O schema
do jogo só muda se ficar demonstrado que o contrato, e não o transporte, estava
incorreto.

## 11. Divisão de trabalho paralelo

| Trilha A — BLE/protótipo | Trilha B — Control/jogo | Contrato de encontro |
|---|---|---|
| estabilizar anúncio, bond, reconexão e watchdog | implementar E04-01 a E04-08 e SPEC-005 | `Q`, `pulse/group/stop/emergency`, ACK/NACK |
| validar zonas e limites no firmware | usar capabilities em fixtures | `zones_ready` e `max_group_size` |
| testar desconexão física | testar saída desabilitada e mock | nenhum comando fica pendente |
| fornecer logs/status reais | registrar `WOULD_SEND` e replay | mesmo comando lógico nos dois modos |

Mudanças futuras no framing BLE devem ficar dentro de `ExusBleClient` ou do
adapter. Elas não podem exigir mudanças no Godot nem no schema de evento.

---

## 12. Critérios de aceite da SPEC-004

### Independência

- [ ] Control, mapper, arbitragem e servidor são testáveis sem ESP32 e sem Godot.
- [ ] O jogo não importa biblioteca BLE nem conhece detalhes do hardware.
- [ ] A estabilização BLE pode ocorrer em paralelo sem bloquear E04-01 a E04-08.
- [ ] Todos os entregáveis são aceitos sem adaptador BLE ou protótipo físico.

### Segurança

- [ ] Saída real começa desabilitada em toda execução.
- [ ] Jogo e Control precisam autorizar saída real simultaneamente.
- [ ] Reconexão nunca rearma saída.
- [ ] Evento simulado nunca é enviado depois ao conectar.
- [ ] Stop, emergência, fechamento e desconexão descartam pendências.
- [ ] Limites finais continuam no firmware.

### Funcionalidade

- [ ] Um único Exus Control contém o adapter BLE e atende o jogo; o fluxo é
  aceito integralmente com `VirtualExusTransport`.
- [ ] Eventos canônicos válidos geram comandos determinísticos e logs.
- [ ] Godot recebe resposta de estado, mas continua jogável sem ela.
- [ ] Zona ausente não provoca redirecionamento silencioso.
- [ ] O executável funciona em Windows sem Python instalado.

### Robustez

- [ ] Pacote inválido, grande, duplicado, atrasado ou não finito é rejeitado.
- [ ] Efeito contínuo expira por TTL.
- [ ] 10.000 eventos sintéticos não causam deadlock nem fila crescente.
- [ ] Logs distinguem claramente `WOULD_SEND`, `sent`, `NACK` e `timeout`.

---

## 13. Fora de escopo

- implementar OTA dentro do Exus Control nesta fase;
- expor a ponte na rede local ou internet;
- permitir que o jogo altere limites elétricos do firmware;
- criar outro executável chamado Exus Bridge;
- integrar jogos comerciais antes de a demo Godot estar aceita;
- capturar tela, áudio ou memória de processo como fonte principal;
- declarar segurança médica ou iniciar teste corporal sem os gates anteriores.

---

## 14. Referências

- [SPEC-002 — firmware multi-zona](SPEC-002.md)
- [SPEC-003 — Bluetooth LE e OTA](SPEC-003.md)
- [SPEC-003.5 — aplicativo visual Windows](SPEC-003.5.md)
- [SPEC-005 — demo Godot](SPEC-005.md)
- [Projeto Exus — A Frequência da Imersão](projeto_eksus_documentacao.pdf)
- [Godot `PacketPeerUDP`](https://docs.godotengine.org/en/stable/classes/class_packetpeerudp.html)
- [Godot TPS Demo](https://github.com/godotengine/tps-demo)

---

**Conclusão:** o Exus Control será o único software de integração no PC. A
implementação inteira é concluída em simulação enquanto BLE é estabilizado. O
teste posterior troca apenas o transporte virtual pelo adapter BLE. Se essa
hipótese de integração falhar no hardware, a correção nasce em uma branch
`fix/hardware-integration-*`, acompanhada por fixture de regressão.
