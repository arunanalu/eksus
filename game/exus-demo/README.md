# Exus Demo — Corredor de Imersão

Uma adaptação do TPS Demo oficial do Godot. Você explora o cenário, mira, atira
e enfrenta robôs; acontecimentos do jogo também geram eventos hápticos para o
Exus Control.

## Jogar

Para jogar **não é necessário abrir o Godot nem o Exus Control**:

1. Extraia a distribuição `Exus-Demo-Windows.zip`.
2. Mantenha `Exus-Demo.exe` e `Exus-Demo.pck` na mesma pasta.
3. Abra `Exus-Demo.exe`.

O Exus Control é opcional: abra-o apenas para receber os eventos, vê-los como
simulação ou enviá-los a hardware compatível. Sem ele, o jogo continua normal e
o painel mostra `CONTROL AUSENTE`.

### Controles

| Ação | Controle |
| --- | --- |
| Andar | WASD ou setas |
| Olhar | Mouse |
| Mirar | Botão direito do mouse |
| Atirar | Botão esquerdo, enquanto mira |
| Pular | Espaço |
| Voltar ao menu | Esc |
| Tela cheia | F11 |
| Mostrar painel Exus | F8 |
| Executar demonstração de eventos | F7 |

## Ver os eventos hápticos

O painel **EXUS CONTROL** fica no canto superior esquerdo; pressione **F8** se
ele estiver oculto. Para uma verificação rápida, deixe **Vibrações reais**
desligado e pressione **F7**. A sequência gera vento, ameaça, explosão e parada
do vento. Os botões do painel permitem disparar cada evento manualmente.

| Ação no jogo | Evento |
| --- | --- |
| Atirar enquanto mira | `weapon_fire` |
| Ser atingido por laser inimigo | `damage` |
| Destruir um robô | `explosion` |
| Ter um robô próximo atrás do personagem | `threat` |
| Entrar, permanecer e sair do volume azul | `wind start/update/stop` |

O vento atualiza no máximo dez vezes por segundo. Ele é interrompido ao sair do
volume, voltar ao menu ou fechar o jogo.

| Estado do painel | Significado |
| --- | --- |
| `CONTROL AUSENTE` | O jogo gerou o evento, mas o Control não está aberto. |
| `SIMULADO` | O Control recebeu e mapeou o evento, sem saída física. |
| `ENVIADO` | O Control autorizou e encaminhou o comando ao transporte. |
| `REJEITADO` | O Control recusou o evento; o motivo aparece no painel. |

Com o Godot aberto, os eventos também aparecem na aba **Output** em linhas
`[EXUS EVENT]`, como JSON do contrato `exus.game-event/1`.

## Usar com Exus Control

1. Em outro terminal, instale e inicie o Control conforme o
   [README do Exus Control](../../exus_control/README.md).
2. Confirme que a ponte local está em `127.0.0.1:4242`.
3. Execute a demo com **F7**, mantendo **Vibrações reais** desligado.
4. O painel deve mostrar `SIMULADO` e o log do Control, `WOULD_SEND`.

Marcar **Vibrações reais** não aciona hardware sozinho: o Exus Control ainda
precisa estar autorizado e ter um transporte conectado. O jogo não acessa BLE,
Serial, ESP32 ou motores diretamente; ele envia eventos UDP apenas para a ponte
local.

## Abrir ou alterar no Godot

Use o **Godot 4.5.2 Standard** (não a edição .NET/C#). Importe
`project.godot`, aguarde a importação inicial dos assets e pressione **F5**.

Para gerar a versão jogável no Windows, a partir da raiz do repositório, execute:

```powershell
powershell -ExecutionPolicy Bypass -File game\exus-demo\tools\build_windows.ps1
```

O script produz `build\windows\Exus-Demo.exe` e
`build\Exus-Demo-Windows.zip`.

## Testes de contrato

Com o executável do Godot no `PATH`:

```powershell
godot --headless --path game\exus-demo --script res://exus/tests/exus_event_test.gd
```

Com a ponte do Exus Control aberta em modo simulado:

```powershell
godot --headless --path game\exus-demo --script res://exus/tests/exus_udp_roundtrip_test.gd
```

## Base e licença

- Godot: **4.5.2 Standard**.
- TPS Demo: `godotengine/tps-demo` no commit
  `90f2e38d7b5cf9e6fd0b788d0da1df4b84d49269`.
- Licenças e origem dos assets: [LICENSES/](LICENSES/).
