# SPEC-003.5 — Aplicativo visual Windows para controle BLE do Exus

> **Status:** MVP implementado; pendente de validação no PC com o protótipo.
>
> **Depende de:** [SPEC-003](SPEC-003.md), que entrega o firmware BLE e o
> cliente de bancada `python -m exus_control.cli`.
>
> **Objetivo:** substituir os comandos de terminal por um aplicativo visual para
> Windows que qualquer integrante do grupo possa abrir e usar.
>
> **Evolução decidida:** a [SPEC-004](SPEC-004.md) amplia este mesmo aplicativo
> para receber eventos de jogos. Não será criado um segundo executável de ponte.

---

## 1. Decisão

Criar um aplicativo desktop Windows chamado **Exus Control**, em Python,
usando **Tkinter** para a interface e `bleak` para Bluetooth LE. Ele será
empacotado com **PyInstaller** em um `.exe` distribuível.

Quem for usar o app não instala Python, `bleak` ou qualquer dependência: baixa
o pacote gerado e abre `Exus-Control.exe`. A instalação de NimBLE-Arduino
continua necessária apenas no computador que compila e faz upload do firmware
para o ESP32.

Tkinter é a escolha do MVP porque já faz parte do Python, atende bem a poucos
controles de bancada e deixa o executável mais simples. Não usar Flet, Electron
ou uma interface web nesta etapa.

---

## 2. Limite importante: o que o aplicativo resolve

O aplicativo substitui o terminal depois da primeira gravação do firmware. A
USB é necessária para upload e diagnóstico, mas não para liberar o Bluetooth:

1. instalar NimBLE-Arduino e fazer upload do firmware pelo Arduino IDE;
2. alimentar o Exus por USB, bateria ou fonte externa validada;
3. abrir o Exus Control, encontrar e conectar o dispositivo;
4. desconectar e reconectar livremente, inclusive em outro computador.

O cliente não chama o pareamento do sistema operacional. O app não altera o
firmware, não alimenta a placa, não aprova bateria nem substitui os limites de
segurança locais. Como o GATT é aberto, o uso deve ocorrer em ambiente controlado.

---

## 3. Experiência esperada para o grupo

```text
Abrir Exus Control
        ↓
clicar “Procurar protótipos”
        ↓
selecionar “Exus-A1B2C3” e clicar “Conectar”
        ↓
ler o status das zonas
        ↓
testar uma zona com pulso baixo OU clicar “PARAR TUDO”
```

Não haverá campo para comando de texto no modo normal. Um painel “Diagnóstico
avançado” poderá mostrar respostas do dispositivo, mas não deve permitir burlar
limites, enviar bytes arbitrários ou ocultar uma emergência ativa.

### 3.1 Tela única do MVP

| Área | Conteúdo | Regra de segurança |
|---|---|---|
| Conexão | botão **Procurar**, lista de `Exus-<id>`, botão Conectar/Desconectar e indicador de link | desabilitar controles hápticos até `Q` concluir com sucesso |
| Estado | firmware, zonas prontas, emergência, intensidade máxima e mensagens claras | mostrar “NÃO CONECTADO”, “EMERGÊNCIA” e “SEM ZONAS” em destaque |
| Teste por zona | caixas de seleção para cada zona pronta; intensidade, duração e frequência conservadoras + botão “Testar zonas marcadas” | uma ou mais zonas podem vibrar simultaneamente por uma máscara `group`; iniciar em 15%, 500 ms e 10 Hz e nunca permitir valores acima da capacidade relatada |
| Ações globais | **PARAR TUDO** e **EMERGÊNCIA** grandes e sempre visíveis; `Resume` pede confirmação | emergência disponível enquanto houver conexão, sem depender de outro formulário |
| Log | últimas respostas ACK/NACK, horário e erro de conexão | somente leitura; não expor chaves/dados sensíveis |

No MVP, suportar apenas `pulse`, `stop`, `emergency`, `resume`, `Q` e `status`.
Efeitos ROM, grupos e controles de padrão podem ser incluídos quando os testes
individuais estiverem estáveis.

---

## 4. Arquitetura

```text
Tkinter (tela e estados) ── ExusBleClient ── bleak ── BLE/GATT ── ESP32-C3
                                  │                         │
                            ACK/NACK/status          Segurança + scheduler
```

O app não reimplementa regras hápticas. Ele manda exatamente o enquadramento da
SPEC-003 (`@<sequência> <comando>\n`) e espera o ACK/NACK correspondente. O
ESP32 segue sendo a autoridade para intensidade, duração, *cooldown*, orçamento
global, emergência, desconexão e watchdog.

### 4.1 Refatoração prevista do cliente atual

Antes de criar a tela, separar o cliente de terminal em duas camadas:

```text
exus_control/
├── exus_ble_client.py  ← biblioteca assíncrona reutilizável: scan, connect,
│                          info, command, emergency e callbacks
├── exus_ble.py         ← CLI atual; passa a chamar a biblioteca
├── exus_control.py     ← interface Tkinter
├── requirements.txt    ← dependências de execução/desenvolvimento
└── build_windows.ps1   ← gera o executável de distribuição
```

`ExusBleClient` não pode chamar Tkinter diretamente. A UI executa a operação BLE
em tarefa/thread própria e devolve resultados à thread principal por fila. Isso
evita congelar a janela durante scan, conexão, reconexão ou timeout.

---

## 5. Plano de desenvolvimento

### Fase 0 — validar a base BLE

- Usar `python -m exus_control.cli` para completar scan, conexão, `Q`, pulso mínimo,
  emergência e teste de desconexão no protótipo físico.
- Registrar nome anunciado, conexão direta sem diálogo do Windows e
  respostas reais ACK/NACK.

**Gate:** o cliente de terminal funciona com o hardware antes de criar a UI.

**Implementação atual:** cliente de terminal e UI foram desenvolvidos, mas este
gate continua pendente até o primeiro teste no ESP32 físico.

### Fase 1 — extrair a biblioteca BLE

- Criar `exus_ble_client.py` com API assíncrona pequena e testável.
- Fazer a CLI existente usar essa API, preservando seus comandos e saídas.
- Criar testes simulados para framing, sequência, timeout e tradução de NACK.

**Gate:** CLI passa na mesma suíte antes e depois da refatoração.

### Fase 2 — tela de conexão e diagnóstico

- Implementar janela Tkinter, busca de dispositivos, lista de seleção, conexão,
  leitura de `device-info` e `Q`.
- Mostrar estados de carregando, sem dispositivo, conexão recusada, conexão
  perdida e dispositivo sem zona pronta em português simples.
- Bloquear botões hápticos até capabilities serem válidas.

**Gate:** grupo consegue conectar e identificar o protótipo sem terminal.

### Fase 3 — controles hápticos seguros

- Criar caixas de seleção para as zonas `READY`, com valores iniciais 15% / 500
  ms / 10 Hz. Uma seleção múltipla gera `group <máscara> pulse ...`; seleção de
  uma única zona usa o mesmo caminho e continua válida.
- Respeitar intervalos publicados pelo firmware; validar também no app para
  orientar o usuário, sem remover a validação obrigatória do ESP32.
- Implementar `PARAR TUDO`, emergência, confirmação para `resume`, log de
  respostas e parada visual ao desconectar.

**Gate:** pulsar uma zona, interromper tudo e desligar Bluetooth nunca deixam
motor ativo; a tela reflete o estado real após reconectar.

### Fase 4 — empacotamento para Windows

- Adicionar `pyinstaller` às dependências de desenvolvimento ou arquivo
  separado `requirements-dev.txt`.
- Criar `build_windows.ps1`, que executa PyInstaller com `--windowed`, inclui os
  módulos do `bleak`/`bleak-winrt`, nomeia o binário `Exus-Control.exe` e copia
  README resumido/licenças para `dist/`.
- Testar o pacote em um segundo PC Windows sem Python instalado.
- Distribuir a pasta `dist/` zipada; preferir modo `onedir` inicialmente, pois
  torna diagnóstico de DLL/dependência BLE mais simples que `onefile`.

**Gate:** em PC limpo, o amigo abre o app por duplo clique, encontra o ESP32 e
executa o roteiro de teste sem instalar Python nem digitar comando.

### Fase 5 — acabamento opcional

- Ícone, nome/versão visíveis e manual visual de uma página.
- Perfis de teste salvos, grupos e efeitos ROM somente após validação física.
- Tela OTA quando e se a fase OTA da SPEC-003 estiver implementada e validada.

---

## 6. Segurança e comportamento em falha

- O botão **EMERGÊNCIA** envia a característica BLE dedicada, não apenas uma
  linha de comando; a firmware ainda aplica a parada local.
- Sem conexão, todos os botões que acionam motor ficam desabilitados; não há
  fila para “enviar quando reconectar”.
- Timeout, NACK, queda de Bluetooth ou falha de conexão devem ser exibidos e
  não podem ser convertidos automaticamente em repetição de `pulse`/`effect`.
- `Resume` só libera o bloqueio após confirmação explícita do operador; não
  religar nenhum motor automaticamente.
- O aplicativo não oferece modo de desenvolvedor para exceder limites da placa.
- A primeira conexão, qualquer teste sem USB e qualquer mudança de alimentação
  seguem a sequência de segurança do README e da SPEC-003.

---

## 7. Critérios de aceite

- [ ] Usuário sem conhecimento de terminal encontra e conecta um Exus pelo app.
- [ ] O app funciona em Windows 10/11 sem Python instalado no PC final.
- [ ] Todas as zonas prontas são mostradas por ID e estado.
- [ ] Pulso mínimo, parada total, emergência e `resume` funcionam pela UI.
- [ ] O app indica conexão perdida e desabilita controles imediatamente.
- [ ] NACK/timeout aparece em linguagem compreensível no log e na tela.
- [ ] O firmware continua recusando qualquer comando inseguro, mesmo se a UI
  falhar ou for alterada.
- [ ] O pacote foi testado no PC do protótipo e em um segundo PC Windows limpo.

---

## 8. Estimativa e dependências

| Entrega | Esforço estimado |
|---|---:|
| Refatoração cliente + testes | 0,5–1 dia |
| MVP visual de conexão e zonas | 1–2 dias |
| Empacotamento e teste em PC limpo | 0,5–1 dia |
| Polimento e perfis opcionais | 1–2 dias |
| **MVP distribuível** | **2–4 dias** |

O prazo pressupõe que a SPEC-003 já tenha sido validada no hardware. Sem placa
disponível, a UI pode ser construída, mas só a validação no PC do protótipo
confirma permissões Bluetooth e comportamento de desconexão.

## Referências

- [SPEC-003 — transporte Bluetooth LE](SPEC-003.md)
- [PyInstaller — documentação](https://pyinstaller.org/en/stable/index.html)
- [Python `tkinter` — documentação](https://docs.python.org/3/library/tkinter.html)
