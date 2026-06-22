# PLAN.md — Código para ESP32 (Projeto Exus)

> **Objetivo deste documento:** guiar, passo a passo, a criação do código (firmware) para o microcontrolador **ESP32-C3** que controla **um** driver háptico **DRV2605** ligado a **um** motor **LRA do tipo moeda**, fazendo-o vibrar com uma **frequência percebida** definida (ritmo de pulsos).
>
> **Para quem é este plano:** pessoas **leigas em eletrônica**. Por isso, cada etapa explica *o que fazer*, *por que fazer* e *onde se pode errar (precauções)*. Não é preciso entender eletrônica avançada para seguir — basta ir na ordem.
>
> Este documento é **um plano**, não o código final. Ele descreve *o que* construir e *em que ordem*. O código em si será escrito seguindo estas etapas.

---

## 0. Resumo em uma frase

Vamos escrever um programa em **Arduino (C++)** que roda no **ESP32-C3**, conversa com o chip **DRV2605** pelo protocolo **I2C** (dois fios de dados) e manda esse chip vibrar o **motor LRA moeda** em **padrões de pulso** que criam a sensação de uma frequência escolhida.

---

## 1. Conceitos básicos (leia antes de tudo)

Estes 6 conceitos evitam 90% da confusão mais à frente.

1. **ESP32-C3** — é o "cérebro": um microcontrolador (um computadorzinho num chip). Ele roda o nosso código e dá as ordens.
2. **DRV2605** — é o "nervo motor": um chip especializado em controlar motores de vibração. Ele recebe ordens do ESP32 e gera o sinal elétrico certo para o motor. **Não ligamos o motor direto no ESP32** — sempre passamos pelo DRV2605.
3. **Motor LRA moeda** — é o "músculo": a pecinha redonda que de fato vibra e encosta na pele.
4. **I2C** — é o "idioma" pelo qual o ESP32 fala com o DRV2605. Usa só 2 fios: **SDA** (dados) e **SCL** (relógio/sincronia). Cada chip no I2C tem um **endereço**; o do DRV2605 é **`0x5A`**.
5. **Biblioteca** — código pronto que outra pessoa escreveu para facilitar nossa vida. Vamos usar a **`Adafruit_DRV2605`**, que já sabe conversar com o chip; nós só chamamos funções simples.
6. **Frequência de um LRA (ponto MAIS importante de entender):**
   - Um motor LRA **vibra sempre na frequência de ressonância mecânica dele** (normalmente algo perto de **170–235 Hz**, definido de fábrica pela construção física do motor). **Não dá para escolher livremente** essa frequência como se fosse um alto-falante.
   - Quando o projeto fala em "vibrar a 10 Hz, 30 Hz, 60 Hz", isso é a **frequência percebida**: a sensação de ritmo que criamos **ligando e desligando** o motor rapidamente (pulsos). O motor continua vibrando na ressonância dele; nós só controlamos o *envelope* (quando liga, quanto tempo fica ligado, quanto tempo fica desligado, com qual intensidade).
   - **É exatamente assim que vamos implementar a "frequência" neste projeto.** (Decisão já tomada com o time.)

> 📌 **Precaução conceitual:** se alguém esperar que o código "mude a frequência física do motor" para qualquer valor, vai se frustrar. Deixe claro para todos que o que controlamos é o **padrão de pulsos (envelope)**, não a frequência de ressonância.

---

## 2. Escopo deste plano (o que entra e o que NÃO entra agora)

**ENTRA no MVP (esta etapa):**
- 1× ESP32-C3
- 1× DRV2605 (ligado **direto** no I2C do ESP32, sem multiplexador)
- 1× motor LRA moeda
- Código que: inicializa tudo, calibra o motor, e faz o motor vibrar em padrões de pulso com frequência percebida e intensidade controláveis.
- Comandos simples pela **USB Serial** (digitar comandos no computador para testar).

**NÃO entra agora** (ver Seção 10 — Evolução):
- Multiplexador TCA9548A
- Múltiplos motores/zonas
- Controle de frequência por *grupo* de motores
- Comunicação BLE / Wi-Fi / integração com jogo

> Manter o MVP pequeno é proposital: validar **um** motor de forma segura e confiável antes de multiplicar. Quase todo problema de hardware aparece já com 1 motor — é mais barato descobrir agora.

---

## 3. Lista de materiais (hardware)

| Item | Para quê | Observação |
|------|----------|------------|
| Placa ESP32-C3 (dev board) | Cérebro / roda o código | Qualquer dev board ESP32-C3 com USB serve |
| Módulo DRV2605 / DRV2605L (breakout) | Controla o motor | O da **Adafruit** já vem com resistores de I2C e facilita muito |
| Motor LRA moeda | Vibra | **LRA**, não ERM. Anote a **tensão e a frequência de ressonância** do datasheet do seu motor |
| Cabo USB | Programar e alimentar | Deve ser cabo de **dados**, não só de carga |
| Protoboard + jumpers | Ligações de teste | Só para a fase de teste (ver precaução abaixo) |
| (Opcional) Multímetro | Medir tensão/corrente | Muito útil para depurar |

> ⚠️ **Precaução — DRV2605 x DRV2605L:** o link de referência da `espp` fala em "DRV2605"; o módulo mais comum no mercado (Adafruit) é o "DRV2605**L**". Para o nosso uso em **modo LRA** eles se comportam igual e a mesma biblioteca atende. Apenas confirme no seu módulo qual você tem e que ele suporta **LRA**.

> ⚠️ **Precaução — motor LRA correto:** existem motores **ERM** (massa excêntrica) e **LRA** (ressonante). O código vai configurar o chip em **modo LRA**. Se o motor for ERM, ele não vai funcionar bem. Confirme no datasheet/descrição que é **LRA**.

---

## 4. Montagem física (ligações)

> Faça toda a montagem com tudo **desligado** (sem USB conectado). Só conecte a energia depois de conferir.

Ligações entre ESP32-C3 e módulo DRV2605 (Adafruit):

| ESP32-C3 | DRV2605 | O que é |
|----------|---------|---------|
| `3V3` | `VIN` | Alimentação 3,3 V |
| `GND` | `GND` | Terra (referência comum) — **obrigatório** |
| `GPIO8` (SDA) | `SDA` | Dado do I2C |
| `GPIO9` (SCL) | `SCL` | Relógio do I2C |
| (saída do DRV) | `OUT+` / `OUT-` → motor LRA | Sinal que vibra o motor |

- **Pinos I2C padrão do ESP32-C3:** `SDA = GPIO8`, `SCL = GPIO9` (confirmado para Arduino-ESP32). Se a sua placa usar outros, dá para definir no código com `Wire.begin(SDA, SCL)`.
- O **motor LRA** se liga nos terminais `OUT+` e `OUT-` do DRV2605 (no módulo Adafruit há um conector/terminal para isso).

> ⚠️ **Precauções de montagem:**
> - **GND comum é sagrado:** ESP32 e DRV2605 precisam compartilhar o mesmo `GND`. Sem isso, o I2C não funciona ou trava.
> - **Confira a tensão:** o módulo Adafruit aceita 3,3 V no `VIN`. Não ligue 5 V nos pinos de I2C do ESP32-C3 sem saber — os pinos do ESP32 são de **3,3 V**.
> - **Protoboard só para teste:** a vibração do motor **solta jumpers** com o tempo. A protoboard serve para validar; para algo usável, evoluir para placa soldada/PCB (ver Seção 10). Fixe bem o motor a uma superfície durante os testes para sentir a vibração de verdade.
> - **Nunca ligue o motor direto no ESP32:** sempre pelo DRV2605. Um motor ligado direto pode danificar o pino do microcontrolador.

---

## 5. Preparação do ambiente de software

Framework escolhido: **Arduino + biblioteca Adafruit_DRV2605**.

Passos:
1. Instalar a **Arduino IDE** (versão 2.x) — ou, se preferir uma ferramenta mais robusta, **PlatformIO** (ambos usam o mesmo framework Arduino).
2. Adicionar o **suporte a placas ESP32** (Boards Manager → "esp32" da Espressif).
3. Selecionar a placa correta (ex.: "ESP32C3 Dev Module") e a porta USB.
4. Instalar a biblioteca **`Adafruit_DRV2605`** (Library Manager) — ela já traz a dependência do `Wire` (I2C).

> ⚠️ **Precaução:** ao gravar o código pela primeira vez, algumas placas ESP32-C3 exigem segurar o botão **BOOT** durante o upload. Se o upload falhar, esse costuma ser o motivo.

---

## 6. Estrutura do código (organização em módulos)

Mesmo sendo um MVP de 1 motor, vamos organizar o código pensando na evolução. Sugestão de blocos lógicos dentro do `.ino` (ou em arquivos separados):

1. **Config** — constantes editáveis num só lugar: pinos I2C, endereço do DRV (`0x5A`), parâmetros do motor, limites de segurança.
2. **DriverHaptico** — funções que falam com o DRV2605: iniciar, calibrar, modo LRA, tocar efeito, ligar/desligar em tempo real (RTP), parar.
3. **GeradorDeEnvelope** — a peça que cria a "frequência percebida": liga/desliga o motor no ritmo certo com a intensidade certa.
4. **Seguranca** — aplica limites (intensidade máxima, duração máxima, *cooldown*) e a **parada de emergência**.
5. **Comandos (Serial)** — lê comandos digitados no computador para testar (ex.: definir frequência, intensidade, duração).
6. **(reservado p/ evolução)** GerenciadorI2C/Zonas — vazio por enquanto; entra quando chegar o multiplexador.

> 💡 Separar responsabilidades agora deixa a expansão para 8 zonas muito mais fácil depois.

---

## 7. Lógica de funcionamento (passo a passo do que o código faz)

### 7.1. No `setup()` (roda uma vez, ao ligar)
1. Iniciar a comunicação USB Serial (para vermos mensagens e darmos comandos).
2. Iniciar o I2C: `Wire.begin(8, 9)` (SDA=8, SCL=9) — ou `Wire.begin()` se os pinos padrão já servirem.
3. Iniciar o DRV2605: `drv.begin()`.
4. **Verificar se o chip respondeu** (tratamento de erro): se não respondeu, mostrar mensagem clara no Serial e **não** acionar o motor.
5. Configurar o chip para **modo LRA**: `drv.useLRA()`.
6. **Calibração / auto-ajuste do LRA:** deixar o DRV2605 descobrir a ressonância do motor (auto-calibração) para vibração eficiente. (Detalhe técnico na Seção 8.)
7. Deixar o motor **parado** e aguardar comandos.

### 7.2. No `loop()` (roda continuamente)
1. **Checar parada de emergência** primeiro (botão/comando). Se ativa: parar o motor e não fazer mais nada até liberar.
2. Ler se chegou algum **comando** pela Serial (ex.: "vibrar 30 Hz, 50% de intensidade, por 500 ms").
3. Passar o comando pelo módulo **Seguranca** (cortar o que passar dos limites).
4. Acionar o **GeradorDeEnvelope** com os parâmetros já validados.
5. Registrar (log) no Serial o que foi executado.

> ⚠️ **Precaução de latência:** **evite `delay()` longos** no `loop()`. Um `delay(2000)` "congela" o ESP32 e ele deixa de responder à parada de emergência. Vamos usar **temporização não-bloqueante** (comparar `millis()`), explicada na Seção 8.2.

---

## 8. O coração técnico: como gerar a "frequência percebida"

Este é o ponto central do projeto. Há **duas formas** de fazer o motor pulsar; vamos descrever as duas e qual usar.

### 8.1. Forma A — RTP (Real-Time Playback): liga/desliga por software
- Colocamos o DRV2605 no modo **RTP** (`DRV2605_MODE_REALTIME`).
- Nesse modo, escrevemos um valor de **amplitude** (`drv.setRealtimeValue(x)`, de 0 a 127): valor alto = vibra forte; **0 = parado**.
- Para criar uma **frequência percebida F** (ex.: 30 Hz), o motor precisa completar **F ciclos por segundo** de "liga e desliga".
  - Período de um ciclo = `1000 / F` milissegundos. Ex.: 30 Hz → ~33,3 ms por ciclo.
  - Em cada ciclo: fica **ligado** uma parte do tempo (ex.: metade) e **desligado** a outra (isso chama-se *duty cycle*).
  - Ex.: 30 Hz com 50% ligado → ~16,6 ms ligado + ~16,6 ms desligado, repetindo.
- A **intensidade** é o valor de amplitude usado na parte "ligado" (0–127, limitado pela Seguranca).

> ✅ **É esta a abordagem recomendada para o MVP:** dá controle direto e didático sobre frequência percebida, intensidade e duração, usando funções simples da biblioteca.

> ⚠️ **Precaução — limite físico:** o motor LRA leva alguns milissegundos para "subir" e "descer" a vibração. Frequências percebidas **muito altas** (perto da ressonância, ex.: > ~100 Hz) podem não ser sentidas como pulsos distintos — vira quase vibração contínua. O alvo útil do projeto é a faixa **baixa (~5–60 Hz)**, exatamente onde a percepção de ritmo funciona bem. Documente a faixa testada.

### 8.2. Temporização não-bloqueante (essencial)
Para alternar liga/desliga sem `delay()` (que travaria a parada de emergência):
- Guardar o instante da última troca (`millis()`).
- A cada volta do `loop()`, comparar se já passou o tempo de "ligado" ou de "desligado" e trocar o estado quando passar.
- Assim o motor pulsa **e** o ESP32 continua respondendo a comandos e à parada.

### 8.3. Forma B — Efeitos prontos da biblioteca (atalho)
- O DRV2605 tem uma **biblioteca interna de 123 efeitos** (`drv.selectLibrary(6)` para LRA, `drv.setWaveform(...)`, `drv.go()`).
- Ótimo para padrões "pulso curto", "pulso duplo", "rampa" etc., **mas** não dá controle fino e arbitrário da frequência percebida.
- **Uso:** bom para alguns padrões da tabela háptica futura; **não** é o caminho para "definir uma frequência exata". Podemos oferecer os dois (RTP para frequência; efeitos prontos para padrões nomeados).

---

## 9. Segurança (obrigatório antes de qualquer teste no corpo)

O PDF do projeto trata segurança como etapa **obrigatória**. No código, isso vira regras concretas:

1. **Limite de intensidade:** nunca passar de um teto configurável (ex.: começar baixo, 20–45%). Todo comando passa pelo filtro antes de chegar ao motor.
2. **Limite de duração por pulso/efeito:** nada de vibração contínua acidental. Use *timeout*: passou do tempo máximo, desliga.
3. **Cooldown (pausa):** após uma rajada, impor um intervalo mínimo para evitar fadiga/superaquecimento.
4. **Parada de emergência:** um comando (e, idealmente, um **botão físico**) que **para tudo imediatamente**. Deve funcionar **antes** de qualquer teste.
5. **Rate limit:** ignorar comandos repetidos rápido demais.
6. **Em caso de erro de I2C:** não acionar o motor; registrar o erro no log.

> ⚠️ **Precauções de uso (do documento do projeto):**
> - Começar com **intensidade baixa** e aumentar só após validar conforto.
> - **Validar fora do rosto primeiro** (bancada, depois mão/antebraço). Só depois pensar em rosto/nuca.
> - **Nunca** perto de olhos, pálpebras, lábios ou mucosas.
> - Este protótipo **não é dispositivo médico**; testes em pessoas devem ser voluntários, progressivos e interrompíveis.

---

## 10. Plano de testes (na ordem!)

Seguir esta ordem reduz risco e facilita achar problemas:

1. **I2C scanner:** rodar um sketch que lista endereços I2C. Deve aparecer **`0x5A`**. Se não aparecer → problema de ligação/GND/tensão. (Não prossiga sem isso.)
2. **Boot sem motor:** subir o código, ver as mensagens de inicialização e calibração no Serial, **sem** o motor ainda acionar.
3. **Motor — vibração mínima:** acionar 1 pulso curto, intensidade baixa. Conferir se vibra e se aquece de forma anormal (não deve).
4. **Frequência percebida:** testar alguns valores (ex.: 10, 30, 60 Hz) e descrever a sensação. Anotar a faixa que funciona bem.
5. **Parada de emergência:** acionar vibração longa e disparar a parada — deve cessar **na hora**.
6. **Estresse leve:** sequências de pulsos, observar estabilidade (sem resets, sem travas).

> ⚠️ **Precaução elétrica:** se o ESP32 **reiniciar sozinho** ao vibrar, é sinal de **queda de tensão** (fonte/USB fraca). Use uma fonte/cabo melhor e capacitor de desacoplamento. Isso é comum e esperado — por isso testamos cedo.

---

## 11. Critério de "pronto" do MVP

O código está pronto para esta fase quando:
- [ ] O chip é detectado (`0x5A`) e calibrado no boot, com tratamento de erro.
- [ ] É possível comandar **frequência percebida + intensidade + duração** pela Serial.
- [ ] A vibração usa **envelope de pulsos** (RTP) sem `delay()` bloqueante.
- [ ] Limites de segurança e **parada imediata** funcionam e foram testados.
- [ ] Há logs básicos no Serial.
- [ ] Os 6 testes da Seção 10 passaram e a faixa de frequência útil foi documentada.

---

## 12. Evolução do projeto (próximos passos — fora do MVP)

> Registrado a pedido do time: o MVP é simples de propósito, mas o caminho de crescimento já fica documentado aqui.

1. **Adicionar o multiplexador TCA9548A.**
   - Por quê: vários DRV2605 têm o **mesmo endereço** (`0x5A`) e brigariam no mesmo barramento. O TCA9548A coloca cada driver num **canal** separado.
   - No código: criar o módulo **GerenciadorI2C** que faz `tcaSelect(canal)` **antes** de falar com cada DRV. Fluxo: *selecionar canal → conversar com o DRV daquele canal → executar efeito*.

2. **Múltiplos motores para vibrar (até 8 zonas).**
   - 1 DRV2605 por motor/zona (8 drivers para 8 zonas independentes).
   - Criar o **ZoneMap** (zona física ↔ canal do TCA ↔ limite de segurança próprio por zona).
   - Inicializar cada canal no boot; se um canal não responder, registrar erro e **não** acionar aquela zona.

3. **Controlar a frequência (percebida) por *grupo* de motores.**
   - Estender o **GeradorDeEnvelope** para manter **estado independente por zona/grupo** (cada grupo com sua própria frequência, intensidade e fase).
   - Permitir disparos **simultâneos/sequenciados** entre zonas com baixa latência (filas de comandos + temporizadores, sem `delay()`).
   - Suportar **perfis por usuário** e a **tabela háptica** (evento → zona, padrão, duração, intensidade) salva em JSON/CSV/struct.

4. **Comunicação externa:** USB Serial → depois **BLE** (wearable) e **Wi-Fi/UDP** (integração com engine). Protocolo com **ACK**, validação de limites e *rate limit*.

5. **Integração com jogo:** Unity/Unreal/Godot enviando eventos; antes disso, um **simulador** (script Python/Node) para testar sem o jogo real.

6. **Hardware usável:** sair da protoboard para **placa soldada/PCB**, conectores travados, fixação mecânica que transmita a vibração sem peças soltas, fonte dimensionada para vários motores com GND comum.

---

## 13. Referências

- DRV2605L (Texas Instruments) — datasheet: https://www.ti.com/lit/ds/symlink/drv2605l.pdf
- TCA9548A (Texas Instruments) — datasheet: https://www.ti.com/lit/ds/symlink/tca9548a.pdf
- ESP32-C3 (Espressif) — datasheet e guia: https://documentation.espressif.com/esp32-c3_datasheet_en.html
- Adafruit DRV2605L — breakout e código Arduino: https://learn.adafruit.com/adafruit-drv2605-haptic-controller-breakout/arduino-code
- Adafruit TCA9548A — multiplexador I2C: https://learn.adafruit.com/adafruit-tca9548a-1-to-8-i2c-multiplexer-breakout/arduino-wiring-and-test
- espp — DRV2605 (referência ESP-IDF/C++, enviada pelo time): https://esp-cpp.github.io/espp/haptics/drv2605.html
- Documentação interna: *Projeto Exus — A Frequência da Imersão* (PDF do projeto).

---

*Resumo das decisões que guiaram este plano: framework **Arduino + Adafruit_DRV2605**; "frequência" = **frequência percebida via envelope de pulsos (RTP)**; escopo do MVP = **1 ESP32-C3 + 1 DRV2605 + 1 LRA moeda**, sem multiplexador, com a evolução (multiplexador, múltiplos motores e frequência por grupo) documentada na Seção 12.*
