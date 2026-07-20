# SPEC-003 — Viabilidade da integração do Projeto Exus com uma demo jogável

> **Status:** proposta técnica para decisão e implementação
>
> **Data da pesquisa:** 19 de julho de 2026
>
> **Base mínima:** [SPEC-001 — firmware háptico de uma zona](SPEC-001.md) e [documentação técnica do Projeto Exus](projeto_eksus_documentacao.pdf)
>
> **Relacionada, mas independente:** [SPEC-002 — firmware multi-zona](SPEC-002.md). A Bridge pode nascer e ser testada com a única zona da SPEC-001; quando a SPEC-002 estiver pronta, a mesma integração passa a endereçar várias zonas.
>
> **Evolução reservada:** a implementação de Bluetooth será objeto de uma SPEC-004 futura. Esta SPEC preserva uma interface de transporte substituível, mas entrega e aceita somente USB Serial.
>
> **Escopo:** software entre uma experiência jogável em PC e a máscara Exus por USB Serial; não substitui validação elétrica, ergonômica, ética ou de segurança.

---

## 1. Decisão executiva

A integração é **tecnicamente viável** com o hardware e o firmware atuais, mas o MVP do repositório ainda não é uma integração de jogo pronta. Hoje ele controla **uma zona** por USB Serial, entende comandos manuais (`v`, `ef`, `s`, `e`) e aplica limites globais. Para uma demo jogável faltam uma ponte no PC, um protocolo com confirmação, eventos direcionais, watchdog de comunicação, arbitragem de eventos e, para espacialização real, múltiplas zonas.

> **A Exus Bridge ainda não existe e deverá ser construída pela equipe.** Ela é um segundo software, executado no PC ao lado do jogo. De um lado, recebe e interpreta os eventos emitidos pelo jogo; do outro, transforma esses eventos em comandos que o ESP32-C3 entende. Ela não é um cabo, driver pronto nem recurso interno do Arduino. Nesta proposta, é um novo entregável do Projeto Exus, com código, testes, configuração, logs e empacotamento próprios.

A recomendação é:

1. **Demo principal confirmada:** adaptar o [Third Person Shooter Demo oficial do Godot](https://godotengine.org/asset-library/asset/2710), versão 4.5, licença MIT. Godot deixa de ser apenas uma recomendação e passa a ser a engine oficial do MVP desta SPEC.
2. **Tecnologia da Bridge confirmada:** Python 3 com `asyncio`, sockets UDP e `pySerial`, empacotado como executável para Windows. Node.js e .NET deixam de ser alternativas do MVP.
3. **Arquitetura com cabo:** jogo → UDP local → **Exus Bridge, software novo a construir no PC** → USB Serial → ESP32-C3. A ponte desacopla o jogo do hardware e permite adicionar outros adaptadores no futuro sem reescrever a camada háptica.
4. **Prova secundária de reutilização comercial:** integrar [BeamNG.drive por UDP](https://documentation.beamng.com/modding/protocols/), usando telemetria oficial e, se necessário, um protocolo Lua personalizado. Colisões, aceleração e orientação são bons sinais para demonstrar impacto direcional.
5. **Trilhas independentes:** esta SPEC deve ser validada primeiro com o hardware atual de uma zona por USB. A evolução para multiplexadores e zonas pertence à SPEC-002 e pode acontecer antes, depois ou em paralelo. Quando ambas estiverem prontas, repete-se a suíte com várias zonas ainda por USB.
6. **Bluetooth fora desta entrega:** uma SPEC-004 futura definirá BLE/GATT, segurança, reconexão, pacotes e testes. A SPEC-003 apenas evita acoplamento que impeça essa evolução.
7. **Não usar como caminho principal:** captura de tela, leitura/injeção de memória, interceptação de áudio como única fonte ou mods invasivos em jogos com anti-cheat. Essas técnicas perdem significado semântico, são frágeis e podem contrariar termos do jogo.

Essa escolha é um meio-termo: não exige construir um jogo completo do zero, mas mantém controle total sobre os sinais necessários para uma demonstração segura, repetível e convincente.

---

## 2. O que já existe e o que ainda falta

### 2.1. Base confirmada neste repositório

| Capacidade | Estado atual | Consequência para a demo |
|---|---|---|
| ESP32-C3 + um DRV2605/LRA | Implementado | Permite validar a cadeia completa com uma sensação por vez. |
| RTP com envelope não bloqueante | Implementado | Permite variar ritmo, intensidade e duração percebidos. |
| Efeitos internos 1–123 | Implementado | Útil para pulsos curtos e impactos de baixo custo computacional. |
| USB Serial a 115200 baud | Implementado | É o transporte recomendado para a primeira demo. |
| Exus Bridge no PC | **Não existe** | Deve ser projetada, implementada, testada e empacotada pela equipe. |
| Limite de intensidade, duração, cooldown e emergência | Implementado | Boa base, mas os limites globais precisam virar limites por zona e classe de evento. |
| ACK/NACK, sequência e checksum | Ausente | A ponte ainda não consegue saber com segurança se um comando foi aceito. |
| Watchdog de link/heartbeat | Ausente | Um evento contínuo pode sobreviver a uma falha do jogo ou do cabo. |
| Filas e prioridade de eventos | Ausente | Eventos simultâneos podem se sobrescrever de forma arbitrária. |
| TCA9548A e várias zonas | Planejado | Necessário para direção espacial na máscara; um motor só comunica padrões, não localização. |

### 2.2. A Exus Bridge como novo produto de software

O Projeto Exus passa a ter três programas/componentes de código, e não apenas o firmware:

| Componente | Onde roda | Responsabilidade | Estado |
|---|---|---|---|
| Demo/adaptador do jogo | PC, dentro ou ao lado do jogo | Detectar tiro, dano, vento, posição e demais acontecimentos; emitir eventos canônicos. | A construir/adaptar. |
| **Exus Bridge** | PC, como processo separado | Receber eventos, aplicar tabela háptica, arbitrar prioridades, manter conexão com a máscara, registrar logs e mostrar diagnóstico. | **A construir do zero neste projeto.** |
| Firmware Exus | ESP32-C3 | Receber comandos, impor limites finais de segurança e controlar os DRV2605L/LRA. | MVP de uma zona já implementado; precisa evoluir. |

“Construir do zero” não significa implementar UDP ou Serial desde os bits básicos. A equipe reutilizará bibliotecas existentes, mas precisará escrever toda a **lógica específica do Exus**: contrato de eventos, tabela de sensações, transporte Serial, reconexão, interface de diagnóstico, logs, instalador e testes.

A Bridge deve ser tratada como um aplicativo de primeira classe, versionado em `exus-bridge/` na raiz do repositório, lado a lado com `firmware/`, e com executável próprio. O jogo não deve abrir a porta COM diretamente; ele só publica acontecimentos. A interface de transporte deve permitir que a SPEC-004 acrescente Bluetooth sem modificar o adaptador Godot.

### 2.3. Estrutura de pastas após a criação da Bridge

A reorganização deve acontecer junto com a primeira implementação funcional da Exus Bridge. Até lá, não é necessário criar pastas vazias. A estrutura-alvo é:

```text
future-makers-26/
├── README.md                         # novo guia geral do Projeto Exus
├── docs/
│   ├── SPEC-001.md                   # firmware/hardware de uma zona
│   ├── SPEC-002.md                   # evolução multi-zona do firmware
│   ├── SPEC-003.md                   # integração jogável e Exus Bridge
│   └── projeto_eksus_documentacao.pdf
├── firmware/
│   ├── README.md                     # README atual, movido e revisado
│   ├── firmware.ino
│   ├── Config.h
│   ├── DriverHaptico.h/.cpp
│   ├── GeradorEnvelope.h/.cpp
│   ├── Seguranca.h/.cpp
│   └── Comandos.h/.cpp
└── exus-bridge/
    ├── README.md                     # instalação e uso da Bridge
    ├── pyproject.toml                # dependências e build Python confirmados
    ├── src/
    │   └── exus_bridge/
    │       ├── app/                  # CLI/UI e estado da aplicação
    │       ├── events/               # schema e validação do evento canônico
    │       ├── haptics/              # tabela, prioridades e arbitragem
    │       ├── protocol/             # H/A/N/X/K/Q, CRC e framing
    │       ├── transports/           # SerialTransport, MockTransport e extensão futura
    │       └── observability/        # logs, métricas e replay
    ├── config/
    │   ├── haptic-map.example.json
    │   └── devices.example.json
    ├── tests/
    │   ├── unit/
    │   ├── integration/
    │   └── fixtures/
    └── scripts/                      # empacotamento e utilitários de desenvolvimento
```

Os nomes internos de `exus-bridge/` podem ser ajustados após o primeiro spike, mas a tecnologia está confirmada como Python. As decisões que devem permanecer estáveis são: Bridge na raiz, código-fonte separado de configuração/testes e `SerialTransport` atrás de uma interface que permita transportes futuros.

O código da demo Godot não aparece na árvore acima porque sua forma de distribuição ainda depende do tamanho dos assets e das respectivas licenças. Antes de importá-lo, decidir entre `demo/` neste repositório, submódulo ou repositório separado. Essa decisão não altera as posições de `firmware/` e `exus-bridge/`.

#### Responsabilidade de cada README

| Arquivo | Conteúdo esperado |
|---|---|
| `/README.md` | visão geral do Exus; diagrama jogo → Bridge → firmware → hardware; estado das trilhas de firmware, Bridge e integração; pré-requisitos gerais; início rápido por componente; segurança; mapa do repositório; links para specs. Não deve duplicar manuais completos. |
| `/firmware/README.md` | conteúdo técnico hoje existente no README da raiz: materiais, ligação, Arduino IDE, configuração, upload, comandos, testes e solução de problemas do firmware. |
| `/exus-bridge/README.md` | finalidade da Bridge; instalação Python; execução em modo simulado e USB Serial; seleção de porta; configuração da tabela háptica; logs; protocolo; testes e empacotamento. Bluetooth será documentado pela SPEC-004 quando existir. |

#### Migração planejada dos READMEs

1. Mover o `README.md` atual para `firmware/README.md`, preservando seu histórico no Git.
2. Revisar todos os links após a mudança: referências a `docs/` passarão a usar `../docs/`, e links que hoje começam com `firmware/` devem se tornar relativos à nova localização.
3. Criar um novo `README.md` na raiz, orientado ao projeto completo — firmware + Exus Bridge — e não apenas ao Arduino.
4. Criar `exus-bridge/README.md` junto do primeiro código executável da Bridge; não entregar a pasta apenas com documentação vazia.
5. Executar um verificador de links locais e atualizar qualquer automação que presuma o README antigo na raiz.

### 2.4. Limitações que afetam diretamente a jogabilidade

- `MIN_CMD_INTERVAL_MS = 100` limita a entrada a, no máximo, cerca de dez comandos aceitos por segundo.
- `MIN_COOLDOWN_MS = 300` é global; passos, vento e dois impactos próximos podem ser descartados mesmo que pertençam a zonas diferentes.
- O buffer Serial atual tem 64 bytes, insuficiente para JSON descritivo com folga.
- Um comando de duração zero pode vibrar continuamente até receber `s`; em jogo, efeitos contínuos devem depender de heartbeat e expirar automaticamente.
- O firmware possui apenas um estado de envelope. Com oito atuadores, cada zona precisará de estado, temporização, prioridade e teto próprios.

Portanto, **não é recomendado ligar o jogo diretamente ao parser atual e considerar a integração concluída**. Um primeiro protótipo pode usar `v`/`ef`, mas a demo apresentável deve usar o protocolo proposto na Seção 8.

---

## 3. Experiência que a demonstração deve provar

A demo não precisa ser um jogo longo. Ela precisa provar, de modo repetível, que um evento virtual se torna uma sensação física correta.

### 3.1. Roteiro recomendado: “Corredor Exus”

Uma sessão de 4–6 minutos, derivada do TPS Demo do Godot:

1. **Calibração:** pulso curto em cada zona, confirmação do usuário e ajuste de intensidade confortável.
2. **Vento frontal:** um corredor com fluxo de ar virtual gera envelope leve enquanto o personagem permanece na área.
3. **Ameaça lateral:** projéteis ou inimigos à esquerda/direita produzem pulsos no lado correspondente.
4. **Dano e impacto:** um disparo recebido gera pulso rápido cuja intensidade é derivada do dano, sempre limitada pelo perfil local.
5. **Ameaça traseira:** aproximação de inimigo fora do campo de visão gera pulsos progressivos na nuca.
6. **Explosão final:** padrão amplo e curto, seguido de cooldown; nunca uma vibração longa de amplitude máxima.
7. **Tela de resultados:** latência medida, eventos enviados/aceitos/descartados, falhas e parada de emergência.

### 3.2. Degradação controlada para o hardware de uma zona

O MVP de um motor continua útil. Nele, direção é codificada por **vocabulário temporal**, não por localização:

| Significado | Padrão provisório em um motor |
|---|---|
| esquerda | dois pulsos curtos |
| direita | um pulso longo-curto |
| frente | pulso único seco |
| atrás | três pulsos lentos |
| vento | tremor leve com timeout renovável |
| explosão | pulso forte com queda rápida |

Essa versão valida software, latência e distinguibilidade. Ela **não deve ser apresentada como prova de direção facial**; isso exige pelo menos duas zonas e, idealmente, a topologia de oito zonas prevista no PDF.

---

## 4. Construir, adaptar uma demo aberta ou reutilizar um jogo comercial

### 4.1. Comparação de estratégias

| Estratégia | Prazo inicial | Qualidade dos eventos | Controle de direção | Risco de manutenção | Licença/custo | Veredito |
|---|---:|---|---|---|---|---|
| Jogo curto criado do zero | médio | excelente | excelente | baixo | depende dos assets | Bom se a equipe já domina uma engine. |
| **Adaptar demo aberta oficial** | **baixo–médio** | **excelente** | **excelente** | **baixo–médio** | **Godot TPS: MIT** | **Recomendado.** |
| Template proprietário editável | baixo–médio | excelente | excelente | médio | termos da engine/template | Viável com equipe experiente em Unity/Unreal. |
| Jogo comercial com telemetria oficial | baixo para prova; médio para acabamento | médio | variável | médio | exige o jogo | Ótima demonstração secundária. |
| Jogo comercial com mod/plugin | médio | bom–excelente | bom | alto | termos do jogo e do loader | Viável, mas dependente de versões. |
| Áudio → vibração | baixo | baixo | baixo–médio por estéreo | médio | baixo | Apenas baseline ou fallback. |
| Captura de tela/OCR | médio | baixo–médio | baixo | alto | variável | Não recomendado. |
| Leitura/injeção de memória | alto | potencialmente alto | potencialmente alto | muito alto | risco jurídico/anti-cheat | Excluído do plano. |

### 4.2. Prós de uma demo sob controle da equipe

- Eventos semânticos no instante correto: `damage`, `explosion`, `wind_enter`, `threat_angle`.
- Direção disponível como vetor ou azimute, sem inferência por áudio ou pixels.
- Cenário curto e determinístico, importante para apresentações e testes comparativos.
- Possibilidade de modo simulado sem máscara, logs e repetição automatizada.
- Segurança desenhada junto com a mecânica: densidade de eventos, intensidade e pausas são controláveis.
- Ausência de anti-cheat e menor risco de uma atualização externa quebrar a demonstração.

### 4.3. Contras de uma demo própria

- Exige trabalho de design, build, distribuição e QA.
- Pode parecer menos reconhecível ao público do que um jogo comercial.
- Assets, áudio e interface ainda precisam de curadoria e licenças compatíveis.
- Uma equipe sem experiência em engine pode gastar mais tempo no jogo do que na háptica.

### 4.4. Prós de um jogo existente

- Reconhecimento imediato e mecânicas já polidas.
- Carga visual, áudio e comportamento emergente mais próximos de uso real.
- Telemetria oficial pode acelerar uma prova de conceito específica.
- Demonstra a capacidade do Exus de funcionar como plataforma, não só com uma aplicação.

### 4.5. Contras de um jogo existente

- A API pode não expor direção, tipo de dano ou o instante exato do contato.
- Atualizações do jogo, mod loader ou anti-cheat podem interromper a integração.
- Instalação, conta, compra, DLC e configuração aumentam a fragilidade de uma apresentação.
- A equipe não controla a cadência dos estímulos; uma cena intensa pode violar o orçamento háptico.
- Mods e redistribuição exigem revisão dos termos e das licenças de cada componente.

---

## 5. Bases editáveis para a demo principal

### 5.1. Godot TPS Demo — decisão confirmada

O [TPS Demo oficial](https://github.com/godotengine/tps-demo) já contém movimento, mira, disparo, inimigos e um nível jogável. A versão publicada na Asset Library está sob **MIT**, e o repositório mantém uma branch compatível com Godot 4.x.

**Técnica de integração:** adicionar um `ExusEventBus` autoload e publicar eventos nos pontos em que o jogo já calcula disparo, dano, morte, explosão e posição relativa. O Godot possui [`PacketPeerUDP`](https://docs.godotengine.org/en/stable/classes/class_packetpeerudp.html) nativo, suficiente para enviar datagramas para `127.0.0.1` sem plugin Serial dentro da engine.

**Prós:** licença simples, código disponível, exportação sem royalties da engine, eventos exatos e arquitetura leve.

**Contras:** o projeto é grande para uma demo; será preciso remover conteúdo, revisar assets e isolar uma sequência curta. A ponte Serial continua sendo um processo separado.

### 5.2. Unity FPS Microgame

O [FPS Microgame oficial](https://learn.unity.com/project/fps-template?signup=true) é um FPS 3D preparado para modificação e usa Unity 2022.3 na página consultada.

**Técnica:** emitir o evento canônico em métodos de dano, projétil, colisão e zonas ambientais; enviar UDP à ponte.

**Prós:** onboarding guiado, ecossistema amplo e muitos desenvolvedores C#.

**Contras:** template e versão mais antigos, termos/licenças do ecossistema Unity e risco de dependências de pacotes. Escolher apenas se a equipe já trabalha com Unity.

### 5.3. Unreal Lyra

O [Lyra Sample Game](https://dev.epicgames.com/documentation/en-us/unreal-engine/lyra-sample-game-in-unreal-engine) fornece modos Elimination, Control e Exploder, armas, bots e uma arquitetura modular baseada em plugins.

**Técnica:** um plugin `ExusHaptics` assina mensagens/gameplay abilities e publica eventos UDP.

**Prós:** base visualmente forte, Gameplay Ability System e exemplo próximo de produção.

**Contras:** instalação pesada, curva de aprendizado e complexidade excessiva para o MVP. O uso e a distribuição são regidos pela [EULA do Unreal](https://www.unrealengine.com/eula/unreal). É indicado somente se a equipe já domina Unreal.

---

## 6. Jogos existentes e como extrair eventos

### 6.1. BeamNG.drive — melhor candidato comercial

O BeamNG permite que software e hardware externos recebam dados por protocolos UDP; o [OutGauge oficial](https://documentation.beamng.com/modding/protocols/) fornece velocidade, pedais e estado básico, e a documentação permite implementar protocolo próprio. A arquitetura expõe partes substanciais em Lua, inclusive missões e veículos. Para uma integração mais rica, o ecossistema BeamNG também documenta sensores de [IMU com aceleração e velocidade angular](https://documentation.beamng.com/beamng_tech/sensors/advanced-imu/) e sensores de dano via BeamNGpy/BeamNG.tech.

**Interpretação háptica:**

- pico de aceleração longitudinal → colisão frontal/traseira;
- pico lateral → impacto esquerdo/direito;
- velocidade e irregularidade do piso → textura leve, com forte filtragem;
- aumento de dano → pulso de impacto;
- derrapagem/ABS/rotação → tremor limitado.

**Prós:** telemetria oficial, física rica, excelente associação entre vetor e zona.

**Contras:** OutGauge sozinho é básico; direção/dano detalhados podem exigir Lua ou produto/API adicional. Exige cópia licenciada do jogo e teste contra a versão instalada.

### 6.2. Minecraft Java Edition + Fabric

A [Fabric API](https://docs.fabricmc.net/develop/events) fornece callbacks de eventos e permite mixins quando não existe um hook. Há eventos explícitos de dano e morte, como [`ServerLivingEntityEvents.AFTER_DAMAGE`](https://maven.fabricmc.net/docs/fabric-api-0.115.6%2B1.21.1/net/fabricmc/fabric/api/entity/event/v1/ServerLivingEntityEvents.html).

**Interpretação háptica:** tipo e valor do dano → padrão/intensidade; posição da fonte menos posição do jogador, rotacionada pelo yaw da câmera → azimute; quebra de bloco → textura; proximidade de entidade hostil → ameaça progressiva.

**Prós:** eventos semânticos, jogo conhecido, mod controlável e direção calculável.

**Contras:** toolchain Java/Fabric, diferenças cliente-servidor, manutenção a cada versão e necessidade de instalar o mod. Nem todo evento possui hook pronto.

### 6.3. Counter-Strike 2 via Game State Integration

A [Game State Integration da Valve](https://developer.valvesoftware.com/wiki/Counter-Strike:_Global_Offensive_Game_State_Integration) envia estados em JSON para um endpoint HTTP configurado. Ela pode revelar mudanças de vida/armadura, estado da rodada, bomba e alguns estados do jogador; um adaptador pode gerar eventos por **delta**, por exemplo, `vida_anterior - vida_atual`.

**Interpretação háptica:** perda de vida → dano; estado de queima/flash → alertas distintos; bomba plantada/contagem → pulso rítmico.

**Limitação central:** GSI não deve ser presumida como fonte de vetor preciso do atacante. Sem direção confiável, o efeito deve ser global ou usar apenas dados explicitamente fornecidos. A documentação pública original é de CS:GO e a compatibilidade exata com a versão atual de CS2 deve ser validada em um spike. A existência de suporte nativo/mod a muitos jogos, inclusive CS2, no ecossistema [bHaptics](https://www.bhaptics.com/games/) mostra que a classe de integração é possível, mas não torna o fluxo de eventos deles reutilizável pelo Exus.

**Regra:** usar somente GSI/configuração documentada. Não injetar DLL, ler memória do processo nem contornar anti-cheat.

### 6.4. Euro Truck Simulator 2 / American Truck Simulator

O [SCS Telemetry SDK](https://modding.scssoft.com/wiki/Documentation/Engine/SDK/Telemetry) existe para aplicações de terceiros e suporta Windows, Linux e macOS.

**Interpretação háptica:** velocidade, aceleração, freio, rotação, dano e estados do veículo → textura de rodagem, frenagem, colisão e alerta lateral quando o dado estiver disponível.

**Prós:** API oficial, experiência fácil de explicar e telemetria contínua.

**Contras:** exige plugin nativo, o material público está desatualizado em partes e o SDK é mais adequado a estado veicular do que a eventos semânticos de combate.

### 6.5. Microsoft Flight Simulator via SimConnect

O [SimConnect SDK](https://docs.flightsimulator.com/html/Programming_Tools/SimConnect/SimConnect_SDK.htm) permite componentes externos em C/C++ ou .NET monitorarem a simulação, solicitarem variáveis e assinarem eventos.

**Interpretação háptica:** touchdown/trem de pouso → impacto; turbulência e G → envelope variável; stall/overspeed → alertas; inclinação/vento relativo → zonas laterais.

**Prós:** API oficial e muitos sinais físicos.

**Contras:** ambiente pesado, necessidade de mapear variáveis por aeronave e menor alinhamento com a narrativa inicial de tiro/ameaça facial.

### 6.6. Mod genérico em jogos Unity/.NET

Para títulos sem SDK, [BepInEx](https://docs.bepinex.dev/articles/dev_guide/plugin_tutorial/index.html) carrega plugins .NET, e [Harmony](https://harmony.pardeike.net/articles/patching.html) permite Prefix/Postfix/Transpiler em métodos do jogo.

**Prós:** acesso semântico pode ser excelente em jogos single-player.

**Contras:** nomes e assinaturas mudam a cada atualização; jogos IL2CPP exigem fluxo adicional; pode conflitar com termos ou anti-cheat. Usar somente em título offline, permitido e congelado em uma versão de demonstração.

### 6.7. Áudio para háptica

Produtos comerciais oferecem **audio-to-haptics** para jogos sem integração, como documentado pela [bHaptics](https://support.bhaptics.com/en/support/solutions/articles/150000210165-what-are-natively-supported-and-mod-supported-games-).

Uma implementação Exus poderia separar bandas, detectar transientes e usar energia estéreo para estimar lado. Contudo, tiro, porta batendo e explosão podem ocupar bandas semelhantes; música e fala geram falsos positivos; e “frente/atrás” não é observável de forma confiável em estéreo comum.

**Uso recomendado:** protótipo de compatibilidade universal e comparação experimental, nunca fonte principal da demo.

---

## 7. Arquitetura proposta

### 7.1. Arquitetura inicial: máscara conectada por cabo

```text
┌──────────────────────┐    UDP localhost     ┌──────────────────────────┐
│ Jogo / adaptador     │ ───────────────────► │ Exus Bridge (PC)         │
│ Godot, Unity, mod,   │  evento canônico     │ - valida e normaliza     │
│ telemetria ou áudio  │                      │ SOFTWARE NOVO A CONSTRUIR│
└──────────────────────┘                      │ - mapeia evento→padrão   │
                                              │ - arbitra e registra     │
                                              └────────────┬─────────────┘
                                                           │ USB Serial
                                                           │ comando compacto + ACK
                                              ┌────────────▼─────────────┐
                                              │ ESP32-C3                 │
                                              │ - limites irremovíveis   │
                                              │ - watchdog / filas       │
                                              │ - scheduler por zona     │
                                              └────────────┬─────────────┘
                                                           │ I²C
                                              ┌────────────▼─────────────┐
                                              │ TCA9548A → DRV2605L/LRA │
                                              │ 1 zona no MVP; até 8    │
                                              └──────────────────────────┘
```

Nesta etapa, o cabo USB tem duas funções possíveis: transportar os comandos e alimentar a placa. A porta COM vista pelo Windows é a interface usada pela Bridge para enviar e receber mensagens do firmware.

### 7.2. Restrição arquitetural para a futura SPEC-004

```text
┌──────────────────────┐    UDP localhost     ┌──────────────────────────┐
│ Jogo / adaptador     │ ───────────────────► │ Exus Bridge (PC)         │
└──────────────────────┘                      │ mesmos eventos e mapas   │
                                              └────────────┬─────────────┘
                                                           │ Bluetooth LE / GATT
                                                           │ comandos + ACK/status
                                              ┌────────────▼─────────────┐
                                              │ ESP32-C3 + máscara       │
                                              │ mesmos limites/scheduler │
                                              └──────────────────────────┘
```

Uma futura implementação Bluetooth deverá alterar somente o **adaptador de transporte** entre a Bridge e o ESP32-C3. O jogo, o evento canônico, a tabela háptica, as prioridades e as regras de segurança deverão continuar iguais. Nesta SPEC, o requisito arquitetural é manter `SerialTransport` atrás de uma interface; `BleTransport` não faz parte da implementação nem do aceite.

O ESP32-C3 oferece **Bluetooth Low Energy (BLE)**, mas não Bluetooth Classic. Essa constatação orienta a futura SPEC-004: ela deverá estudar serviço GATT, biblioteca compatível com a versão Arduino/ESP-IDF, segurança e comportamento de desconexão. Nenhuma dessas escolhas fica implícita como entrega da SPEC-003.

Bluetooth transmite **dados, não energia**. A futura SPEC-004 deverá separar a substituição do cabo de dados do eventual projeto de bateria. Uma versão realmente sem cabo continuará exigindo especificação elétrica e mecânica própria.

### 7.3. Por que usar uma ponte no PC

- Godot, Unity, Unreal e jogos comerciais passam a falar o mesmo contrato.
- A engine não precisa gerenciar porta COM, reconexão ou permissões do driver.
- Perfis e tabela háptica podem ser ajustados em JSON sem recompilar o jogo ou o firmware.
- A ponte pode simular hardware, gravar/reproduzir sessões e medir latência.
- Um transporte futuro pode substituir USB Serial em um módulo, sem alterar cada jogo.
- Segurança crítica permanece duplicada no firmware; fechar a ponte não pode deixar motor ligado.

### 7.4. Tecnologia confirmada para a ponte

O MVP da Exus Bridge será implementado em **Python 3**, com `asyncio`, sockets UDP e [pySerial](https://pyserial.readthedocs.io/en/latest/pyserial_api.html), e empacotado como executável para Windows. A versão menor exata do Python será fixada no `pyproject.toml` no bootstrap, junto com o lock das dependências. Node.js e .NET não fazem parte das alternativas de implementação desta SPEC.

A Bridge terá `SerialTransport`. Sua interface mínima deve oferecer: selecionar porta COM, conectar/desconectar, enviar, receber, parar tudo, consultar status e informar o estado do link. A interface não deve pressupor detalhes exclusivos da Serial, permitindo que a futura SPEC-004 acrescente `BleTransport` sem alterar a lógica de jogo.

### 7.5. Glossário para leitura deste plano

| Termo | Explicação em linguagem simples |
|---|---|
| **Software** | Conjunto de programas e regras executadas por um computador. A Exus Bridge será um software novo no PC. |
| **Firmware** | Software gravado dentro do ESP32-C3. Ele continua funcionando sem a interface visual do PC e controla diretamente o hardware. |
| **Engine de jogo** | Ferramenta/base usada para criar e executar um jogo, como Godot, Unity ou Unreal. |
| **Adaptador/plugin do jogo** | Pequeno módulo que observa o que ocorreu no jogo e avisa a Bridge. Cada jogo pode precisar de um adaptador diferente. |
| **Exus Bridge** | Aplicativo intermediário do PC: traduz eventos do jogo em comandos hápticos, gerencia a conexão e registra diagnósticos. Não existe pronto; será desenvolvido pela equipe. |
| **UDP** | Forma rápida de um programa enviar pequenos pacotes de dados para outro. É adequado para eventos em tempo real, mas, sozinho, não garante entrega nem ordem; por isso a solução usa sequência, expiração e logs. |
| **localhost / `127.0.0.1`** | Endereço que significa “este mesmo computador”. O jogo envia UDP para a Bridge dentro do PC, sem depender da internet ou do roteador. |
| **IP e porta** | O IP identifica a máquina; a porta identifica qual programa deve receber o pacote. Exemplo: Bridge escutando em `127.0.0.1:4242`. |
| **USB Serial** | Comunicação de texto ou bytes pelo cabo USB como se fosse uma linha serial. No Windows aparece normalmente como uma porta `COM`. |
| **Porta COM** | Nome dado pelo Windows a uma conexão serial, por exemplo `COM5`. A Bridge abre essa porta para conversar com o firmware. |
| **Bluetooth Low Energy (BLE)** | Variante de Bluetooth voltada a dispositivos de baixo consumo e pequenos pacotes. É o tipo suportado pelo ESP32-C3. |
| **Bluetooth Classic** | Variante tradicional usada por alguns fones e pelo perfil Serial Port. O ESP32-C3 **não** a suporta. |
| **GAP** | Parte do BLE responsável por anunciar o dispositivo, encontrá-lo e estabelecer a conexão. |
| **GATT** | Modelo do BLE que organiza dados em serviços e características, comparáveis a “pastas e campos” que o PC lê ou escreve. |
| **Serviço/característica BLE** | Um serviço agrupa uma função; uma característica é um campo de dados. O Exus pode ter uma característica de comando e outra de status. |
| **Central/cliente GATT** | Papel normalmente exercido pelo PC: procura a máscara, conecta e envia comandos. |
| **Periférico/servidor GATT** | Papel normalmente exercido pelo ESP32-C3: anuncia “Exus”, aceita a conexão e expõe suas características. |
| **I²C** | Barramento de dois fios usado dentro da máscara para o ESP32 conversar com o TCA9548A e os DRV2605L. Não é a conexão entre jogo e PC. |
| **JSON** | Formato de texto com nomes e valores, fácil de ler e depurar. É usado entre jogo e Bridge, não necessariamente no link compacto com o ESP32. |
| **Evento canônico** | Descrição padronizada de algo que ocorreu — por exemplo, dano com direção e intensidade — independentemente do jogo de origem. |
| **ACK / NACK** | Respostas “aceitei” e “não aceitei”. Permitem que a Bridge saiba o que o firmware fez. |
| **Heartbeat** | Mensagem curta e periódica que diz “a conexão ainda está viva”. |
| **Watchdog** | Temporizador de proteção. Se o heartbeat parar, o firmware interrompe efeitos contínuos. |
| **TTL / timeout** | Prazo de validade/tempo máximo de espera. Um comando expirado deixa de produzir vibração. |
| **CRC** | Pequeno código calculado sobre a mensagem para detectar corrupção acidental durante a transmissão. Não é criptografia. |
| **Latência** | Tempo entre o acontecimento no jogo e o início da vibração. |
| **Jitter** | Variação da latência: ora a resposta chega rápida, ora mais lenta. |
| **Biblioteca** | Código reutilizável para resolver uma tarefa, como abrir Serial ou BLE. Usar uma biblioteca não elimina o desenvolvimento da lógica específica da Bridge. |

---

## 8. Contratos de comunicação

### 8.1. Evento canônico: jogo → ponte

JSON é adequado no loopback, onde legibilidade importa mais que alguns bytes:

```json
{
  "schema": "exus.game-event/1",
  "seq": 1042,
  "game_time_ms": 82133,
  "event": "damage",
  "azimuth_deg": -65.0,
  "elevation_deg": 3.0,
  "magnitude": 0.42,
  "duration_ms": 80,
  "state": "oneshot",
  "source": "projectile"
}
```

Regras:

- `magnitude` sempre entre 0 e 1 e nunca equivale diretamente à corrente do motor.
- Azimute: `0°` frente, `-90°` esquerda, `+90°` direita, `±180°` atrás.
- `oneshot` é pontual; `start/update/stop` representa vento, textura ou ameaça contínua.
- Evento contínuo deve enviar renovação periódica e possuir TTL.
- A ponte rejeita schema desconhecido, campos não finitos e mensagens grandes.

### 8.2. Comando compacto: ponte → ESP32

Não enviar o JSON acima ao parser atual. A 115200 baud, uma linha de 150 bytes ocupa aproximadamente 13 ms apenas na serialização física (10 bits por byte); um comando de 25–35 bytes ocupa cerca de 2–3 ms.

Formato textual versionado para o MVP:

```text
H 1 1042 02 3 42 80 7A1C
│ │ │    │  │ │  │  └─ CRC16
│ │ │    │  │ │  └──── duração ms
│ │ │    │  │ └─────── intensidade 0..100 (ainda limitada no firmware)
│ │ │    │  └───────── padrão id
│ │ │    └──────────── máscara de zonas (hexadecimal)
│ │ └───────────────── sequência
│ └─────────────────── versão do protocolo
└───────────────────── comando háptico
```

Respostas:

```text
A 1042 0        # aceito
N 1042 3 LIMIT  # rejeitado/ajustado por limite
S 1042          # parado
```

Controles especiais:

- `X <seq>` — emergência; sempre preemptivo e fora de rate limit;
- `K <seq>` — heartbeat;
- `Q <seq>` — status/capacidades;
- `C ...` — calibração, habilitada somente em modo local explícito.

### 8.3. Segurança do protocolo

- Timeout de link inicial sugerido: **250 ms** para efeitos de estado; ao expirar, parar as zonas contínuas. O valor deve ser medido e ajustado, não tratado como norma fisiológica.
- `duration_ms = 0` não é aceito de fontes de jogo; somente uma ferramenta de bancada autenticada/local pode iniciar contínuo sem TTL.
- Número de sequência evita repetir um pacote após reconexão.
- CRC detecta corrupção; ACK/NACK melhora diagnóstico, mas não autoriza elevar limites.
- Limites máximos são compilados no firmware e podem ser apenas reduzidos pelo perfil do PC.

### 8.4. Requisitos entregues à futura SPEC-004

Esta seção registra restrições para a futura SPEC-004 e **não autoriza nem exige implementar BLE nesta SPEC**. O conteúdo lógico de `H/A/N/X/K/Q` deverá permanecer equivalente nos dois transportes, evitando protocolos com comportamentos incompatíveis.

| Função | USB Serial | Bluetooth LE |
|---|---|---|
| Descobrir hardware | listar portas COM | procurar anúncio BLE com nome/UUID do Exus |
| Conectar | abrir a porta COM | conectar e descobrir o serviço GATT |
| Enviar comando | escrever linha/pacote na Serial | escrever na característica `Command RX` |
| Receber ACK/status | ler da Serial | receber notification da característica `Status TX` |
| Detectar link morto | erro da porta + heartbeat | evento de desconexão + heartbeat |
| Parar com segurança | `X`, timeout e watchdog | os mesmos `X`, timeout e watchdog |

Pontos que a SPEC-004 deverá decidir e versionar:

- `Exus Haptic Service`: UUID próprio e versionado;
- `Command RX`: PC escreve comandos hápticos, heartbeat e emergência;
- `Status TX`: ESP32 notifica ACK, NACK, falhas I²C, bateria e estado;
- `Device Info`: leitura de versão de firmware, número de zonas e capacidades;
- `Safety State`: leitura do estado de emergência e dos limites ativos.

Pacotes BLE pequenos simplificam a compatibilidade. O tamanho disponível por escrita depende do ATT MTU negociado entre PC e ESP32. O protocolo deve, portanto, caber no menor pacote escolhido — preferencialmente uma representação binária curta — ou implementar fragmentação e remontagem com sequência, tamanho e timeout. Não se deve presumir que uma linha Serial de 25–35 bytes caberá inteira em qualquer conexão BLE.

Para uma apresentação pública, o ESP32 não deverá aceitar comandos de qualquer computador próximo. Pareamento/bonding, identificação do cliente permitido e reconexão serão critérios da SPEC-004. Mesmo com criptografia do BLE, os tetos de segurança permanecerão no firmware.

---

## 9. Tradução de jogo para sensação

### 9.1. Tabela inicial

| Evento canônico | Detecção ideal | Zonas (8 canais) | Padrão inicial | Prioridade |
|---|---|---|---|---:|
| `damage` | callback nativo + vetor da fonte | setor do azimute; têmporas com teto menor | pulso seco 60–90 ms | 90 |
| `explosion` | evento nativo + posição/raio | setores voltados à explosão, máximo de 3–4 zonas | forte com queda 120–180 ms | 100 |
| `collision` | evento ou pico de aceleração | frente/lado/trás conforme vetor | pulso + cauda curta | 95 |
| `wind` | entrada/saída de volume ou telemetria | testa + bochechas | envelope leve, TTL renovado | 20 |
| `footstep_threat` | IA/evento + distância | lado/nuca mais próximo | pulso espaçado; amplitude cresce com proximidade | 40 |
| `threat_behind` | posição relativa fora do FOV | nuca | 2–3 pulsos lentos | 60 |
| `weapon_fire` | callback do jogador | mandíbula/lado dominante, opcional | click curto | 30 |
| `low_health` | cruzamento de limiar, não por frame | nuca alternada | batimento limitado | 50 |

### 9.2. Mapeamento direcional

1. Calcular `relative = source_position - player_position`.
2. Projetar no plano horizontal.
3. Converter para o espaço local da câmera/personagem.
4. Calcular `atan2(local_x, -local_z)` e converter para graus.
5. Quantizar com histerese em setores: frente, frente-esquerda, esquerda, trás, direita e frente-direita.
6. Se a fonte for desconhecida, usar zona global segura; **não inventar direção**.

### 9.3. Arbitragem

- Emergência e `stop` preemptam tudo.
- Explosão/dano podem interromper vento na mesma zona.
- Eventos de baixa prioridade podem ser descartados quando a fila ultrapassa o orçamento.
- Eventos idênticos em uma janela curta são combinados, sem somar amplitude indefinidamente.
- Cooldown é por zona e padrão, não global.
- O scheduler deve preservar no máximo um efeito ativo por zona e limitar energia acumulada em uma janela móvel.

O DRV2605L oferece efeitos em ROM, RTP, controle I²C e início típico de saída de aproximadamente **0,7 ms** após GO/trigger; esses recursos são documentados no [datasheet da Texas Instruments](https://www.ti.com/lit/ds/symlink/drv2605l.pdf). O maior risco de latência está, portanto, nas camadas de captura, transporte, fila e mecânica, não apenas no chip.

---

## 10. Metas de desempenho e medição

Estudos de sincronismo visual-háptico encontraram percepção de assincronia em torno de 45 ms em uma tarefa de colisão com force feedback; outro estudo com display vibrotátil encontrou mudanças perceptivas em torno de 40 ms e detecção de atraso em torno de 60 ms. Esses resultados não são limites universais para a face ou para jogos, mas justificam uma meta conservadora de baixa latência ([Vogels, 2004](https://doi.org/10.1518/hfes.46.1.118.30394); [Okamoto et al., 2009](https://doi.org/10.1109/TOH.2009.17)).

### 10.1. Orçamento alvo, não garantia

| Etapa | Meta p95 |
|---|---:|
| callback do jogo → datagrama UDP | ≤ 8 ms |
| ponte: parse, mapa e fila | ≤ 2 ms |
| comando compacto + USB Serial | ≤ 10 ms |
| firmware: parse, validação e agenda | ≤ 5 ms |
| comando I²C → início elétrico | ≤ 2 ms |
| **evento → início elétrico total** | **≤ 30 ms p95** |
| **evento → vibração mecanicamente detectável** | **≤ 50 ms p95** |

### 10.2. Como medir

- Inserir `game_time_ms` e timestamps monotônicos em cada camada.
- Sincronizar somente relógios no mesmo PC; o ESP32 retorna seus próprios `micros()` no ACK.
- Medir saída elétrica com analisador lógico/osciloscópio.
- Medir início mecânico com acelerômetro fixado ao LRA ou microfone de contato; filmagem comum não basta.
- Registrar p50, p95, p99, jitter, perdas, descartes por segurança e diferença entre comando e início físico.

---

## 11. Desafios e mitigação

| Risco | Impacto | Mitigação proposta |
|---|---|---|
| Latência e jitter variáveis | sensação “atrasada” ou evento errado | UDP local, pacote compacto, thread/fila dedicada, telemetria de p95/p99. |
| Cabo USB desconecta ou processo fecha | efeito contínuo preso | heartbeat, TTL e fail-safe no firmware; reconexão começa com `stop all`. |
| Tempestade de eventos | fadiga, fila atrasada | coalescência, prioridade, teto de fila e orçamento por zona. |
| Cooldown global atual | perda de eventos válidos | cooldown por zona/classe; emergência e stop sempre passam. |
| JSON maior que buffer | truncamento e comandos inválidos | JSON só no UDP; protocolo Serial compacto e buffer com descarte explícito. |
| Direção ausente na API do jogo | feedback enganoso | efeito não direcional ou jogo/adaptador com vetor real. |
| Múltiplos DRV2605L no endereço 0x5A | conflito I²C | TCA9548A e seleção de canal antes de cada acesso. |
| RTP independente em oito zonas | scheduler e tráfego I²C mais complexos | efeitos ROM para impactos; RTP por estados de zona e benchmark a 400 kHz. |
| Queda de tensão com vários motores | reset/efeito errático | fonte dimensionada, desacoplamento, teste 1→2→8 motores e medição de corrente. |
| Crosstalk mecânico na máscara | direção percebida incorreta | isolamento entre suportes, rigidez controlada e teste cego por zona. |
| Contato/pressão inconsistentes | intensidades diferentes por usuário | encaixe ajustável, calibração individual e limites conservadores. |
| Adaptação e mascaramento tátil | padrões deixam de ser distinguíveis | pausas, poucos padrões, baixa densidade e evitar zonas simultâneas demais. Há evidência de [adaptação vibrotátil na face](https://pubmed.ncbi.nlm.nih.gov/2011449/). |
| Atualização de jogo/mod | demo quebra na apresentação | pin de versão, teste de inicialização e build offline conhecido. |
| Anti-cheat/termos | bloqueio de conta ou distribuição | SDK/telemetria oficial; nada de injeção/memória; revisão jurídica antes de distribuir. |
| Perfil do PC eleva amplitude | desconforto | PC só pode pedir; firmware aplica teto final por zona. |
| Logs expõem dados do usuário | privacidade | sem nome/conta; IDs aleatórios; retenção curta e consentida. |
| **SPEC-004 futura:** BLE sofre interferência ou sai de alcance | atraso, perda de pacote ou desconexão | heartbeat, watchdog local, teste no ambiente real e fallback por USB. |
| **SPEC-004 futura:** outro PC encontra a máscara por BLE | comandos não autorizados | pareamento/bonding, cliente permitido e limites sempre locais no firmware. |
| **SPEC-004 futura:** pacote BLE excede o MTU útil | fragmentação ou comando truncado | pacote binário curto ou protocolo explícito de fragmentação/remontagem. |
| **SPEC-004 futura:** BLE é confundido com alimentação sem fio | protótipo continua preso ao USB ou usa bateria improvisada | separar teste de dados BLE do projeto formal de bateria/regulação/proteção. |

### 11.1. Observação de segurança humana

O PDF interno já exclui olhos, pálpebras, lábios e mucosas do MVP e exige progressão bancada → mão/antebraço → região aprovada. Isso permanece obrigatório. A literatura mostra que a estimulação vibrotátil facial produz adaptação e respostas fisiológicas; estudos clínicos de outros wearables não estabelecem automaticamente segurança para esta máscara, motor, montagem ou dose.

Não há, nesta especificação, base para declarar o Exus um dispositivo médico ou “seguro para qualquer usuário”. Interromper diante de dor, dormência, tontura, alteração visual, irritação, cefaleia ou desconforto. Testes com pessoas precisam ser voluntários, documentados e aprovados conforme a instituição responsável.

---

## 12. Plano de implementação

Esta SPEC possui um núcleo concluível com o hardware atual e um ponto posterior de integração multi-zona. A [SPEC-002](SPEC-002.md) possui roadmap próprio para multiplexadores, drivers e zonas; portanto, nenhuma das duas precisa bloquear a outra. Bluetooth não aparece como fase: será planejado e aceito exclusivamente pela SPEC-004 futura.

| Trilha/marco | Responsabilidade | Dependência | O que será provado |
|---|---|---|---|
| **A. Bridge + demo, uma zona via USB** | SPEC-003 | SPEC-001 | jogo, Bridge e firmware funcionam ponta a ponta com padrões temporais no hardware atual |
| **B. Firmware multi-zona via USB** | SPEC-002 | SPEC-001 | zonas são endereçáveis e seguras por comandos de bancada, sem exigir jogo ou Bridge |
| **C. Integração multi-zona via USB** | encontro entre SPEC-002 e SPEC-003 | trilhas A e B | eventos direcionais da demo selecionam IDs lógicos de zonas reais |

Se a SPEC-003 for desenvolvida primeiro, a trilha A entrega uma demo jogável completa em modo de uma zona e a trilha C fica adiada. Se a SPEC-002 terminar primeiro, suas zonas são testadas pela Serial e a trilha C pode começar assim que a Bridge entender o contrato de capabilities.

Se a montagem física atual tiver mais de um motor acionado como um único conjunto, ela ainda conta como **uma zona lógica**. Direção real só é validada quando cada zona pode ser comandada de modo independente.

### Fase 0 — Gate do hardware atual

- Executar os seis testes da SPEC-001.
- Registrar motor, tensão nominal, ressonância, limites usados e resultado da calibração.
- Demonstrar emergência e timeout fora do corpo.

**Saída:** hardware atual confiável; sem isso, o software de jogo apenas mascara falhas elétricas.

### Fase 1 — Construção da Exus Bridge e do simulador

- Criar o novo aplicativo em `exus-bridge/`, na raiz e ao lado de `firmware/`; ele ainda não está presente no repositório.
- Mover o README atual para `firmware/README.md`, corrigir seus links e criar o novo README geral na raiz.
- Criar `exus-bridge/README.md` com instruções que funcionem desde o primeiro executável.
- Implementar recepção UDP local, validação do evento canônico, tabela háptica, `SerialTransport` e modo sem hardware.
- Criar painel/CLI com seleção da porta COM e eventos manuais: dano esquerda/direita, explosão, vento, ameaça atrás e stop.
- Implementar logs JSON Lines, métricas e replay determinístico.
- Usar os comandos atuais temporariamente para validar ponta a ponta.
- Empacotar a Bridge como executável reproduzível para o PC de apresentação.

**Critério:** estrutura e três READMEs conforme a Seção 2.3, sem links locais quebrados; 1.000 eventos simulados sem travar; stop/emergência sempre aceitos; uma pessoa leiga consegue selecionar a porta e verificar “conectado”.

### Fase 2 — Protocolo de jogo no firmware

- Implementar `H/A/N/X/K/Q`, sequência, CRC e capabilities.
- Tornar duração obrigatória para fonte de jogo.
- Adicionar watchdog, fila limitada, prioridade e métricas.
- Separar rate limit de `stop`, emergência, heartbeat e atualização de estado.

**Critério:** desconectar o cabo/processo durante vento e observar parada automática no prazo configurado.

### Fase 3 — Demo Godot com hardware atual via cabo

- Fixar uma release/commit do TPS Demo e registrar licenças dos assets.
- Adicionar `ExusEventBus` e o adaptador UDP.
- Construir o roteiro da Seção 3 com modo tutorial e sem eventos aleatórios excessivos.
- Exibir estado da Bridge/hardware sem bloquear o gameplay se a máscara estiver ausente.
- Aplicar o vocabulário temporal de uma zona descrito na Seção 3.2.

**Núcleo da SPEC-003 concluído quando:** pelo menos quatro eventos chegam por jogo → UDP → Bridge → USB Serial → ESP32 e produzem padrões distintos no hardware atual; perda de cabo ou Bridge sempre para efeitos contínuos. A ausência da SPEC-002 não bloqueia este aceite.

### Fase 4 — Ponto de integração opcional com a SPEC-002, ainda via cabo

- Não implementar TCA9548A, `ZoneMap`, scheduler por zona ou segurança elétrica nesta SPEC; essas entregas pertencem à [SPEC-002](SPEC-002.md).
- Fazer a Bridge consultar capabilities e trabalhar com IDs/máscaras lógicas, nunca com endereço de multiplexador ou canal I²C.
- Reproduzir pela Bridge a suíte que a SPEC-002 já executou por Serial.
- Validar primeiro esquerda/direita, depois frente/trás e somente então todas as zonas disponíveis.
- Confirmar degradação explícita para uma zona quando o firmware anunciar somente `zone_count=1`.
- Manter USB Serial durante este encontro; rádio permanece fora do escopo.

**Integração multi-zona concluída quando:** os eventos direcionais selecionam os IDs lógicos esperados durante uma sessão completa, a Bridge não conhece a topologia I²C e emergência/watchdog continuam funcionando. A conclusão desta fase não é necessária para aceitar a demo de uma zona.

### Fase 5 — Prova opcional com jogo comercial

- Primeiro spike: BeamNG OutGauge → Bridge → máscara.
- Segundo spike opcional: protocolo Lua com vetor de impacto/dano.
- Manter o mesmo evento canônico; apenas o adaptador do jogo muda.
- Executar por USB Serial. Uma repetição sem fio pertence à futura SPEC-004.

**Critério:** demonstrar que a Bridge não depende da engine da demo principal.

### Fase 6 — Estudo de experiência

- Sessão de treinamento curta com os padrões.
- Ordem contrabalanceada: jogo sem háptica, háptica de uma zona e háptica espacial.
- Medir reconhecimento de direção/evento, conforto, presença, latência percebida e fadiga.
- Coletar motivo de cada interrupção e nunca pressionar o participante a continuar.

---

## 13. Estimativa de esforço

Estimativa para uma pessoa com experiência em Python/C++ e conhecimento básico de Godot; não inclui fabricação da máscara, PCB, compras, aprovação ética ou arte nova.

| Entrega | Esforço provável |
|---|---:|
| Reorganização do repositório + três READMEs | 1–2 pessoa-dias |
| Bridge + simulador + logs | 4–7 pessoa-dias |
| Protocolo, watchdog e ACK no firmware de uma zona | 4–7 pessoa-dias |
| Adaptação e roteiro enxuto no Godot TPS | 7–12 pessoa-dias |
| QA, instrumentação de latência e empacotamento | 5–8 pessoa-dias |
| **Demo de uma zona** | **20–34 pessoa-dias** |
| Integração da Bridge com um firmware multi-zona já entregue pela SPEC-002 | +2–4 pessoa-dias |
| Firmware, TCA, scheduler e segurança multi-zona | estimativa própria da SPEC-002; fora do esforço desta SPEC |
| Adaptador BeamNG | +3–7 pessoa-dias para OutGauge; mais para Lua/dano detalhado |

O total da demo de uma zona independe do cronograma de mecânica, energia e calibração multi-zona. Esses riscos continuam relevantes para o ponto de integração com a SPEC-002, mas não devem ser contabilizados como implementação da Exus Bridge. Bluetooth, GATT, testes de rádio e alimentação portátil não estão estimados nesta SPEC; caberão à SPEC-004 futura e, no caso da alimentação, ao projeto elétrico correspondente.

---

## 14. Critérios de aceite da demo

### Funcionais

- [ ] O usuário experimenta ao menos quatro eventos distintos.
- [ ] Todo evento recebido tem tipo, sequência, magnitude limitada, duração/TTL e origem registrada.
- [ ] Eventos direcionais acionam a zona correta ou assumem explicitamente modo de uma zona.
- [ ] Jogo e ponte funcionam em modo simulado sem hardware.
- [ ] A Exus Bridge possui executável/versionamento próprios e deixa claro quando jogo, porta COM e hardware estão conectados.
- [ ] A raiz contém o novo README geral; os manuais específicos estão em `firmware/README.md` e `exus-bridge/README.md`.
- [ ] Os exemplos de instalação/execução dos três READMEs foram testados e não há links locais quebrados.
- [ ] Reconexão sempre começa com `stop all` e nova negociação de capabilities.

### Desempenho e robustez

- [ ] Latência evento→início mecânico medida; meta p95 ≤ 50 ms.
- [ ] Nenhum motor permanece ligado após perda de heartbeat além do timeout configurado.
- [ ] 10.000 eventos sintéticos sem overflow, deadlock ou reset.
- [ ] Evento duplicado, fora de ordem, corrompido e grande demais é rejeitado de modo observável.
- [ ] Com firmware multi-zona disponível, uma zona ausente é reportada pela interface lógica e não provoca redirecionamento para outra zona; o comportamento elétrico/I²C é aceito pela SPEC-002.

### Integração opcional com múltiplas zonas

- [ ] O firmware multi-zona atende aos critérios próprios da SPEC-002 antes de ser usado no aceite integrado.
- [ ] A Bridge descobre a quantidade e os IDs das zonas por capabilities.
- [ ] A Bridge nunca recebe nem armazena endereços de TCA ou canais I²C.
- [ ] O mesmo replay direcional produz a sequência lógica esperada de zonas por USB Serial.
- [ ] A indisponibilidade da SPEC-002 não impede a demonstração e o aceite da SPEC-003 em modo de uma zona.

### Segurança e experiência

- [ ] Parada física ou comando de emergência funciona em qualquer estado.
- [ ] Teto de intensidade e duração é aplicado no firmware por zona.
- [ ] Calibração ocorre fora do rosto antes de cada sessão.
- [ ] Áreas proibidas pelo PDF interno não recebem atuador.
- [ ] O participante pode interromper a sessão sem justificativa.
- [ ] Em teste cego após treinamento, definir meta inicial de ≥ 80% de identificação entre quatro padrões; se não alcançada, reduzir o vocabulário em vez de aumentar amplitude.

---

## 15. Decisões que devem permanecer explícitas

1. **A demo principal é uma adaptação aberta controlada**, não um mod de jogo competitivo.
2. **BeamNG é validação secundária de portabilidade**, não dependência da apresentação principal.
3. **O evento canônico é independente da engine** e termina na ponte.
4. **O firmware é a autoridade final de segurança**; o jogo não define amplitude elétrica.
5. **SPEC-002 e SPEC-003 são trilhas independentes.** A Bridge deve funcionar com a zona única atual; o firmware multi-zona deve funcionar por Serial sem jogo; o aceite integrado acontece quando ambas estiverem prontas.
6. **Não há direção real com um motor.** Padrões temporais são apenas uma degradação funcional.
7. **A integração não usa leitura de memória, injeção ou contorno de anti-cheat.**
8. **A Exus Bridge é software novo; Bluetooth não faz parte desta entrega.** A interface de transporte deve permitir que a SPEC-004 acrescente BLE sem alterar jogo, eventos ou tabela háptica.
9. **Firmware e Bridge são componentes de primeira classe na raiz.** O README raiz apresenta o projeto inteiro; cada componente mantém seu próprio manual operacional.
10. **A implementação oficial do MVP usa Python 3 e Godot TPS Demo 4.5.** Alternativas de linguagem ou engine exigirão decisão posterior e não fazem parte do escopo contratado por esta SPEC.
11. **A SPEC-004 está reservada para Bluetooth.** Ela deverá definir GATT, pacotes, segurança, reconexão, regressão de uma/múltiplas zonas e a separação entre dados sem fio e alimentação portátil.

---

## 16. Referências consultadas

### Projeto e hardware

- [SPEC-001 — Firmware háptico para ESP32](SPEC-001.md)
- [SPEC-002 — Evolução do firmware para múltiplas zonas hápticas](SPEC-002.md)
- [Projeto Exus — A Frequência da Imersão](projeto_eksus_documentacao.pdf)
- [Texas Instruments — DRV2605L datasheet](https://www.ti.com/lit/ds/symlink/drv2605l.pdf)
- [Texas Instruments — TCA9548A datasheet](https://www.ti.com/lit/ds/symlink/tca9548a.pdf)
- [Espressif — Arduino ESP32 USB CDC](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/usb_cdc.html)
- [Espressif — visão geral de BLE no ESP32-C3](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/api-guides/ble/overview.html)
- [Espressif — API BLE e exemplo BLE UART Service](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/api-reference/bluetooth/index.html)
- [pySerial — API](https://pyserial.readthedocs.io/en/latest/pyserial_api.html)

### Engines e bases editáveis

- [Godot — TPS Demo na Asset Library](https://godotengine.org/asset-library/asset/2710)
- [Godot — código-fonte do TPS Demo](https://github.com/godotengine/tps-demo)
- [Godot — PacketPeerUDP](https://docs.godotengine.org/en/stable/classes/class_packetpeerudp.html)
- [Unity — FPS Microgame](https://learn.unity.com/project/fps-template?signup=true)
- [Epic Games — Lyra Sample Game](https://dev.epicgames.com/documentation/en-us/unreal-engine/lyra-sample-game-in-unreal-engine)
- [Epic Games — Unreal Engine EULA](https://www.unrealengine.com/eula/unreal)

### Jogos, telemetria e mods

- [BeamNG — protocolos externos/OutGauge UDP](https://documentation.beamng.com/modding/protocols/)
- [BeamNG — arquitetura e Lua](https://documentation.beamng.com/beamng_tech/architecture/)
- [BeamNG — Advanced IMU](https://documentation.beamng.com/beamng_tech/sensors/advanced-imu/)
- [Fabric — sistema de eventos](https://docs.fabricmc.net/develop/events)
- [Fabric API — eventos de dano/morte](https://maven.fabricmc.net/docs/fabric-api-0.115.6%2B1.21.1/net/fabricmc/fabric/api/entity/event/v1/ServerLivingEntityEvents.html)
- [Valve — Game State Integration](https://developer.valvesoftware.com/wiki/Counter-Strike:_Global_Offensive_Game_State_Integration)
- [SCS Software — Telemetry SDK](https://modding.scssoft.com/wiki/Documentation/Engine/SDK/Telemetry)
- [Microsoft Flight Simulator — SimConnect SDK](https://docs.flightsimulator.com/html/Programming_Tools/SimConnect/SimConnect_SDK.htm)
- [BepInEx — criação de plugin](https://docs.bepinex.dev/articles/dev_guide/plugin_tutorial/index.html)
- [Harmony — patching](https://harmony.pardeike.net/articles/patching.html)
- [bHaptics — integração nativa, mods e audio-to-haptics](https://support.bhaptics.com/en/support/solutions/articles/150000210165-what-are-natively-supported-and-mod-supported-games-)

### Percepção e segurança

- [NCBI Bookshelf — Physiology, Vibratory Sense](https://www.ncbi.nlm.nih.gov/books/NBK542288/)
- [Hollins et al. — Vibrotactile adaptation on the face](https://pubmed.ncbi.nlm.nih.gov/2011449/)
- [Vogels — Detection of Temporal Delays in Visual-Haptic Interfaces](https://doi.org/10.1518/hfes.46.1.118.30394)
- [Okamoto et al. — Detectability and Perceptual Consequences of Delayed Feedback](https://doi.org/10.1109/TOH.2009.17)
- [Singhal e Schneider — Juicy Haptic Design](https://uwaterloo.ca/haptic-experience-lab/projects/juicy-haptic-design-vibrotactile-embellishments-can-improve)

---

**Conclusão:** a SPEC-003 confirma Python 3 para a Exus Bridge e Godot TPS Demo 4.5 para a experiência principal, começando pela zona única atual via USB Serial. Em paralelo ou em outra ordem, a SPEC-002 pode evoluir o firmware e provar as zonas pela Serial. As trilhas se encontram por IDs lógicos e capabilities, sem transferir à Bridge a responsabilidade por multiplexadores. Bluetooth fica reservado para uma SPEC-004 futura, que poderá reutilizar o jogo, a tabela háptica e o protocolo lógico sem ampliar o aceite desta entrega.
