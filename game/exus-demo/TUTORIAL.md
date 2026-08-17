# Tutorial - Exus Demo: Corredor de Imersao

Este guia executa a demo sem ESP32, Bluetooth ou Exus Control. Ao final, você
terá jogado o TPS e visto os eventos hápticos sendo produzidos em modo seguro.

## Jogar sem instalar Godot

Se você recebeu a pasta `build\\windows` ou o ZIP da distribuição, basta extrair
todo o conteúdo para uma pasta local e abrir **Exus-Demo.exe**. Não mova o arquivo
`.pck` para outra pasta: ele contém os recursos do jogo.

As instruções a seguir são apenas para quem quer abrir ou modificar o projeto no
editor Godot.

## 1. Instalar o Godot correto

1. Baixe o **Godot 4.5.2 Standard** para Windows.
2. Extraia o ZIP em uma pasta de sua preferência.
3. Abra `Godot_v4.5.2-stable_win64.exe`.

Não use a versão .NET/Mono para este projeto.

## 2. Importar o jogo

1. Na tela inicial do Godot, clique em **Importar**.
2. Abra a pasta `future-makers-26\game\exus-demo`.
3. Selecione `project.godot` e confirme.
4. Espere a importação inicial terminar. Os assets 3D são grandes e esta etapa
   pode levar alguns minutos.
5. Com o projeto selecionado, clique em **Editar**.

## 3. Jogar

1. No editor Godot, pressione **F5**.
2. No menu do jogo, clique em **Play**.
3. Use os controles abaixo:

| Ação | Controle |
| --- | --- |
| Andar | WASD ou setas |
| Olhar | Mouse |
| Mirar | Botão direito do mouse |
| Atirar | Botão esquerdo, enquanto mira |
| Pular | Espaço |
| Voltar ao menu | Esc |
| Tela cheia | F11 |

## 4. Verificar a integração Exus sem hardware

O painel **EXUS CONTROL** aparece no canto superior esquerdo. Se estiver oculto,
pressione **F8**.

1. Confirme que **Vibrações reais** está desmarcado.
2. Pressione **F7**.
3. O painel deve mostrar eventos de vento, ameaça, explosão e parada do vento.
4. Sem o Exus Control aberto, o estado fica `CONTROL AUSENTE`; isto é esperado
   e o jogo continua jogável.
5. Abra a aba **Output** do Godot e procure linhas começando por `[EXUS EVENT]`.
   Cada linha é um JSON válido do contrato `exus.game-event/1`.

Também é possível usar os botões do painel para emitir cada evento manualmente.

## 5. Eventos durante a partida

| No jogo | Evento Exus esperado |
| --- | --- |
| Atirar | `weapon_fire` |
| Ser atingido por laser inimigo | `damage`, com azimute da origem |
| Destruir robô | `explosion` |
| Robô próximo atrás do personagem | `threat` |
| Entrar/sair do volume azul `CORREDOR DE VENTO` | `wind start/update/stop` |

O vento atualiza no máximo 10 vezes por segundo. Ao sair do volume, voltar ao
menu ou fechar o jogo, streams ativos enviam `stop`.

## 6. Testar com o Exus Control em simulação (opcional)

1. Em outro terminal, na raiz do repositório, inicie o Exus Control e a ponte
   local em `127.0.0.1:4242`.
2. Volte ao jogo e mantenha **Vibrações reais** desligado.
3. Pressione F7 novamente.
4. O painel deve mostrar `SIMULADO` e o Control deve registrar `WOULD_SEND`.

Não marque **Vibrações reais** durante os testes sem hardware. Mesmo marcada,
a opção não aciona nada sozinha: o Exus Control precisa autorizar a saída e ter
um transporte conectado.

## Problemas comuns

- **Tela preta ou FPS baixo:** aguarde a importação terminar e reduza a qualidade
  gráfica no menu **Settings**.
- **Painel mostra CONTROL AUSENTE:** normal sem o Exus Control aberto; o gameplay
  não depende dele.
- **Não vejo JSON no console:** pressione F7 com o painel aberto e confira a aba
  Output, não a aba Debugger.
- **O Godot pede para atualizar o projeto:** cancele. A versão fixada para esta
  demo é 4.5.2 Standard.
