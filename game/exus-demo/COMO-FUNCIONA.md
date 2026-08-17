# Como funciona a Exus Demo

## O que é o jogo

**Exus Demo - Corredor de Imersão** é uma adaptação da demo oficial de tiro em
terceira pessoa do Godot. O personagem explora o cenário, mira, atira e enfrenta
robôs. A camada Exus observa acontecimentos específicos do jogo e cria eventos
hápticos independentes do hardware.

O jogo não conversa com Bluetooth, Serial, ESP32 ou motores. Ele envia somente
eventos UDP para `127.0.0.1:4242`, onde o Exus Control decide se vai simular ou
encaminhar o comando ao transporte.

## Gatilhos hápticos

| Ação no jogo | Evento enviado | Como testar |
| --- | --- | --- |
| Atirar enquanto mira | `weapon_fire` | Mire com botão direito e atire com botão esquerdo. |
| Ser atingido pelo laser de um robô | `damage` | Deixe um robô enxergar e acertar o personagem. O azimute vem da posição real do inimigo. |
| Destruir um robô | `explosion` | Acerte o mesmo robô até ele explodir. |
| Ter um robô próximo atrás do personagem | `threat` | Passe por um robô e deixe-o atrás; há cooldown de cinco segundos. |
| Entrar no volume azul `CORREDOR DE VENTO` | `wind start` | Atravesse o volume azul no cenário. |
| Permanecer no corredor | `wind update` | Atualiza no máximo 10 vezes por segundo. |
| Sair do corredor, voltar ao menu ou fechar o jogo | `wind stop` | Saia do volume ou pressione Esc. |

Todos os eventos passam pela mesma API, `ExusEventBus`. Assim, as mecânicas do
jogo não conhecem formato JSON, UDP, BLE ou detalhes elétricos do protótipo.

## Como saber se funcionou

Pressione **F8** para mostrar ou ocultar o painel **EXUS CONTROL**. Ele fica no
canto superior esquerdo e mostra o último evento, a sequência e o resultado da
entrega.

| Estado do painel | Significado |
| --- | --- |
| `CONTROL AUSENTE` | O jogo criou o evento, mas não recebeu resposta do Control. É normal se o Control estiver fechado. |
| `SIMULADO` | O Control recebeu e mapeou o evento, mas não enviará saída física. Este é o modo padrão e seguro. |
| `ENVIADO` | O Control está autorizado e encaminhou o comando ao transporte conectado. |
| `REJEITADO` | O Control recusou o evento; o painel mostra o motivo. |

O painel também indica se o jogo solicitou saída real. Essa opção começa
desligada. Mesmo marcada, ela **não** aciona hardware sozinha: o operador ainda
precisa autorizar a saída no Exus Control e ter um transporte conectado.

## Roteiro rápido de validação

1. Abra o jogo e pressione **F8** se o painel não estiver visível.
2. Confirme que **Vibrações reais** está desligado.
3. Pressione **F7**.
4. Confira no painel a sequência: vento, atualização de vento, ameaça, explosão
   e parada do vento.
5. Sem Exus Control aberto, o estado termina como `CONTROL AUSENTE`, sem afetar
   o gameplay.
6. Com Exus Control aberto e a ponte iniciada, o estado deve ser `SIMULADO` e o
   Control deve registrar `WOULD_SEND` no log da sessão.

Os botões no painel também permitem disparar manualmente `weapon_fire`, `damage`,
`explosion`, `threat` e o stream de `wind`, sem precisar alcançar cada situação
no cenário.
