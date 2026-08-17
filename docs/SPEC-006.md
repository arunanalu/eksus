Exit code: 0
Wall time: 0.2 seconds
Output:
# SPEC-006 — Boat Demo leve integrada ao Exus Control

> **Status:** implementação inicial concluída; requer validação em Godot, PC-alvo e hardware.
>
> **Substitui como próxima demo:** a execução da SPEC-005 no protótipo. A demo TPS fica preservada como referência do contrato, mas não será usada no PC fraco.
>
> **Depende de:** UDP local, modo simulado e `VirtualExusTransport` da SPEC-004.

---

## 1. Objetivo e decisões

Construir **Exus Boat Demo**, uma experiência de navegação a vela curta: o vento varia, muda a aparência do mar/vela/barco e é sentido como vibração direcional contínua. Virar o barco ou mudar o vento desloca a sensação suavemente entre esquerda, testa e direita. Colidir com gelo flutuante produz impacto mais forte.

| Item | Decisão |
|---|---|
| Projeto | `game/boat-demo/` |
| Engine | Godot 4.5.2 Standard, GDScript, renderer **Compatibility** |
| Apresentação | 2D top-down estilizado; sem 3D, sombras, pós-processamento ou partículas caras |
| Integração | UDP em `127.0.0.1:4242`; `exus.game-event/1` |
| Controles | A/D ou setas: leme; W/S: vela; Esc: pausa/sair |
| Duração | roteiro guiado de 2–3 min |
| Windows | `Exus-Boat-Demo.exe` + `.pck`, distribuídos juntos em ZIP |

O 2D preserva o experimento háptico e elimina o custo que tornou a TPS Demo inadequada. A arte será mar azul-petróleo, espuma clara, barco contrastante e icebergs geométricos; assets devem ser próprios ou licenciados e documentados.

### 1.1 Direção visual: bonito sem pesar

“Leve” não significa visual pobre. A referência é uma ilustração marítima viva:
mar azul profundo, ondas em faixas suaves, reflexos dourados móveis, espuma
branca estilizada e silhuetas limpas. O barco, os icebergs e a ilha distante
usam cores quentes/frias bem contrastadas para permanecerem legíveis.

O mar é uma composição GPU barata de um único `CanvasItem`/malha 2D grande
com shader próprio. Não haverá água 3D, SSR, iluminação em tempo real,
reflexos por render target nem texturas de vídeo.

| Camada | Técnica | Custo/objetivo |
|---|---|---|
| Base | gradiente vertical de profundidade e ruído discreto | dá volume ao oceano sem texture grande |
| Ondas distantes | 2–3 senoides lentas deslocando cor e linhas | parallax estilizado, sem simulação |
| Ondas próximas | normal/fake-light por derivada analítica simples | movimento perceptível ao redor do barco |
| Brilho solar | faixa especular animada e mascarada pelas ondas | reflexo convincente, sem refletir a cena |
| Espuma | textura pequena repetível + máscara de velocidade/iceberg | resposta visual ao movimento |
| Detalhes | sprites de barco, gelo e ilha; animação curta | identidade artística com baixo uso de memória |

Parâmetros como paleta, altura/velocidade das ondas, direção do sol e densidade
de espuma ficam em `config/visual/sea_style.json`. Isso permite criar
variações de clima sem recompilar nem alterar o shader.

### 1.2 Como os gráficos serão criados durante o desenvolvimento

O agente trabalha em ciclos curtos e verificáveis:

1. desenha primeiro as formas essenciais em código: mar, casco, vela, gelo e
   espuma, para validar leitura, câmera e desempenho;
2. implementa o shader de mar com parâmetros expostos e compara capturas em
   720p e 1080p;
3. quando um elemento precisa de acabamento ilustrado, gera asset 2D pequeno
   (por exemplo, barco, iceberg, ilha e nuvens) com fundo transparente,
   documenta prompt/origem/licença e importa-o como sprite/atlas;
4. revisa capturas dentro do Godot, reduz resolução/quantidade de camadas se
   necessário e só então congela o asset no repositório.

O jogo não depende de geração nem internet em execução: imagens geradas são
arquivos versionados. O shader continua responsável por ondas, brilho, reflexo
estilizado e espuma, pois esses elementos precisam responder ao vento e ao
movimento do barco em tempo real.

### 1.3 Orçamento de desempenho visual

- alvo: 60 FPS a 1280×720 no PC fraco de referência; 30 FPS continua jogável,
  mas reprova o aceite visual;
- uma passagem de shader para o mar; no máximo três funções de onda e uma
  textura repetível de até 512 px para espuma/ruído;
- resolução interna ajustável: 100%, 75% ou 50%, com preset **Baixo** já
  selecionável no menu;
- no máximo 10 icebergs ativos e sem partículas persistentes fora da câmera;
- qualidade do mar, VSync, resolução e tela cheia ficam no menu e em
  `sea_style.json`; o preset nunca altera o contrato háptico.

## 2. Topologia háptica do protótipo

Esta demo usa somente IDs lógicos já expostos pelo firmware. O jogo nunca envia I²C, mux, canal ou tipo de motor.

| ID | Mux/canal | Atuador | Posição | Uso |
|---:|---|---|---|---|
| 0 | 1/0 | moeda ERM | lado esquerdo do rosto | vento/impacto à esquerda |
| 1 | 1/1 | moeda ERM | lado direito do rosto | vento/impacto à direita |
| 2 | 1/2 | bastão LRA | testa | vento frontal/central e impacto comum |

Se alguma zona não estiver `READY`, o Control aplica a degradação declarada no perfil ou rejeita o estímulo. Nunca redireciona a sensação silenciosamente.

## 3. Mecânica e eventos

O barco usa movimento cinemático simples (`CharacterBody2D` ou equivalente), sem física naval complexa. A cada frame calcula rumo, vento relativo e força da vela. Icebergs são `Area2D` com formas simples; no toque o barco perde velocidade, desliza e recebe breve feedback visual.

O diretor interpola `wind_world_deg` e `wind_strength`; não há mudanças em degrau. O jogo calcula:

```text
wind_relative_deg = wrap(wind_world_deg - boat_heading_deg, -180, 180)
```

Assim, virar o barco altera a direção háptica sem o Control conhecer o mundo.

| Situação | Evento | Estado | Dados |
|---|---|---|---|
| abrir/jogar | `wind` | `start` | `stream_id="boat-wind"`, direção e força relativas |
| vento ou rumo muda | `wind` | `update` | no máximo 10 Hz; somente quando valor significativo mudar |
| pausa, troca de cena, saída | `wind` | `stop` | mesmo `stream_id` |
| tocar iceberg | `ice_collision` | `oneshot` | azimute, força da colisão, duração curta |

Todo evento inclui `haptic_profile: "boat-demo/v1"`. Esse campo opcional é adicionado ao schema v1 com padrão `default/v1`, sem quebrar a demo TPS. `magnitude` é sempre `0..1`, não corrente nem intensidade elétrica. Colisão usa debounce inicial de 800 ms por iceberg.

## 4. Mapeamento háptico Boat Demo

### Vento e propagação

Centros angulares: zona 0 = `-90°`, zona 2 = `0°`, zona 1 = `+90°`. O Control transforma a direção contínua em níveis independentes por zona:

```text
weight(zone) = max(0, cos(delta(azimuth, zone_center))) ^ direction_sharpness
target(zone) = wind_magnitude * weight(zone) * zone_gain(zone)
actual(zone) += (target(zone) - actual(zone)) * smoothing_alpha
```

O filtro é aplicado no Control: updates recebidos até 10 Hz, comandos enviados no máximo 4 Hz e TTL curto. Isso faz a sensação atravessar suavemente 0 → 2 → 1 ou 1 → 2 → 0 quando barco/vento mudam. Histerese desliga zonas próximas de zero sem tremulação. O jogo mantém apenas a direção contínua.

O vento inicia com RTP leve: 10–18 Hz percebidos, renovação de 250 ms e TTL de 600 ms. O LRA mantém sua ressonância; a frequência aqui é somente o envelope percebido. Todas as intensidades passam pelos limites do firmware.

### Colisão com gelo

`ice_collision` tem prioridade superior: pausa/atenua vento nas zonas afetadas, envia pulso direcional e permite que o próximo update retome o campo suavemente. Valores iniciais conservadores: 80–140 ms, 24–35 Hz e 18–32%. Impactos frontais acentuam testa; laterais acentuam a moeda correspondente, sem ativar tudo no máximo.

## 5. Configuração e calibração

O arquivo versionado `game/boat-demo/config/haptics/boat-demo.v1.json` é a fonte de ajuste humano. O build do Control inclui cópia validada em `exus_control/profiles/`; um teste exige mesma versão e checksum. O jogo lê parâmetros visuais/eventos; o Control lê níveis por zona e gera comandos.

```json
{
  "profile_id": "boat-demo/v1",
  "zones": {
    "0": {"label": "face_left_coin", "bearing_deg": -90, "gain": 0.75, "min_pct": 6, "max_pct": 22},
    "1": {"label": "face_right_coin", "bearing_deg": 90, "gain": 0.75, "min_pct": 6, "max_pct": 22},
    "2": {"label": "forehead_bar", "bearing_deg": 0, "gain": 0.60, "min_pct": 5, "max_pct": 18}
  },
  "wind": {"frequency_hz": 14, "update_hz": 10, "command_hz": 4, "ttl_ms": 600, "smoothing_ms": 350, "direction_sharpness": 1.6},
  "ice_collision": {"frequency_hz": 30, "duration_ms": 110, "min_pct": 18, "max_pct": 32, "cooldown_ms": 800}
}
```

`gain`, `min_pct` e `max_pct` permitem calibrar uma zona fraca/forte sem código. Alterações são feitas em bancada, em passos pequenos, e não superam o teto do firmware. O app mostra e registra perfil/versão ativos.

## 6. Ajustes necessários

### Exus Control — obrigatório

O mapper atual reduz `wind` a uma zona e usa `group` com intensidade uniforme; isso não representa a propagação de três zonas. Implementar:

1. aceitar `ice_collision` e `haptic_profile` no validador;
2. `BoatDemoMapper`, selecionado por perfil, preservando o mapper TPS;
3. `HapticIntent` com níveis/comandos por zona, em vez de uma string única;
4. filtro, histerese, limite de 4 Hz, TTL e stop do stream de vento;
5. preempção por gelo e retomada suave do vento;
6. painel com perfil, níveis 0/2/1, modo simulado/real e degradações;
7. testes de campo direcional, zona ausente, TTL, impacto, NACK, timeout e queda.

O Godot não ganha BLE, Serial ou configuração elétrica.

### Firmware — recomendado para fluxo contínuo

`pulse` reinicia a zona e o cooldown atual de 300 ms impediria atualização suave. Adicionar comando genérico de stream, por exemplo `stream <mask> <níveis> <ttl_ms> <hz>`, que valida tudo antes de aplicar e:

- atualiza RTP ativo sem `stop`/cooldown entre updates;
- permite amplitude independente por zona;
- mantém prioridade, orçamento global e expiração TTL;
- permite stop apenas da máscara/stream;
- obedece emergência, watchdog BLE e desconexão;
- é testável pela Serial e pelo `VirtualExusTransport`.

Não reduzir `MIN_COOLDOWN_MS` globalmente como atalho. Se esse comando não ficar pronto, usar fallback explícito de pulsos de 600 ms renovados a 2 Hz; ele não é aceite para a propagação suave final.

## 7. Estrutura esperada

```text
game/boat-demo/
├── project.godot
├── README.md
├── export_presets.cfg
├── scenes/main.tscn
├── scripts/boat.gd
├── scripts/wind_director.gd
├── scripts/iceberg.gd
├── exus/                         # Event Bus, UDP e overlay adaptados
├── config/haptics/boat-demo.v1.json
├── assets/
├── LICENSES/ASSET-SOURCES.md
└── tools/build_windows.ps1

exus_control/
├── profiles/boat-demo.v1.json
├── profile_loader.py
├── haptic_mapper.py
└── tests/test_boat_demo_profile.py
```

## 8. Plano incremental

### E06-01 — Bootstrap leve e arte-base

- criar projeto 2D Compatibility, mar, barco, vela, câmera e controles;
- criar o primeiro shader de mar: gradiente, duas camadas de onda e brilho
  solar; expor seus parâmetros em `sea_style.json`;
- fixar engine, fontes de assets e licenças; sem downloads em runtime;
- estabelecer meta 1280×720, 60 FPS em PC fraco de referência sem GPU dedicada.

**Aceite:** clone limpo abre, navega sem Control e exibe um mar animado,
legível e estável a 60 FPS no PC de referência.

### E06-02 — Navegação e vento determinísticos

- implementar vento relativo, vela e diretor com semente/roteiro fixo;
- overlay de desenvolvimento mostra rumo, vento relativo e força.

**Aceite:** virar o barco desloca corretamente a direção exibida, sem salto.

### E06-03 — UDP seguro

- reutilizar Event Bus/UDP da SPEC-005;
- emitir `wind` start/update/stop e `haptic_profile`;
- preservar “Vibrações reais” desligado por padrão.

**Aceite:** Control ausente não afeta jogo; modo simulado correlaciona logs por `session_id/seq`.

### E06-04 — Perfil e mapper Boat Demo

- carregar/validar JSON e implementar níveis 0/2/1;
- exibir níveis e perfil no Control;
- testar varredura `-90° → 0° → +90°`.

**Aceite:** curva contínua nas três zonas; ausência de zona é explícita.

### E06-05 — Stream RTP virtual e firmware

- implementar comando de stream, TTL, máscara, prioridades e testes virtuais;
- validar stop, watchdog, NACK, emergência e 10 min de updates sem fila crescente.

**Aceite:** nenhum motor/stream fica ativo após TTL ou emergência.

### E06-06 — Gelo e impacto

- criar icebergs, colisão com debounce e `ice_collision`;
- aplicar preempção e retorno suave do vento;
- adicionar roteiro que garante uma colisão.

**Aceite:** uma colisão = um impacto forte/direcional, sem pendências.

### E06-07 — QA, performance e `.exe`

- testes Godot headless, pytest e ponta a ponta com `VirtualExusTransport`;
- revisar captura/vídeo curto em 720p e 1080p: ondas, brilho, reflexo falso,
  espuma, contraste do barco e legibilidade dos icebergs;
- medir FPS/RAM/carregamento no PC fraco de referência, nos presets Alto,
  Médio e Baixo;
- exportar e testar ZIP no Windows sem Godot/Python; atualizar README iniciante.

**Aceite:** jogo e Control empacotados concluem o roteiro em modo simulado,
com aparência aprovada e meta de desempenho comprovada.

### E06-08 — Validação física controlada

- identificar 0, 1 e 2 com pulsos mínimos em bancada;
- validar vento, transições, gelo, stop, desconexão e emergência;
- registrar versões de perfil, firmware, Control e jogo.

**Aceite:** calibração ocorre pelo JSON versionado; todas as paradas funcionam.

## 9. Critérios finais

- [ ] Build Windows leve roda em Compatibility no PC fraco de referência.
- [ ] Mar possui ondas, brilho, reflexo estilizado e espuma sem técnicas 3D caras.
- [ ] Presets Alto/Médio/Baixo preservam a jogabilidade e o contrato háptico.
- [ ] Sem Control/BLE, o jogo continua jogável.
- [ ] Vento usa stream com start/update/stop, TTL e sem UDP por frame.
- [ ] Rumo/vento propagam suavemente por 0 → 2 → 1 e inverso.
- [ ] Gelo gera impacto prioritário, curto, direcional e com debounce.
- [ ] Perfil usa apenas zonas lógicas 0, 1 e 2 e calibra cada uma em JSON.
- [ ] Control não conhece I²C; firmware segue autoridade de segurança.
- [ ] Saída real começa desligada e mantém dupla autorização.
- [ ] Stop, TTL, desconexão e emergência não deixam comandos pendentes.
- [ ] Licenças, README, roteiro e testes em PC fraco estão entregues.

## 10. Fora de escopo

- apagar/migrar a TPS Demo;
- multiplayer, IA, progressão, mundo aberto ou simulação naval realista;
- rede fora de loopback, BLE/Serial no Godot ou ajuste elétrico pelo jogo;
- elevar limites de segurança pelo perfil;
- uso corporal antes dos gates de bancada, desconexão e emergência.

**Conclusão:** a Boat Demo testa com baixo custo gráfico o que importa: campo de vento direcional contínuo nos três atuadores e impacto inequívoco de gelo, sem quebrar o contrato UDP, a dupla autorização ou a segurança final do firmware.

## 11. Ajustes visuais e de navegação — 17/08/2026

- O mar passa a orientar ondas, reflexos frios e rastros de ar pela direção real
  do vento; a antiga faixa amarela deixa de ser o indicador principal.
- O diretor de vento aumenta a variação angular e reduz o atraso da transição,
  mantendo interpolação contínua para o UDP e o campo háptico.
- A vela aceita `0%`: sem área de vela, a velocidade desejada é zero e o barco
  desacelera. O casco permanece único; a vela, extraída do mesmo sprite, é
  recolhida com uma máscara suave conforme a abertura.
- O mapa usa `Camera2D` seguindo o barco, limites maiores e objetos espaçados.
  Ilhas recebem areia, vegetação, pedras e colisão cinemática; não é possível
  atravessá-las e o contato gera o mesmo feedback de impacto conservador.

### Revisão de assets

O barco usa um **casco-base único** e uma camada de vela extraída do mesmo
sprite, mascarada no shader e recolhida suavemente pela abertura da vela. Não
há mais troca de cascos. Ilhas e icebergs são sprites ilustrados; a colisão
continua baseada em formas simples, independentes da arte. O shader do mar foi
reduzido para ondulações longas, lentas e de baixo contraste, mantendo apenas
reflexos discretos na direção do vento.

O diretor de vento usa uma brisa predominante de baixa frequência e uma variação
secundária ainda mais lenta. Assim, a direção fica tempo suficiente para ser
lida no mar, na vela e no feedback háptico; viradas maiores são ocasionais.
