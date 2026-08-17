# SPEC-005 — Demo Godot integrada ao Exus Control

> **Status:** plano técnico pronto para implementação incremental.
>
> **Depende de:** [SPEC-004](SPEC-004.md), entregáveis E04-03 e E04-04 para o
> contrato de eventos e o servidor local em modo simulado.
>
> **Objetivo:** adaptar uma demo oficial do Godot para emitir eventos hápticos,
> funcionar sem hardware e, posteriormente, participar do teste ponta a ponta
> com o protótipo conectado pelo Exus Control.
>
> **Público do plano:** pessoa iniciante em desenvolvimento de jogos. Cada
> entregável deve incluir instruções de abrir, executar e verificar o resultado.

---

## 1. Decisões

- Engine: **Godot Standard**, sem C#/.NET.
- Base: **Third Person Shooter Demo 4.5-90f2e38**, licença MIT.
- Engine fixada para essa base: **Godot 4.5.2 Standard**.
- Linguagem: GDScript.
- Comunicação: `PacketPeerUDP` para `127.0.0.1:4242`.
- Dependência de hardware: nenhuma em qualquer entregável desta SPEC.
- Nome de trabalho: **Exus Demo — Corredor de Imersão**.

Embora existam versões mais novas do Godot, o MVP fixa a versão correspondente
ao asset oficial escolhido. Atualização de engine é uma entrega separada, nunca
uma mudança automática no meio do desenvolvimento.

Fontes fixadas:

- [TPS Demo 4.5-90f2e38 na Asset Library](https://godotengine.org/asset-library/asset/2710)
- [repositório oficial do TPS Demo](https://github.com/godotengine/tps-demo)
- [Godot 4.5.2 no arquivo oficial](https://godotengine.org/download/archive/4.5.2-stable/)

---

## 2. Experiência mínima

A demo não precisa virar um jogo completo. Uma sessão curta deve provar:

1. personagem anda, mira e atira usando a base TPS;
2. disparo gera `weapon_fire`;
3. dano com origem gera `damage` e azimute;
4. uma área do cenário gera `wind start/update/stop`;
5. explosão gera `explosion`;
6. ameaça atrás do jogador gera `threat`;
7. painel mostra evento enviado e resposta do Exus Control;
8. ausência do Control ou do protótipo não interrompe o jogo.

Quatro desses eventos bastam para o aceite inicial; os demais podem ser
adicionados depois sem alterar o contrato.

---

## 3. Modos e opção de vibração

O menu da demo terá **Vibrações reais**, desligado por padrão.

O jogo sempre pode construir e registrar o evento. A opção altera apenas
`output_requested`:

| Jogo | Exus Control | Resultado |
|---|---|---|
| desligado | desligado | evento e comando aparecem como simulação |
| ligado | desligado | simulação; Control informa que falta autorização local |
| desligado | ligado | simulação; jogo não solicitou saída real |
| ligado | ligado + BLE pronto | Control pode enviar ao firmware |

Logo, o usuário consegue desenvolver e demonstrar o pipeline completo sem o
protótipo. Ativar no jogo sozinho nunca vibra o hardware.

Configurações iniciais do jogo:

- `exus/integration_enabled = true`;
- `exus/real_vibration_requested = false`;
- `exus/host = 127.0.0.1`;
- `exus/port = 4242`;
- `exus/show_debug_overlay = true` apenas em builds de desenvolvimento.

---

## 4. Componentes dentro do Godot

```text
game/exus-demo/
├── project.godot
├── LICENSES/
│   ├── TPS-DEMO-LICENSE.md
│   └── ASSET-SOURCES.md
└── exus/
    ├── exus_event_bus.gd        # API única usada pelas mecânicas
    ├── exus_udp_client.gd       # serializa, envia e recebe resultado
    ├── exus_event.gd            # valida/constrói evento canônico
    ├── exus_settings.gd         # opções e padrão seguro
    ├── exus_debug_overlay.gd    # estado e últimos eventos
    └── tests/
        └── exus_event_test.gd
```

`ExusEventBus` será Autoload e oferecerá funções pequenas:

```gdscript
ExusEventBus.oneshot("damage", magnitude, duration_ms, azimuth, "projectile")
ExusEventBus.start_stream("wind", stream_id, magnitude, azimuth)
ExusEventBus.update_stream("wind", stream_id, magnitude, azimuth)
ExusEventBus.stop_stream("wind", stream_id)
```

As cenas do TPS não montam JSON nem chamam UDP diretamente. Elas apenas chamam
o Event Bus. Isso reduz o risco para quem ainda está aprendendo Godot.

---

## 5. Cálculo direcional

Quando houver posição da origem:

1. calcular `origem - posição_do_jogador`;
2. zerar o eixo vertical para obter a direção horizontal;
3. converter o vetor para o espaço local do personagem/câmera;
4. calcular o azimute com `atan2`;
5. normalizar para `-180..180`;
6. enviar o valor contínuo; a quantização em zonas pertence ao Exus Control.

Se a origem não for conhecida, omitir direção ou usar `null`. Não inventar
esquerda/direita a partir de dados incompletos.

---

## 6. Estado contínuo e falhas

- vento envia `start`, atualizações em frequência limitada e `stop` ao sair;
- todo stream possui ID único e TTL efetivo no Control;
- sair da cena, morrer, pausar ou fechar o jogo envia `stop` dos streams ativos;
- não reenviar impactos por falta de resposta UDP;
- erro de socket vira log/indicador, não erro fatal de gameplay;
- o overlay distingue `CONTROL AUSENTE`, `SIMULADO`, `ENVIADO` e `REJEITADO`;
- não usar UDP a cada frame; estados contínuos começam com meta de 10 Hz e serão
  ajustados por medição.

---

## 7. Plano em pequenos entregáveis

### E05-01 — Bootstrap reproduzível da demo

- obter o asset oficial fixado e registrar checksum/commit;
- abrir no Godot 4.5.2 Standard e confirmar execução sem alterações;
- criar `game/exus-demo/README.md` com passos visuais para iniciante;
- registrar licença do TPS e de assets adicionais;
- criar uma tag/commit de baseline antes das mudanças.

**Aceite:** em clone limpo, uma pessoa abre `project.godot`, aguarda a primeira
importação e inicia a demo seguindo o README.

### E05-02 — Event Bus sem rede

- criar `ExusEvent`, configurações e `ExusEventBus` Autoload;
- adicionar opção **Vibrações reais**, desligada por padrão;
- gerar eventos apenas no console Godot;
- criar uma cena de teste com botões para os cinco eventos.

**Aceite:** console mostra JSON válido, sequência crescente e
`output_requested=false`; nenhum socket ou hardware é necessário.

### E05-03 — UDP e resposta do Control

- criar `ExusUdpClient` com `PacketPeerUDP`;
- enviar ao loopback e ler `exus.bridge-result/1` sem bloquear frames;
- adicionar timeout apenas para estado visual;
- integrar com E04-04 usando `MockTransport`.

**Aceite:** a cena de teste mostra quatro respostas `simulated` e o Control
registra quatro `WOULD_SEND`.

### E05-04 — Overlay e experiência de configuração

- mostrar estado do Control, modo real solicitado, último evento e resultado;
- adicionar menu acessível para integração e vibrações reais;
- incluir explicação curta de que o Control também precisa autorizar hardware;
- manter a opção real desligada em toda instalação nova.

**Aceite:** um iniciante identifica, pela tela, se está sem Control, simulando ou
solicitando hardware.

### E05-05 — Instrumentação das mecânicas existentes

- integrar primeiro `weapon_fire` e `damage` nos callbacks reais do TPS;
- calcular azimute do dano quando houver origem;
- adicionar logs locais e teste manual determinístico;
- não alterar regras de movimentação/combate além do necessário.

**Aceite:** atirar e receber dano geram exatamente um evento cada, sem duplicação
por frame ou por animação.

### E05-06 — Corredor de demonstração

- criar uma cena curta reutilizando os assets existentes;
- adicionar volume de vento com start/update/stop;
- adicionar explosão controlada e ameaça traseira roteirizada;
- limitar densidade de estímulos e incluir tutorial de 30–60 segundos;
- manter caminho de saída/pausa sempre disponível.

**Aceite:** uma sessão reproduzível dispara ao menos quatro eventos na ordem
planejada e termina em 4–6 minutos.

### E05-07 — QA sem hardware

- executar a sessão inteira com saída real desligada;
- comparar log Godot, respostas UDP e log do Control por `session_id/seq`;
- testar Control fechado, aberto depois do jogo, reiniciado e porta incorreta;
- testar spam, pausa, morte e troca de cena com streams ativos.

**Aceite:** gameplay nunca bloqueia; todos os streams terminam; nenhum evento
simulado fica guardado para execução posterior.

### E05-08 — Ponta a ponta virtual e candidato congelado

- executar o jogo exportado com o Exus Control empacotado;
- usar o `VirtualExusTransport` da SPEC-004;
- habilitar as duas autorizações e comprovar que o transporte virtual recebe os
  comandos que o BLE receberia;
- testar quatro eventos, `stop`, emergência, NACK, timeout e perda de conexão;
- validar capabilities de uma e várias zonas;
- congelar versão da engine, build do jogo, schema e roteiro de demonstração.

**Aceite:** logs rastreiam cada evento do Godot até o ACK/NACK virtual e toda a
demo é considerada pronta para validação física, ainda sem protótipo.

---

## 8. Relação com a validação física posterior

O teste com o protótipo não é entregável de desenvolvimento da demo. Ele segue
a Seção 10 da SPEC-004 depois de todo o jogo estar aceito em modo virtual.

Se o primeiro teste físico falhar, preservar o candidato e abrir a branch
`fix/hardware-integration-<sintoma>`. A correção deve ocorrer primeiro no
firmware ou adapter BLE, salvo evidência de erro no contrato do jogo. Todo defeito
real descoberto deve virar fixture automatizada antes do novo teste físico.

## 9. Guia de trabalho para iniciante em Godot

Cada PR/entrega da demo deve informar:

1. qual versão do Godot abrir;
2. qual arquivo `project.godot` importar;
3. qual cena executar com F6 ou qual projeto executar com F5;
4. qual ação realizar no jogo;
5. qual texto observar no overlay, console e Exus Control;
6. como voltar ao estado anterior pelo Git;
7. captura curta ou GIF do resultado esperado quando houver mudança visual.

Evitar instruções como “conecte o signal adequado” sem indicar nó, signal,
arquivo e método. Mudanças nas cenas devem ser pequenas e acompanhadas por uma
explicação de onde o evento original do TPS nasce.

---

## 10. Critérios de aceite da SPEC-005

- [ ] Base e engine estão fixadas e suas licenças registradas.
- [ ] Opção de vibrações reais começa desligada.
- [ ] Com a opção desligada, eventos completos continuam aparecendo nos logs.
- [ ] Event Bus é a única API usada pelas mecânicas para integração Exus.
- [ ] Jogo continua funcionando sem Exus Control e sem protótipo.
- [ ] Control responde em modo simulado e o overlay mostra o estado correto.
- [ ] Ao menos quatro eventos têm origem determinística na demo.
- [ ] Eventos contínuos sempre terminam por stop ou TTL.
- [ ] Não existe acesso BLE/Serial direto dentro do Godot.
- [ ] O README permite que uma pessoa iniciante rode e verifique cada entrega.
- [ ] Todos os entregáveis são concluídos e aceitos sem protótipo.
- [ ] A arquitetura permite trocar o transporte virtual pelo BLE real sem mudar
  as mecânicas; essa hipótese será confirmada somente na validação posterior.

---

## 11. Fora de escopo

- criar arte ou jogo completo do zero;
- atualizar automaticamente para a última versão do Godot;
- incluir BLE, `bleak`, Serial ou drivers nativos no projeto Godot;
- jogo comercial, mod, captura de tela, áudio ou anti-cheat;
- salvar a opção de saída real ligada por padrão;
- uso corporal antes da validação formal do protótipo.

---

**Conclusão:** a demo será desenvolvida primeiro como produtora independente de
eventos verificáveis. O Exus Control simula o último trecho e registra o comando
que enviaria. Quando BLE estiver estável, a integração física exigirá habilitar
as duas autorizações e trocar somente o transporte do Control, sem reescrever as
mecânicas da demo.
