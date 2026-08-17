# Exus Demo - Corredor de Imersao

Uma adaptação do TPS Demo oficial que produz eventos hápticos para o Exus Control.
Ela funciona integralmente sem ESP32, Bluetooth, Serial ou qualquer outro hardware.

Para o guia passo a passo de primeira execução, consulte [TUTORIAL.md](TUTORIAL.md).
Para entender os gatilhos hápticos e a validação visual, consulte
[COMO-FUNCIONA.md](COMO-FUNCIONA.md).

Para jogar sem instalar Godot, abra `build/windows/Exus-Demo.exe` depois de uma
build de distribuição. O pacote da build deve sempre manter o `.exe` e o `.pck`
na mesma pasta.

## Base fixa

- Godot: **4.5.2 Standard** (não use a edição .NET/C# para esta entrega).
- TPS Demo: `godotengine/tps-demo` no commit
  `90f2e38d7b5cf9e6fd0b788d0da1df4b84d49269` (tag `4.5-90f2e38`).
- Licença e origem: [LICENSES/](LICENSES/).

## Abrir e executar

1. Instale ou abra o **Godot 4.5.2 Standard**.
2. No gerenciador de projetos, clique em **Importar** e selecione
   `game/exus-demo/project.godot`.
3. Aguarde a primeira importação dos assets. É normal demorar alguns minutos.
4. Pressione **F6** para a cena atualmente aberta ou **F5** para executar a demo.
5. No menu, escolha **Play**. Use WASD para andar, botão direito para mirar e
   botão esquerdo para atirar enquanto mira.

Para voltar ao estado antes de qualquer mudança local: use `git status`, copie o
caminho listado e execute `git restore -- <arquivo>`. Não use esse comando para
descartar mudanças de outras pessoas.

## Painel Exus e roteiro de verificação

O painel aparece no canto superior esquerdo; **F8** o oculta/mostra. Ele informa
o último evento, resultado e um destes estados: `CONTROL AUSENTE`, `SIMULADO`,
`ENVIADO` ou `REJEITADO`.

- A opção **Vibrações reais** começa desligada em uma instalação nova. Marcá-la
  apenas define `output_requested=true`; o Exus Control ainda precisa estar
  conectado e autorizado para qualquer saída física.
- Os botões do painel geram os cinco eventos para teste manual.
- **F7** executa um roteiro curto e determinístico: vento start/update, ameaça,
  explosão e vento stop. Observe a sequência crescente e os JSONs `[EXUS EVENT]`
  no console do Godot.
- Ao avançar pelo volume azul `CORREDOR DE VENTO`, a demo envia `wind start`,
  atualiza no máximo 10 vezes por segundo e envia `wind stop` ao sair.
- Atirar emite exatamente um `weapon_fire`. Um laser de robô que atinge o jogador
  emite `damage` com o azimute da origem; destruir um robô emite `explosion`; um
  robô próximo atrás do jogador emite `threat` com cooldown de cinco segundos.

## Usar com Exus Control (somente simulação)

1. Em outro terminal, instale e inicie o Control conforme
   [`../../exus_control/README.md`](../../exus_control/README.md).
2. Confirme que a ponte está em `127.0.0.1:4242`; não exponha essa porta na rede.
3. Deixe **Vibrações reais** desligado e execute o roteiro com F7.
4. O painel deve mostrar `SIMULADO` e os logs do Control devem registrar
   `WOULD_SEND`, sem depender de hardware.

Se o Control não estiver aberto, o jogo continua normalmente e após 750 ms o
painel mostra `CONTROL AUSENTE`. Eventos de impacto não são reenviados. Streams
ativos são interrompidos ao sair do volume, trocar de cena ou fechar a janela.

## Teste rápido de contrato

No PowerShell, com o executável do Godot 4.5.2 no PATH:

```powershell
godot --headless --path game\exus-demo --script res://exus/tests/exus_event_test.gd
```

Saída esperada: `ExusEvent tests passed`.

Com a ponte do Exus Control iniciada em modo simulado, valide também o retorno
UDP com:

```powershell
godot --headless --path game\exus-demo --script res://exus/tests/exus_udp_roundtrip_test.gd
```

Saída esperada: `Exus UDP round-trip passed`.

## Limites desta entrega

O projeto Godot não importa `bleak`, Serial, driver nativo ou código BLE. Ele
apenas cria o schema `exus.game-event/1` e conversa por UDP loopback com o Exus
Control. O teste físico fica fora desta demo até que a SPEC-004 esteja aceita em
modo virtual.
