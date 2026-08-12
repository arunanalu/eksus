# SPEC-002 — Evolução do firmware para múltiplas zonas hápticas

> **Status:** implementação de software adicionada em 5 de agosto de 2026;
> compilação no toolchain Arduino e validação elétrica/mecânica com hardware real
> permanecem obrigatórias
>
> **Base mínima:** [SPEC-001 — firmware háptico de uma zona](SPEC-001.md) e [documentação técnica do Projeto Exus](projeto_eksus_documentacao.pdf)
>
> **Relacionada, mas independente:** [SPEC-003 — Exus Bridge e demo jogável](SPEC-003.md)
>
> **Escopo:** firmware, topologia I²C, multiplexadores, drivers, motores, segurança e testes de múltiplas zonas. Não inclui construir a Bridge nem integrar um jogo.

---

## 1. Decisão executiva

O controle de múltiplas zonas é uma evolução própria do sistema embarcado e deve ser desenvolvido separadamente da integração com jogos.

- A **SPEC-002** transforma o firmware atual, que controla uma zona, em um controlador de várias zonas endereçáveis.
- A **SPEC-003** transforma eventos de jogos em comandos para o firmware.
- Nenhuma precisa esperar a outra: esta SPEC pode ser exercitada integralmente por comandos USB Serial e um simulador simples de bancada; a SPEC-003 pode operar inicialmente com a única zona da SPEC-001.
- Quando ambas estiverem prontas, a integração acontece por um contrato pequeno: descoberta de capacidades, identificadores/máscaras de zonas, comandos hápticos, ACK/NACK, status e parada.

Para as oito zonas previstas no PDF, a topologia recomendada continua sendo **um TCA9548A + oito DRV2605L + oito motores LRA**, um driver por zona. O firmware também deve modelar mais de um TCA9548A, para não ficar limitado a oito ramificações, mas isso é uma capacidade de arquitetura — não uma autorização para ampliar o protótipo antes de validar energia, barramento e segurança.

---

## 2. Escopo e fora de escopo

### 2.1. Entra nesta SPEC

- Um ou mais multiplexadores TCA9548A no barramento I²C principal.
- Um DRV2605L e um LRA por zona independente.
- Descoberta, inicialização, calibração e diagnóstico por zona.
- `ZoneMap`: zona física → multiplexador → canal → limites → configuração do motor.
- Scheduler não bloqueante com estado independente por zona.
- Efeitos ROM e RTP em uma ou várias zonas.
- Prioridade, cooldown e orçamento de energia por zona e globais.
- Comandos Serial para testar tudo sem jogo e sem Exus Bridge.
- Contrato de capacidades que a SPEC-003 poderá consumir depois.
- Falha parcial: uma zona ausente não pode derrubar nem acionar outra.

### 2.2. Não entra nesta SPEC

- Código da Exus Bridge.
- UDP, adaptadores de jogos, Godot, Unity, Unreal ou telemetria comercial.
- Interface gráfica no PC.
- Bluetooth como transporte externo; isso ficará na SPEC-004 futura.
- Projeto industrial da PCB, bateria ou certificação do produto.
- Validação clínica ou declaração de segurança médica.

---

## 3. Estado atual do repositório

### 3.1. Implementação entregue

O firmware agora preserva `direct_single_zone` e oferece uma topologia dinâmica
habilitada por `EXUS_USE_TCA9548A`. Nesse modo ele descobre TCAs nos endereços
`0x70..0x77`, verifica os oito canais de cada um e atribui o ID lógico
`(número do mux - 1) * 8 + canal`. Portanto, a quantidade física não precisa ser
conhecida na compilação: existem até 64 slots, mas apenas zonas `READY` aceitam
acionamento.

O comando `mux <1-8> pulse ...` ou `mux <1-8> effect ...` tenta acionar todas as
zonas prontas daquele multiplexador, sujeito aos limites globais de simultaneidade
e amplitude. Um mux inexistente produz um no-op diagnosticado; nunca redireciona
o pedido. Esta generalização mantém a premissa elétrica de um DRV2605L/LRA por
canal — um TCA não controla motores diretamente.

Arquivos implementados: `MuxManager`, `ZoneMap`, `ZoneDriver`,
`MultiZoneScheduler`, segurança zonal/global e o roteador de comandos em
`Comandos`. A descoberta, calibração e falha parcial estão implementadas, mas os
gates de corrente, jitter, crosstalk, soak test e identificação física exigem a
montagem real e não são considerados aprovados por revisão de código.

| Capacidade | Situação atual | Evolução necessária |
|---|---|---|
| DRV2605L | uma instância global `drv` | acesso roteado para a zona selecionada |
| LRA | um motor | um motor/configuração por zona |
| TCA9548A | ausente | manager para um ou mais multiplexadores |
| Envelope RTP | um estado global | estado e temporização por zona |
| Segurança | intensidade, duração, cooldown e rate limit globais | limites por zona + limites globais de energia |
| Comandos | `v`, `ef`, `s`, `e`, `status`, `scan` | seleção de zona/grupo, diagnóstico e parada total |
| Inicialização | procura um `0x5A` diretamente | varredura `mux → canal → 0x5A` |
| Falha | erro bloqueia o sistema | degradar apenas a zona defeituosa quando seguro |

A regressão de uma zona deve continuar passando durante toda a implementação. O firmware multi-zona não pode tornar o hardware atual inutilizável: uma configuração sem TCA deve permanecer suportada como `direct_single_zone`.

---

## 4. Topologia elétrica e endereçamento

### 4.1. Por que o multiplexador é necessário

Os DRV2605L usam o mesmo endereço I²C (`0x5A`). Dois drivers ligados diretamente ao mesmo barramento responderiam ao mesmo comando. O TCA9548A separa o barramento principal em oito pares SDA/SCL selecionáveis.

O [datasheet do TCA9548A](https://www.ti.com/lit/ds/symlink/tca9548a.pdf) especifica um registrador de controle de oito bits: cada bit habilita um canal, e o componente opera em I²C Standard-Mode ou Fast-Mode, até 400 kHz.

```text
ESP32-C3
   │ SDA/SCL principal
   ▼
TCA9548A @ 0x70
   ├── canal 0 ── DRV2605L @ 0x5A ── LRA zona 0
   ├── canal 1 ── DRV2605L @ 0x5A ── LRA zona 1
   ├── ...
   └── canal 7 ── DRV2605L @ 0x5A ── LRA zona 7
```

### 4.2. Mais de um multiplexador

Os pinos A0, A1 e A2 permitem endereços de `0x70` a `0x77`, isto é, até oito TCA9548A distintos no barramento principal. Em tese isso cria até 64 ramificações; na prática, capacitância, corrente, memória, tempo de atualização, conectores e segurança limitam o sistema muito antes desse número.

Regra essencial para múltiplos multiplexadores: como cada ramificação contém um DRV no mesmo `0x5A`, o firmware deve manter **no máximo um caminho independente conectado durante uma transação**. Ao trocar de TCA, desabilitar o canal do TCA anterior antes de habilitar o próximo.

```text
selectExclusive(mux, channel):
  se outro mux/canal estiver ativo: escrever 0x00 nele
  escrever (1 << channel) no mux desejado
  confirmar ACK e atualizar o cache somente em sucesso
```

O TCA permite habilitar vários canais ao mesmo tempo, mas uma escrita em `0x5A` seria recebida por todos os DRVs conectados. Esse broadcast pode sincronizar um efeito idêntico, porém reduz diagnóstico e isolamento de falhas. Ele fica **desabilitado no MVP**; efeitos simultâneos serão disparados sequencialmente, com seleção exclusiva.

### 4.3. Unidade real de independência

- Um motor conectado a um DRV = uma zona controlável.
- Dois motores no mesmo DRV não são duas zonas independentes.
- Oito motores com três DRVs oferecem, no máximo, três comandos independentes.
- Oito zonas independentes exigem oito drivers ou outra arquitetura explicitamente validada.

---

## 5. Mapa inicial de zonas

Mapa derivado do PDF interno; números são identificadores lógicos, não posições rígidas na PCB:

| ID | Zona | TCA | Canal | Limite inicial do PDF | Observação |
|---:|---|---:|---:|---:|---|
| 0 | testa | `0x70` | 0 | 20–45% | região ampla; começar abaixo do teto |
| 1 | têmpora esquerda | `0x70` | 1 | 15–35% | teto menor |
| 2 | têmpora direita | `0x70` | 2 | 15–35% | teto menor |
| 3 | bochecha esquerda | `0x70` | 3 | 20–45% | validar acoplamento |
| 4 | bochecha direita | `0x70` | 4 | 20–45% | validar simetria |
| 5 | mandíbula esquerda | `0x70` | 5 | 25–60% | não adotar teto alto sem ensaio |
| 6 | mandíbula direita | `0x70` | 6 | 25–60% | validar simetria |
| 7 | posterior/nuca | `0x70` | 7 | 25–65% | fixação e aquecimento separados |

Esses intervalos são pontos de partida do documento do projeto, **não resultados de certificação**. O valor efetivo deve ser reduzido por configuração de motor, montagem, calibração e perfil de teste. Olhos, pálpebras, lábios e mucosas continuam fora do MVP.

Estrutura conceitual:

```cpp
struct ZoneConfig {
  uint8_t id;
  uint8_t muxAddress;  // 0x70..0x77; valor especial para ligação direta
  uint8_t muxChannel;  // 0..7
  ZonePosition position;
  MotorConfig motor;
  SafetyLimits limits;
  bool enabled;
};
```

O `id` é estável para software externo. Endereço e canal são detalhes substituíveis de montagem. Trocar um chicote não deve exigir alterar o jogo ou a Bridge.

---

## 6. Arquitetura do firmware

```text
Comandos Serial / futuro protocolo externo
                  │
                  ▼
           HapticCommandRouter
                  │
          valida zonas e grupos
                  ▼
          MultiZoneScheduler
        estado/prioridade por zona
                  │
          SafetyManager zonal
                  │
                  ▼
              ZoneDriver
                  │
         MuxManager → DRV2605L
                  │
                  ▼
               motores LRA
```

### 6.1. Módulos propostos

| Módulo | Responsabilidade |
|---|---|
| `MuxManager` | detectar TCAs, desabilitar canais, selecionar caminho exclusivo e tratar erro de seleção |
| `ZoneMap` | manter configuração física e limites de cada zona |
| `ZoneDriver` | selecionar a zona antes de toda operação DRV; configurar, calibrar, tocar e parar |
| `MultiZoneScheduler` | estados, envelopes, deadlines, prioridades e término por zona sem `delay()` |
| `SafetyManager` | limites zonais, emergência global, cooldown, timeout e orçamento agregado |
| `CommandRouter` | comandos de bancada por zona/grupo e contrato versionado futuro |
| `Diagnostics` | presença, calibração, falhas I²C, contadores, tempo de atualização e resets |

### 6.2. Regra de encapsulamento

Nenhum módulo, exceto `ZoneDriver`, deve falar diretamente com o DRV2605L. Nenhum acesso ao DRV pode ocorrer sem selecionar explicitamente a zona. Isso evita o defeito mais perigoso dessa arquitetura: enviar um padrão para o canal que ficou selecionado pela operação anterior.

---

## 7. Inicialização e descoberta

Fluxo de boot proposto:

1. Manter todos os motores parados e iniciar Serial/I²C.
2. Detectar os endereços TCA configurados.
3. Escrever `0x00` em cada TCA detectado para desconectar todas as ramificações.
4. Para cada `ZoneConfig` habilitada:
   1. selecionar exclusivamente TCA/canal;
   2. procurar o DRV em `0x5A`;
   3. configurar LRA, tensão nominal, clamp e biblioteca;
   4. executar diagnóstico/calibração somente no modo previsto;
   5. parar e colocar a zona em standby;
   6. registrar `READY`, `MISSING`, `CALIBRATION_FAILED` ou `DISABLED`.
5. Desabilitar novamente todos os canais.
6. Publicar uma linha de capabilities e aceitar comandos somente nas zonas `READY`.

Calibração pode produzir movimento e deve ser feita sequencialmente, fora do corpo. Avaliar uma fase de comissionamento que capture parâmetros por motor e um boot normal que não recalibre toda a máscara a cada ligação.

Se uma zona falhar, ela fica isolada. O sistema pode continuar nas demais quando a falha não indicar problema comum de alimentação/barramento. Falha do TCA principal, emergência ou queda global de energia bloqueiam tudo.

---

## 8. Execução simultânea

“Simultâneo” possui duas implementações diferentes.

### 8.1. Efeitos internos do DRV2605L

O firmware seleciona uma zona, grava o efeito e aciona `GO`; depois seleciona a próxima. Cada DRV continua executando autonomamente. O início terá pequeno desvio entre canais, que deve ser medido.

Essa é a primeira opção para impactos, clicks e pulsos curtos multi-zona.

### 8.2. RTP e envelopes próprios

Cada zona mantém:

- ativa/inativa;
- amplitude atual;
- frequência percebida e duty cycle;
- próximo deadline de troca;
- duração/TTL;
- prioridade e cooldown;
- falha e contador de atualizações perdidas.

O scheduler percorre apenas zonas ativas, seleciona o canal e escreve a amplitude quando o deadline vence. Não usar `delay()`. O desempenho deve ser medido com I²C a 400 kHz, variando 1, 2, 4 e 8 zonas ativas. Se o firmware não mantiver os deadlines, deve degradar de forma explícita — reduzir taxa/zonas de baixa prioridade — e nunca acumular uma fila atrasada.

### 8.3. Política de conflito

- Emergência e `stop all` têm prioridade absoluta.
- Um efeito mais prioritário pode substituir outro na mesma zona.
- Zonas diferentes podem executar efeitos distintos.
- Comandos idênticos próximos podem ser coalescidos sem somar intensidade indefinidamente.
- Cooldown é por zona/padrão; existe também um orçamento global de energia.
- Toda duração contínua expira sem renovação.

---

## 9. Comandos de bancada e contrato externo

Esta SPEC deve ser testável no Serial Monitor ou por um pequeno script, sem Exus Bridge.

Comandos humanos sugeridos:

```text
zones                         listar mapa e estado
scan all                      verificar TCAs e DRVs
pulse <zone> <intens%> <ms>   pulso em uma zona
effect <zone> <1-123>         efeito ROM em uma zona
group <mask> <pattern> ...    efeito em um grupo
stop <zone|all>               parada de zona ou global
status <zone|all>             diagnóstico
emergency                     parada e bloqueio globais
resume                        liberar após inspeção
```

Contrato compacto para automação/futura SPEC-003:

```text
H <version> <seq> <zone-mask> <pattern> <intensity> <duration> <crc>
Q <seq>                         consultar capacidades
A <seq> <result>               aceito
N <seq> <error>                rejeitado
X <seq>                        emergência
```

Para oito zonas, uma máscara de oito bits é suficiente. Para mais de oito, versionar o contrato com máscara maior ou lista de IDs; não reutilizar silenciosamente o mesmo campo com outra interpretação.

Exemplo de capabilities, ainda que o formato final seja compacto:

```json
{
  "protocol": 2,
  "zones_configured": 8,
  "zones_ready_mask": "0xEF",
  "max_group_size": 4,
  "features": ["rom", "rtp", "per_zone_limits", "watchdog"]
}
```

Essa resposta permite que qualquer ferramenta — Bridge, script de teste ou terminal — descubra a realidade do hardware em vez de presumir oito zonas.

---

## 10. Segurança e energia

### 10.1. Limites por zona

Cada zona possui intensidade máxima, duração máxima, cooldown e energia acumulada em janela móvel. O menor limite entre firmware, configuração do motor e perfil de teste vence.

### 10.2. Limites globais

- quantidade máxima de zonas simultâneas;
- soma máxima de amplitudes solicitadas;
- timeout de qualquer estado contínuo;
- parada global por comando e, idealmente, botão físico;
- bloqueio após brownout/reset repetido ou falha comum do I²C;
- inicialização sempre com saídas paradas.

### 10.3. Energia real

Validar em etapas: um motor, dois motores, quatro e oito. Medir corrente de pico, tensão no ESP32 e nos drivers, resets e temperatura. A fonte USB usada no MVP pode não sustentar todos os motores. Não aumentar capacitores ou fonte por tentativa durante uso corporal; dimensionar e revisar o circuito.

### 10.4. Isolamento de falhas

- NACK ao selecionar TCA: não acessar `0x5A` e marcar caminho com falha.
- NACK do DRV: parar logicamente a zona e continuar apenas se o barramento comum estiver íntegro.
- Zona inválida/desabilitada: NACK; nunca redirecionar para zona 0.
- Reset: todos os estados voltam a parado; nenhum comando antigo é retomado.

---

## 11. Roadmap de desenvolvimento e testes

### Fase 0 — Regressão sem multiplexador

- Criar abstrações de zona mantendo a ligação direta atual.
- Mapear o único motor como zona 0.
- Reexecutar todos os testes da SPEC-001.

**Gate:** nenhuma regressão no hardware atual.

### Fase 1 — Um TCA, um canal, um motor

- Ligar o hardware existente ao canal 0 do TCA.
- Implementar `MuxManager` e seleção exclusiva.
- Comparar efeitos e RTP com a ligação direta.
- Testar TCA ausente, canal errado e DRV ausente.

**Gate:** mesma sensação funcional, com diagnóstico correto e nenhuma seleção residual.

### Fase 2 — Duas zonas simétricas

- Adicionar segundo DRV/LRA.
- Implementar `ZoneMap`, limites e estado por zona.
- Testar esquerda, direita, alternância e disparo quase simultâneo.
- Medir desvio de início e crosstalk mecânico.

**Gate:** teste cego distingue lados acima da meta definida sem elevar amplitude para compensar erro de software/montagem.

### Fase 3 — Oito zonas por USB Serial

- Popular os oito canais planejados.
- Inicializar/calibrar sequencialmente.
- Testar cada zona individualmente e pares controlados.
- Medir energia com 1, 2, 4 e 8 zonas.
- Injetar falha em cada canal e repetir emergência.

**Gate:** oito IDs endereçam oito posições corretas; uma falha não aciona outra zona; fonte e barramento permanecem estáveis.

### Fase 4 — Scheduler multi-zona completo

- Validar efeitos ROM simultâneos.
- Validar RTP independente em cargas crescentes.
- Medir frequência de atualização, deadline perdido e jitter.
- Implementar prioridade, coalescência e orçamento global.
- Executar soak test mínimo de 30 minutos fora do corpo.

**Gate:** nenhum `delay()` bloqueante, fila sem crescimento, watchdog/emergência responsivos e métricas dentro das metas registradas.

### Fase 5 — Mais de um TCA, somente se necessário

- Adicionar segundo endereço TCA e poucas zonas de bancada.
- Confirmar que `selectExclusive` desabilita o mux anterior.
- Testar endereços duplicados, mux ausente e canais simultaneamente habilitados por falha.
- Reavaliar desempenho, capacitância, memória e fonte antes de expandir.

**Gate:** nenhuma transação atinge dois DRVs de mesmo endereço por engano.

### Fase 6 — Integração opcional com a SPEC-003

- Expor capabilities reais.
- Reproduzir pela Bridge a mesma suíte antes executada por Serial.
- Confirmar que jogo/adaptador usa IDs lógicos, não canais físicos.
- Repetir por USB Serial. Uma futura SPEC-004 poderá reutilizar a mesma suíte para validar Bluetooth.

**Observação:** esta fase não bloqueia a conclusão técnica da SPEC-002.

---

## 12. Independência entre SPEC-002 e SPEC-003

| Ordem de desenvolvimento | Como testar primeiro | Ponto de encontro posterior |
|---|---|---|
| SPEC-002 primeiro | Serial Monitor/script envia comandos por zona e grupo | Bridge consulta capabilities e passa a enviar máscaras/IDs reais |
| SPEC-003 primeiro | Bridge traduz jogos para a zona única da SPEC-001 | após atualizar firmware, a mesma Bridge descobre zonas e habilita espacialização |
| Em paralelo | cada trilha usa simuladores próprios | teste de contrato com fixtures e hardware quando ambos estiverem estáveis |

Contrato de responsabilidade:

- SPEC-002 decide **se** uma zona existe, seus limites e como ela é acionada.
- SPEC-003 decide **qual acontecimento do jogo** deve solicitar determinada zona/padrão.
- SPEC-003 nunca envia endereço TCA/canal; envia somente ID/máscara lógica.
- SPEC-002 nunca precisa conhecer `damage`, `wind` ou nomes de jogos; executa comandos hápticos genéricos.
- O firmware é a autoridade final de segurança, qualquer que seja o emissor.

---

## 13. Riscos principais

| Risco | Consequência | Mitigação |
|---|---|---|
| Canal errado fica selecionado | vibração em posição incorreta | seleção encapsulada, cache só após ACK, testes de rastreio e desligar canais após falha |
| Dois muxes mantêm `0x5A` conectado | colisão/broadcast não intencional | seleção exclusiva global e broadcast desabilitado |
| Estado ainda global | um comando cancela outra zona | array de estados e scheduler por zona |
| I²C insuficiente para RTP | jitter/pulsos deformados | benchmark crescente, escrever apenas em deadline e priorizar ROM quando adequado |
| Inicialização/calibração em massa | pico de corrente/movimento inesperado | uma zona por vez e modo de comissionamento fora do corpo |
| Fonte subdimensionada | brownout/reset e comportamento errático | medição incremental e orçamento global |
| Crosstalk mecânico | direção percebida errada | isolamento/fixação, teste cego e revisão física |
| Configuração física diverge do código | esquerda vira direita | ZoneMap versionado, etiqueta de cabos e teste de identificação antes da sessão |
| Zona falha silenciosamente | experiência enganosa | capabilities com ready mask, NACK e interface de diagnóstico |
| Complexidade para mais de oito zonas | firmware difícil sem benefício | validar oito primeiro; múltiplos TCAs somente por requisito demonstrado |

---

## 14. Impacto previsto no repositório

Estrutura esperada dentro de `firmware/` após a implementação:

```text
firmware/
├── README.md
├── firmware.ino
├── Config.h
├── MuxManager.h/.cpp
├── ZoneMap.h/.cpp
├── ZoneDriver.h/.cpp
├── MultiZoneScheduler.h/.cpp
├── SafetyManager.h/.cpp
├── CommandRouter.h/.cpp
├── Diagnostics.h/.cpp
└── tests/ ou sketches de validação
```

Os nomes finais podem reutilizar módulos atuais (`DriverHaptico`, `GeradorEnvelope`, `Seguranca`, `Comandos`) em vez de duplicá-los. A refatoração deve preservar histórico e testes; a árvore representa responsabilidades, não obriga reescrever tudo.

Configuração deve separar:

- topologia (`ZoneMap`);
- parâmetros elétricos de cada motor;
- limites de segurança;
- parâmetros de protocolo;
- flags de diagnóstico/comissionamento.

---

## 15. Estimativa de esforço

Estimativa para uma pessoa com experiência em Arduino/C++, sem incluir PCB, compras, montagem mecânica ou validação ética:

| Entrega | Esforço provável |
|---|---:|
| Refatorar uma zona para abstrações sem regressão | 3–5 pessoa-dias |
| `MuxManager`, descoberta e diagnóstico | 3–5 pessoa-dias |
| Duas zonas + estado/segurança por zona | 4–7 pessoa-dias |
| Oito zonas + scheduler/energia/soak tests | 8–15 pessoa-dias |
| Contrato de capabilities e testes de integração | 3–5 pessoa-dias |
| Segundo TCA, se realmente necessário | +3–6 pessoa-dias |
| **Total para oito zonas via Serial** | **18–32 pessoa-dias**, além do hardware |

---

## 16. Critérios de aceite

### Funcionais

- [ ] Ligação direta de uma zona continua suportada.
- [ ] Cada zona configurada corresponde a exatamente um TCA/canal/DRV/motor.
- [ ] Comandos individuais, grupo, parada por zona e parada total funcionam pela Serial.
- [ ] Efeitos ROM simultâneos e RTP independente possuem comportamento documentado.
- [ ] Capabilities relatam zonas configuradas e zonas prontas.

### Robustez

- [ ] Todos os TCAs começam com canais desabilitados.
- [ ] Mais de um mux nunca deixa caminhos independentes ativos durante uma transação normal.
- [ ] Zona inválida, ausente ou com NACK não redireciona comando.
- [ ] Remover qualquer DRV não impede diagnóstico/parada das zonas saudáveis.
- [ ] Brownout/reset retorna com todas as zonas paradas.
- [ ] Soak test não apresenta fila crescente, deadlock ou reset.

### Segurança

- [ ] Limites por zona e globais são aplicados no firmware.
- [ ] Emergência interrompe todas as zonas mesmo durante RTP multi-zona.
- [ ] Calibração multi-zona ocorre apenas fora do corpo e sequencialmente.
- [ ] Fonte, cabos e conectores foram validados na carga simultânea autorizada.
- [ ] Áreas excluídas no PDF não recebem atuador.

### Independência

- [ ] Toda a SPEC-002 pode ser demonstrada sem jogo e sem Exus Bridge.
- [ ] A SPEC-003 consegue continuar operando com uma zona quando este firmware não está instalado.
- [ ] A integração usa IDs/máscaras lógicas e capabilities, nunca endereço/canal físico hardcoded no PC.

---

## 17. Referências

- [SPEC-001 — firmware háptico de uma zona](SPEC-001.md)
- [SPEC-003 — Exus Bridge e demo jogável](SPEC-003.md)
- [Projeto Exus — A Frequência da Imersão](projeto_eksus_documentacao.pdf)
- [Texas Instruments — TCA9548A datasheet](https://www.ti.com/lit/ds/symlink/tca9548a.pdf)
- [Texas Instruments — DRV2605L datasheet](https://www.ti.com/lit/ds/symlink/drv2605l.pdf)
- [Adafruit — TCA9548A wiring and test](https://learn.adafruit.com/adafruit-tca9548a-1-to-8-i2c-multiplexer-breakout/arduino-wiring-and-test)
- [Adafruit — DRV2605L Arduino code](https://learn.adafruit.com/adafruit-drv2605-haptic-controller-breakout/arduino-code)

---

**Conclusão:** a evolução multi-zona pertence ao firmware e pode ser concluída por testes Serial independentemente da integração com jogos. A SPEC-003 deve enxergar zonas apenas como capacidades lógicas; isso permite desenvolver as duas trilhas em qualquer ordem e integrá-las sem misturar endereços I²C, regras de jogo ou responsabilidades de segurança.

---

## Adendo A - perfis ERM/LRA reais e política de calibração

**Correção (12 de agosto de 2026):** a premissa anterior de que os três atuadores do mux 1 eram LRA estava incorreta. A conferência dos produtos comprados identificou os seguintes perfis:

| Zonas | Atuador | Tipo | Configuração aplicada |
|---|---|---|---|
| 0 e 1 | moeda 1020, 10 x 2 mm, 3 V, 10.000 rpm | ERM | `ERM_COIN_*`, biblioteca ERM 1 e modo aberto; não executa auto-calibração LRA |
| 2 | bastão 0619AAC, 19 x 6 x 3 mm, 1,2 Vrms, 170 +/- 5 Hz, até 100 mA | LRA | `BAR_LRA_*`, biblioteca LRA 6, malha fechada e auto-calibração |

Para o 0619AAC, o firmware parte de `RATED_VOLTAGE=0x32` (1,2 Vrms com `SAMPLE_TIME=300 us`), `OD_CLAMP=0x50` (aprox. 1,70 Vp, conservador) e `DRIVE_TIME=0x18` (aprox. 2,9 ms, próximo de metade do período a 170 Hz). Esses valores devem ser confirmados em bancada com o motor fixado na montagem final. O anúncio fornece especificações de revenda, não um datasheet original do fabricante.

A auto-calibração do LRA continua sequencial e limitada a 1,5 s por zona durante o boot. Isso é bloqueante somente na inicialização, nunca no scheduler de RTP. Os ERMs em modo aberto não requerem nem podem usar a detecção automática de ressonância LRA.

No RTP bidirecional do ERM em modo aberto, `0x80` representa repouso; portanto, o driver converte internamente a amplitude lógica `0..127` para `0x80..0xFF`. Assim, o trecho "desligado" do envelope não aplica frenagem reversa contínua ao motor moeda.

Enquanto os ensaios do LRA ocorrem fora da máscara, `ALLOW_UNCALIBRATED_ZONES=1` permite que uma zona LRA cujo DRV foi encontrado, mas cuja calibração falhou, seja exposta como `READY_UNCALIBRATED`. Ela aceita exclusivamente RTP, limitado pelo firmware a 15% e 500 ms; efeitos ROM ficam bloqueados porque não permitem o mesmo controle fino de amplitude. A opção não se aplica às moedas ERM: elas devem surgir como `READY` quando o DRV estiver presente. Antes de testes na máscara/corpo, definir `ALLOW_UNCALIBRATED_ZONES=0` e exigir calibração bem-sucedida da zona LRA.

Referências de compra e configuração: [moeda ERM 1020](https://pt.aliexpress.com/item/1005009820342320.html), [bastão LRA 0619AAC](https://pt.aliexpress.com/item/1005006421331249.html) e [datasheet DRV2605L](https://www.ti.com/lit/ds/symlink/drv2605l.pdf).
