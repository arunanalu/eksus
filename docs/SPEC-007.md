# SPEC-007 — Disponibilidade, acesso aberto e reconexão Bluetooth LE

> Data da análise: 17/08/2026  
> Escopo original: diagnóstico e plano de solução.  
> Estado em 17/08/2026: **opção 4 (GATT aberto) selecionada pelo usuário e implementada no repositório; validação física ainda pendente.**  
> Plataforma observada: ESP32-C3, Arduino, NimBLE-Arduino, Windows e Bleak.

## 1. Objetivo solicitado

O comportamento desejado possui dois requisitos:

1. Ao receber energia por USB, bateria ou fonte externa, o Exus deve iniciar o BLE automaticamente e permanecer encontrável/conectável sem janela de tempo nem comando Serial.
2. Depois de uma desconexão, reinicialização ou desligamento, o mesmo computador deve conseguir reconectar sem remover/esquecer o dispositivo no Windows.

A ambiguidade original foi resolvida pelo requisito posterior: “livre para uma
nova conexão” significa aceitar **qualquer computador ou telefone BLE**, sem
pareamento do sistema operacional, quando não houver uma conexão ativa.

Isso seleciona o GATT aberto analisado na opção 4. O risco de qualquer dispositivo
próximo enviar comandos é, portanto, uma decisão consciente do projeto.

## 2. Conclusão executiva

O problema não é uma ausência de Bluetooth no ESP32-C3 nem uma dependência técnica do cabo USB para o rádio funcionar. O firmware atual inicia BLE sem esperar uma porta Serial e solicita advertising sem duração, portanto o BLE deveria operar também com alimentação externa.

Entretanto, existem quatro problemas ou riscos concretos:

1. **Bloqueio intencional do primeiro pareamento:** o dispositivo anuncia, mas rejeita um PC desconhecido, exceto quando `ble pair enable` abriu uma janela de 60 segundos. Essa janela sempre começa fechada no boot. Logo, com bateria e sem Serial, um dispositivo ainda não reconhecido aparece no scan, mas não consegue completar a conexão. Este comportamento contradiz diretamente o novo requisito.
2. **Possível divergência do bond:** Windows e ESP32 mantêm cópias independentes das chaves de pareamento. Se uma cópia for apagada, substituída, migrada incorretamente ou não reconhecida, um lado acredita que já existe vínculo e o outro tenta um vínculo novo. O resultado típico é exatamente “só volta depois de esquecer o dispositivo”.
3. **Reconexão frágil e pouco observável:** o firmware depende de reconhecimento do bond já no `onConnect`, inicia segurança manualmente e não tem recuperação ativa caso o advertising não reinicie. O aplicativo também chama `pair()` em toda conexão, não possui callback imediato de desconexão e usa uma faixa muito ampla de versões do Bleak.
4. **Alimentação externa ainda precisa ser isolada como variável:** o BLE só começa depois da descoberta e calibração dos atuadores. Uma fonte inadequada, rampa lenta, ligação no pino errado ou brownout durante calibração/transmissão pode impedir o boot completo ou causar ciclos de reset. USB funcionar e bateria não funcionar é um forte sinal para testar energia, embora o bloqueio de pareamento já explique parte do cenário.

### Direção escolhida e implementada

- advertising conectável por tempo indefinido desde todo boot;
- BLE iniciado antes da descoberta/calibração dos atuadores;
- GATT sem `WRITE_ENC`, PIN, janela de pareamento ou bond obrigatório;
- bonds legados preservados, mas ignorados pela política de acesso;
- um central por vez, mas qualquer computador/telefone pode ser o próximo;
- reinício automático e supervisão do advertising a cada 1 s;
- cliente Windows sem chamada `pair()`, com callback de desconexão;
- parada dos motores e descarte da fila em desconexão/emergência;
- Bleak fixado em 2.1.1 e NimBLE-Arduino fixado/testado em 2.5.1.

O modo aberto resolve a divergência de bonds e atende literalmente a experiência
solicitada. Ele não autentica o operador; por isso a operação deve ocorrer em
ambiente controlado. Uma versão futura de produto pode adicionar autenticação na
aplicação sem voltar a depender do pareamento do Windows.

## 3. Estado real do repositório

### 3.1 Fluxo de boot

O fluxo atual em [`firmware/firmware.ino`](firmware/firmware.ino) é:

1. iniciar `Serial` e aguardar 600 ms;
2. iniciar I2C;
3. descobrir TCA/DRV e calibrar motores;
4. somente então executar `ble_transport_begin()`;
5. processar Serial, BLE e scheduler no `loop()`.

Não existe `while (!Serial)` nem outra espera pelo PC. Portanto, retirar o cabo de dados **não deveria**, por software, impedir o BLE. A diferença USB versus fonte externa deve ser atribuída à política de pareamento, à alimentação/boot ou aos dois.

O início do BLE pode atrasar conforme a quantidade de LRAs. Em [`firmware/DriverHaptico.cpp`](firmware/DriverHaptico.cpp), cada auto-calibração pode esperar até 1,5 segundo. Com vários LRAs, o tempo entre energizar e anunciar pode chegar a vários segundos. Além disso, a calibração gera carga elétrica antes do rádio iniciar.

### 3.2 Advertising atual

Em [`firmware/BleTransport.cpp`](firmware/BleTransport.cpp), o firmware:

- usa nome estável `Exus-XXXXXX`, derivado do eFuse;
- configura anúncio geral, conectável e descobrível;
- coloca o nome no advertising principal e o UUID na scan response;
- chama `start()` sem duração explícita;
- ativa `advertiseOnDisconnect(true)`.

No NimBLE-Arduino, duração `0` significa anunciar para sempre, e `advertiseOnDisconnect(true)` solicita que o servidor volte a anunciar quando o central desconectar. Portanto, **o limite de 60 segundos não é limite do advertising**. Ele é somente uma trava de autorização para um novo bond.

Referências oficiais: [NimBLEAdvertising::start](https://h2zero.github.io/NimBLE-Arduino/class_nim_b_l_e_advertising.html) e [NimBLEServer::advertiseOnDisconnect](https://h2zero.github.io/NimBLE-Arduino/class_nim_b_l_e_server.html).

### 3.3 Política de pareamento atual

As regras implementadas são:

- `BLE_PAIR_WINDOW_MS = 60000`;
- janela fechada no boot (`s_pairWindowUntil = 0`);
- `ble pair enable` só funciona se não houver conexão nem bond;
- uma conexão é aceita se o endereço for reconhecido como bond existente;
- um desconhecido só é aceito se a janela estiver aberta e não houver nenhum bond;
- após autenticação bem-sucedida, a janela fecha;
- o projeto mantém, por política própria, um único PC autorizado.

Assim, o dispositivo é “visível”, mas não “livre para parear”. A mensagem de boot inclusive orienta usar `ble pair enable`. Isso é a explicação direta para o primeiro requisito não ser atendido com bateria.

### 3.4 Política de reconexão atual

Ao conectar, o firmware executa:

```text
isBonded(info.getIdAddress())
    -> se conhecido: startSecurity(handle)
    -> se desconhecido e janela fechada: desconectar
```

Após `onAuthenticationComplete`, só libera comandos se o link estiver criptografado, marcado como bonded e tiver sido admitido na conexão.

Ao desconectar, ele:

- marca o link como inativo;
- bloqueia controle;
- descarta fragmentos/comando pendente;
- solicita `scheduler_stop_all()`;
- depende de `advertiseOnDisconnect(true)` para voltar a anunciar.

O fluxo é conceitualmente correto para “um PC autorizado”, mas é sensível a qualquer divergência entre a identidade apresentada pelo Windows e o registro persistido no ESP32.

### 3.5 Cliente Windows atual

Em [`exus_control/ble_client.py`](exus_control/ble_client.py), cada tentativa faz:

1. `connect()`;
2. `pair()` quando solicitado pelo chamador;
3. ativa indicações/respostas.

O adapter visual sempre usa `pair=True`, inclusive para um dispositivo que já deveria estar pareado. Em Bleak/WinRT, `pair()` normalmente detecta que o Windows já está pareado e não repete o processo; porém esse caminho aumenta a quantidade de estados possíveis e não distingue claramente primeira conexão de reconexão.

Outras lacunas:

- não é passado `disconnected_callback` ao `BleakClient`;
- a UI percebe a queda por consulta de `is_connected` a cada segundo;
- não existe tentativa controlada de reconexão;
- não existe classificação de erros WinRT (`Unreachable`, timeout, autenticação, cache GATT, rádio desligado);
- o executável não mostra as versões de firmware, NimBLE, core ESP32 e Bleak.

## 4. Diagnóstico provável por sintoma

### 4.1 “Só funciona conectado no PC”

#### Causa A - política de pareamento fechada (certeza alta)

Se o teste com USB inclui o comando `ble pair enable`, e o teste por bateria começa sem bond válido, o firmware rejeita o PC por projeto. A bateria não é o problema do rádio; a falta do comando Serial é.

Sinal esperado no log:

```text
[BLE] Link recusado: pareamento local fechado ou bond de outro PC.
```

#### Causa B - fonte/boot/brownout (probabilidade relevante)

Se nem o nome `Exus-XXXXXX` aparece no scan quando alimentado externamente, a investigação deve começar pelo boot e alimentação:

- tensão ou pino de entrada incorreto;
- regulador incapaz de fornecer pico do ESP32 + drivers + motores;
- GND ausente ou ruim;
- rampa lenta da fonte/CHIP_EN;
- queda durante auto-calibração;
- bateria descarregada ou proteção entrando em corte;
- antena encoberta por bateria, cabos, corpo ou partes metálicas.

A Espressif recomenda, para o ESP32-C3 isolado, alimentação de 3,3 V com capacidade de pelo menos 500 mA, capacitor de pelo menos 10 µF na entrada e atenção à rampa/CHIP_EN. Esse valor **não inclui** os motores, DRV2605L e perdas do regulador do Exus; a fonte total precisa ser dimensionada pela carga real. Consulte as [diretrizes oficiais de hardware do ESP32-C3](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32c3/schematic-checklist.html).

#### Causa C - demora antes do BLE (probabilidade média)

O BLE inicia após varredura e calibração. Um scan iniciado imediatamente após ligar pode terminar antes de o dispositivo começar a anunciar. O requisito deve definir um tempo máximo de disponibilidade, por exemplo 3 ou 5 segundos, ou o firmware deve iniciar BLE antes da calibração e expor estado `INITIALIZING`.

### 4.2 “Depois de desconectar, só reconecta se esquecer o pareamento”

#### Causa A - bond divergente Windows x ESP32 (probabilidade alta)

Bond não é uma propriedade única: há chaves no Windows e chaves na NVS do ESP32. Os estados podem ficar assim:

| Windows | ESP32 | Resultado provável |
|---|---|---|
| possui bond A | possui bond A | reconexão criptografada normal |
| possui bond A | sem bond | Windows tenta usar chave antiga; ESP32 não reconhece |
| sem bond | possui bond A | Windows tenta parear; firmware trata como desconhecido/bloqueado |
| possui bond A | possui bond B | autenticação falha e o link cai |
| sem bond | sem bond | primeiro pareamento possível somente se a política permitir |

Motivos comuns para divergência:

- upload com “erase flash”/apagamento de NVS;
- mudança de tabela de partições;
- troca da placa física mantendo o mesmo nome percebido pelo usuário;
- atualização NimBLE 1.x -> 2.x sem migração do armazenamento de bonds;
- firmware antigo que apagava bond após falha de autenticação;
- queda de energia durante gravação de segurança;
- cancelamento/timeout no meio do primeiro pareamento;
- remoção feita somente em um dos lados.

O histórico Git é especialmente relevante: o commit `6286bb0` se chama **“Fix BLE bonded reconnection after reboot”** e alterou exatamente a identificação/recriptografia do bond e a regra que evitava apagar um bond conhecido após falha. Portanto, é obrigatório verificar se a placa física realmente está executando o firmware atual; hoje `device-info` informa apenas `firmware=ble-v1`, sem commit ou build ID.

#### Causa B - versão antiga ou variável do NimBLE (probabilidade alta/média)

O README pede apenas “NimBLE-Arduino 2.x”. Isso não é reproduzível. A máquina examinada possui NimBLE-Arduino 2.5.1, mas outro PC de build pode gerar firmware diferente.

Mudanças oficiais relevantes:

- no NimBLE 2.x, advertising deixou de reiniciar automaticamente; o projeto já usa `advertiseOnDisconnect(true)`, como exige o [guia de migração](https://h2zero.github.io/NimBLE-Arduino/md_1_8x__to2_8x__migration__guide.html);
- NimBLE 2.4.0 corrigiu o novo pareamento depois de `deleteAllBonds()` e adicionou migração de bonds 1.x -> 2.x;
- versões recentes adicionaram versão de runtime/compile-time, útil para diagnóstico.

Essas alterações constam nas [releases oficiais do NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino/releases). A versão deve ser fixada, não apenas limitada à major 2.

#### Causa C - reconhecimento prematuro no `onConnect` (probabilidade média)

O código toma uma decisão de autorização imediatamente em `onConnect` usando `getIdAddress()`. A API diferencia endereço transmitido no ar de endereço de identidade e oferece `onIdentity` quando uma identidade privada é resolvida. A comparação pode funcionar normalmente, mas o desenho mais robusto é iniciar segurança e validar o peer depois que identidade/autenticação estiverem estabelecidas, especialmente com endereços privados resolvíveis. Consulte [`NimBLEConnInfo`](https://h2zero.github.io/NimBLE-Arduino/_nim_b_l_e_conn_info_8h_source.html) e [`NimBLEDevice`](https://h2zero.github.io/NimBLE-Arduino/class_nim_b_l_e_device.html).

#### Causa D - Bleak/WinRT e descoberta GATT (probabilidade média)

O ambiente local possui Bleak 0.22.3, enquanto `requirements.txt` aceita qualquer versão `>=0.22,<3`. O Bleak 2.1.1 corrigiu casos de `Could not get GATT services: Unreachable` no Windows adicionando nova tentativa durante descoberta de serviços. A [release oficial do Bleak](https://github.com/hbldh/bleak/releases) é diretamente pertinente ao sintoma documentado no README.

A documentação atual recomenda passar `pair=True` ao construtor quando o pareamento precisa ocorrer durante a conexão, em vez de conectar e chamar `pair()` sempre depois. Ela também recomenda usar o objeto `BLEDevice` retornado pelo scan, algo que o projeto já faz. Consulte a [API oficial do BleakClient](https://bleak.readthedocs.io/en/latest/api/client.html).

#### Causa E - advertising não reiniciado ou central ainda conectado (probabilidade média/baixa)

O código usa a opção correta para reiniciar advertising, mas não há verificação posterior. Uma desconexão incompleta no Windows, outro scanner/cliente mantendo GATT aberto, exceção durante `stop_notify` ou reset da pilha pode deixar o usuário sem saber se o periférico está anunciando.

Uma rotina de supervisão deve confirmar:

```text
sem conexão + advertising parado -> registrar motivo -> reiniciar advertising
```

Isso é recuperação, não substitui descobrir a causa original.

## 5. “Disponível” precisa ser dividido em estados

Para evitar falsos diagnósticos, disponibilidade BLE deve ser medida em quatro níveis:

1. **Inicializado:** a pilha BLE iniciou sem erro.
2. **Anunciando:** pacotes de advertising estão sendo transmitidos.
3. **Conectável:** o central consegue estabelecer o link GAP.
4. **Autorizado:** o link completa segurança e pode escrever nas características protegidas.

O estado atual pode estar em 1, 2 e 3 e ainda falhar em 4. Para o usuário isso parece “Bluetooth não conecta”, mas a correção é de autenticação/bond, não de advertising.

## 6. Caminhos possíveis de solução

### Opção 1 - bond persistente, um PC principal, primeiro pareamento automático

**É a recomendação para o requisito mais provável.**

Comportamento:

- zero bonds: anunciar e aceitar primeiro pareamento indefinidamente;
- bond existente: anunciar indefinidamente e aceitar reconexão daquele PC;
- outro PC: pode enxergar o Exus, mas não recebe controle;
- desconexão/reboot: manter bond e voltar a anunciar;
- troca intencional de PC: procedimento de reset de bond, futuramente por botão físico ou ação administrativa.

Vantagens:

- remove a dependência do comando Serial no primeiro uso;
- preserva criptografia e reduz acesso não autorizado;
- resolve o cenário “mesmo PC reconecta sem esquecer”;
- é mudança pequena em relação à arquitetura atual.

Limitação:

- não atende “qualquer novo PC pode parear a qualquer momento” sem um mecanismo adicional.

### Opção 2 - múltiplos PCs bonded

Comportamento:

- pairing permanece permitido;
- bonds válidos anteriores podem reconectar;
- apenas uma conexão fica ativa por vez;
- quando a capacidade enche, aplica-se política explícita de recusa ou remoção do bond mais antigo.

O NimBLE-Arduino 2.5.1 usa por padrão até três bonds, embora esse limite seja configurável. Não se deve inferir “três” para qualquer versão/build sem registrar a configuração usada.

Vantagens:

- computadores do grupo podem alternar sem “esquecer” toda vez;
- mantém criptografia e identidade persistente.

Riscos:

- gerenciamento e revogação ficam mais complexos;
- remover automaticamente o bond antigo pode criar exatamente o problema relatado para aquele PC;
- aceitar pairing permanentemente sem autenticação forte permite que terceiros ocupem os slots.

### Opção 3 - sem bond do sistema operacional, autenticação na aplicação

Comportamento:

- o periférico aceita conexão BLE sem bond persistente;
- Exus Control autentica a sessão com segredo/chave provisionada;
- comandos hápticos só são liberados após autenticação do protocolo.

Vantagens:

- elimina estados de bond divergentes entre NVS e Windows;
- reconexão tende a ser mais previsível;
- política de autorização fica sob controle do projeto.

Riscos e custo:

- exige desenhar desafio/resposta, armazenamento de chave e proteção contra replay;
- uma chave embutida igual em todos os executáveis não oferece segurança real;
- não deve ser improvisada com token fixo em texto aberto;
- aumenta significativamente o escopo de implementação e testes.

É a melhor alternativa se “sempre livre” for prioridade absoluta e o bonding do Windows continuar instável, mas exige projeto de segurança próprio.

### Opção 4 - GATT totalmente aberto, sem bond nem autenticação

É a opção tecnicamente mais simples e provavelmente a mais previsível para reconectar, mas **não é recomendada** para o Exus.

Qualquer central próximo que descubra os UUIDs poderia enviar `pulse`, `stream`, `resume` ou outros comandos. Os limites locais reduzem o dano possível, porém não resolvem autorização. Só seria aceitável em bancada isolada e como experimento diagnóstico temporário.

### Opção 5 - pareamento sem bond, criptografia a cada sessão

Evita chave persistente no ESP/Windows, mas pode solicitar confirmação ou repetir o procedimento de pairing em cada conexão. Isso conflita com a experiência “bastou ligar e conectar” e não é a primeira escolha.

## 7. Arquitetura-alvo sugerida

### 7.1 Máquina de estados do firmware

```text
POWER_ON
  -> saídas hápticas paradas
  -> carregar política/bonds e motivo do reset
  -> iniciar BLE
  -> iniciar advertising indefinido
  -> descobrir/calibrar hardware em estado INITIALIZING
  -> READY ou DEGRADED

ADVERTISING
  -> peer conecta
  -> iniciar/resumir segurança
  -> resolver identidade
  -> validar política de peer
  -> AUTHORIZED ou DISCONNECT

AUTHORIZED
  -> aceitar comandos pelo roteador comum
  -> watchdog e limites continuam ativos
  -> desconexão/emergência: parar motores e descartar fila

DISCONNECTED
  -> confirmar scheduler parado
  -> limpar somente estado transitório, nunca bond válido por erro de RF
  -> reiniciar/confirmar advertising
```

### 7.2 Separar readiness BLE de readiness háptica

Para reduzir a demora e isolar falhas:

- iniciar BLE logo após garantir motores parados;
- anunciar enquanto a topologia é descoberta;
- expor `device-info/status` como `INITIALIZING`;
- bloquear `pulse/effect/stream` até a descoberta terminar;
- manter `emergency` e consulta de status disponíveis;
- ao terminar, publicar `READY`, `DEGRADED` ou `NO_ZONES`.

Isso permite que fonte externa e BLE sejam diagnosticados mesmo quando I2C, TCA ou calibração falham.

### 7.3 Política de segurança explícita

A política deve sair de condições espalhadas e virar configuração documentada, por exemplo conceitualmente:

```text
SINGLE_OWNER_AUTO_FIRST_PAIR
MULTI_BOND_ALWAYS_PAIRABLE
APP_AUTH_NO_OS_BOND
DIAGNOSTIC_OPEN (somente build de bancada)
```

Para a opção recomendada, o fluxo de admissão é:

```text
se não há bond:
    admitir primeiro pairing automaticamente
se há bond:
    iniciar segurança e validar identidade após resolução
    admitir somente bond autorizado
jamais apagar bond conhecido apenas por timeout/RF
```

### 7.4 Segurança BLE a manter

O código atual usa `setSecurityAuth(true, false, true)`: bonding + Secure Connections, sem MITM. Como o ESP32 não possui tela/teclado, isso normalmente resulta em “Just Works”: o link é criptografado, mas não há prova forte de que o usuário autorizou fisicamente o peer.

Se o pareamento ficar aberto indefinidamente, considerar uma destas barreiras:

- botão físico para reivindicar/trocar proprietário;
- chave de aplicação provisionada;
- QR code/segredo individual da unidade;
- aceitar novos peers apenas quando ainda não existir proprietário.

O requisito “sem comando” pode ser atendido no primeiro boot sem tornar troca de proprietário irrestrita.

### 7.5 Reconexão do cliente Windows

O cliente deve:

- usar o `BLEDevice` retornado pelo scan;
- registrar callback real de desconexão;
- na primeira conexão, usar o caminho de pairing suportado pela versão fixada do Bleak;
- em reconexão, não forçar novo pareamento se o Windows já estiver paired;
- classificar erro de link, autenticação, descoberta GATT e timeout;
- encerrar notificações/sessão de forma idempotente;
- fazer, no máximo, tentativas limitadas com backoff;
- nunca reenviar automaticamente comandos hápticos;
- após reconectar, consultar `Q` e manter saída real desabilitada.

Reconexão automática da sessão é aceitável; rearmar vibração automaticamente não é.

## 8. Bibliotecas e versionamento

### 8.1 Firmware

Dependências necessárias já existentes:

- Arduino-ESP32 compatível com a placa ESP32-C3;
- NimBLE-Arduino;
- Adafruit DRV2605 Library;
- Adafruit BusIO;
- `Wire`, incluída no core Arduino.

Não é necessária nova biblioteca para atender o BLE básico. O problema principal está na política/estado, não na falta de pacote.

Recomendação de reprodutibilidade:

- fixar NimBLE-Arduino em uma versão testada, inicialmente a 2.5.1 já instalada nesta máquina;
- fixar a versão do Arduino-ESP32;
- registrar essas versões no build e em `device-info`;
- se houver atualização desde NimBLE 1.x, tratar a migração de bonds deliberadamente;
- validar a versão fixada em hardware antes de distribuí-la.

### 8.2 Aplicativo PC

Dependência necessária:

- Bleak/WinRT para BLE no Windows.

O intervalo atual `bleak>=0.22,<3` permite comportamento diferente entre máquinas. Caminho recomendado:

- testar e fixar uma versão moderna com correções WinRT, candidata Bleak 2.1.1;
- atualizar os testes fake para a API dessa versão;
- reconstruir o pacote PyInstaller inteiro;
- validar em Windows 10 e Windows 11 usados pelo grupo;
- incluir versão do Bleak no log da sessão.

Não se deve atualizar silenciosamente para Bleak 3.x: o requisito atual explicitamente o exclui e uma mudança de major exige nova validação.

### 8.3 Migração para ESP-IDF

Não é necessária para resolver este problema. ESP-IDF daria controle e logs mais baixos da pilha, porém elevaria custo e risco de regressão no firmware háptico. Arduino + NimBLE-Arduino é suficiente, desde que versões, estados e testes sejam disciplinados.

## 9. Observabilidade necessária antes da correção final

Hoje não é possível diferenciar com segurança firmware antigo, bond inválido, brownout e advertising parado só pela UI. Adicionar posteriormente:

- versão semântica e hash do firmware;
- versão NimBLE e Arduino-ESP32;
- motivo do reset (`power-on`, `brownout`, watchdog etc.);
- contador de boots;
- sucesso/erro de `NimBLEDevice::init`;
- momento e resultado de `advertising->start`;
- estado `INITIALIZING/READY/DEGRADED`;
- quantidade e identidades mascaradas dos bonds;
- evento de identidade resolvida;
- resultado/código da autenticação;
- motivo textual da desconexão, não apenas número;
- número de reinícios do advertising;
- RSSI visto pelo PC;
- versão do Bleak e Windows no log do Control.

Dados sensíveis e chaves não devem ser registrados.

## 10. Plano de diagnóstico em bancada

### Fase A - congelar o baseline

1. Registrar commit do repositório, binário, NimBLE, Arduino-ESP32, Bleak e Windows.
2. Confirmar se a placa está realmente com esse binário; `firmware=ble-v1` atual não basta.
3. Limpar bonds dos dois lados uma única vez para criar baseline conhecido.
4. Salvar logs Serial completos do boot e primeiro pairing.

### Fase B - separar alimentação de política BLE

Executar quatro combinações:

| Alimentação | Atuadores | O que observar |
|---|---|---|
| USB | desconectados/standby | boot e advertising puro |
| externa | desconectados/standby | boot e advertising puro |
| USB | montagem completa | efeito da calibração/carga |
| externa | montagem completa | brownout, demora e RF reais |

Em cada caso:

- medir 5 V/VIN e 3,3 V durante boot, calibração e transmissão BLE;
- registrar motivo de reset;
- medir tempo `power-on -> primeiro advertising`;
- executar scan por pelo menos 20 segundos;
- não alimentar simultaneamente por USB e bateria sem circuito apropriado.

Se precisar de log com fonte externa, usar método de depuração que não injete alimentação de volta no circuito.

### Fase C - matriz de bonds

Testar deliberadamente:

1. ambos sem bond;
2. ambos com o mesmo bond;
3. Windows com bond e ESP32 sem bond;
4. ESP32 com bond e Windows sem bond;
5. reinício somente do ESP32;
6. reinício somente do Windows;
7. atualização do firmware preservando NVS;
8. atualização apagando NVS;
9. desligamento durante pairing;
10. tentativa de segundo PC.

Cada caso precisa de resultado esperado e recuperação definida. “Esquecer até funcionar” não é critério de aceite.

### Fase D - ciclos de reconexão

Automatizar ou repetir ao menos 50 ciclos:

```text
scan -> connect -> Q -> disconnect -> confirmar advertising -> reconnect
```

Variar:

- botão Desconectar do Exus Control;
- fechar o aplicativo;
- desligar/ligar Bluetooth do Windows;
- sair e voltar ao alcance;
- desligar/ligar ESP32;
- reiniciar Windows;
- conexão por bateria;
- distância e orientação da antena.

Nenhum ciclo deve exigir esquecer o dispositivo.

### Fase E - segurança funcional

Em toda desconexão:

- motores param imediatamente;
- fila BLE é descartada;
- stream expira;
- advertising retorna;
- reconexão não restaura `hardware_output_enabled`;
- o primeiro comando após reconectar é `Q/status`, nunca replay de pulso.

## 11. Critérios de aceite propostos

### Disponibilidade

- [ ] Todo cold boot por USB ou fonte externa inicia advertising automaticamente.
- [ ] Não existe comando obrigatório para o primeiro uso normal.
- [ ] Advertising permanece ativo indefinidamente quando não há conexão.
- [ ] Tempo máximo power-on -> conectável é definido e medido.
- [ ] Falha de I2C/calibração não oculta o diagnóstico BLE.

### Pareamento e reconexão

- [ ] Primeiro pareamento funciona sem Serial.
- [ ] Mesmo PC reconecta após desconexão, reboot do ESP32 e ciclo de bateria.
- [ ] Nenhum desses fluxos exige “esquecer dispositivo”.
- [ ] 50 ciclos consecutivos passam sem bond divergente.
- [ ] Segundo PC segue política explícita e testada.
- [ ] Atualização de firmware define se preserva ou migra bonds.

### Energia e rádio

- [ ] Fonte externa mantém as tensões dentro das especificações sob pico.
- [ ] Não há brownout/reset durante boot, calibração, conexão ou vibração autorizada.
- [ ] Antena mantém alcance no gabinete e posição final da bateria.
- [ ] RSSI e taxa de sucesso são registrados na distância da demonstração.

### Segurança

- [ ] Desconexão e watchdog param todos os atuadores.
- [ ] Reconexão nunca rearma saída física.
- [ ] Peer não autorizado não envia comandos.
- [ ] Bond válido não é apagado automaticamente por timeout/RF.
- [ ] Logs não expõem chaves.

## 12. Ordem sugerida para uma futura implementação

1. Fixar versões e adicionar identificação de build/telemetria.
2. Reproduzir o defeito atual com logs e matriz de bonds.
3. Corrigir a política de primeiro pareamento para não depender da Serial.
4. Reestruturar autorização para ocorrer após resolução/autenticação.
5. Garantir e supervisionar reinício do advertising.
6. Ajustar o cliente Bleak para primeira conexão versus reconexão.
7. Separar início do BLE da inicialização demorada dos atuadores.
8. Validar fonte externa e reset reason.
9. Executar matriz completa, 50 ciclos e teste de distância.
10. Só depois atualizar README/SPEC e gerar novo Exus-Control.exe.

## 13. Arquivos impactados em uma futura correção

Prováveis arquivos, sem alterações feitas nesta análise:

- `firmware/Config.h`: política de pairing, timeouts e opções de diagnóstico;
- `firmware/BleTransport.h/.cpp`: estados, segurança, bonds, advertising e logs;
- `firmware/firmware.ino`: ordem de boot e reset reason;
- `firmware/BleProtocol.h`: versão/capabilities, se o contrato mudar;
- `exus_control/ble_client.py`: conexão, pairing, callback e erros;
- `exus_control/transports/ble.py`: reconexão segura e capabilities;
- `exus_control/app.py`: estados/erros e UX;
- `exus_control/requirements.txt`: versão fixada do Bleak;
- testes BLE no PC e novos testes/roteiros de hardware;
- `README.md`, `docs/SPEC-003.md` e `docs/SPEC-003.5.md`: nova política operacional.

## 14. Decisões que o time precisa fechar

Antes de codificar:

1. “Nova conexão” significa o mesmo PC, vários PCs autorizados ou qualquer PC?
2. Quantos computadores precisam manter bond ao mesmo tempo?
3. O risco de qualquer pessoa próxima tentar parear é aceitável na demonstração?
4. Existe ou existirá botão físico para redefinir proprietário?
5. Qual é a placa ESP32-C3 exata e por qual pino/regulador a fonte externa entra?
6. Qual tempo máximo entre ligar e ficar conectável é aceitável?
7. Bonds devem sobreviver a atualização de firmware? Como será feita a migração?

Sem essas decisões, é possível corrigir a reconexão do mesmo PC, mas não definir corretamente a política de novos PCs.

## 15. Veredito

O requisito é tecnicamente viável com o hardware atual e não exige módulo Bluetooth adicional nem migração de framework.

O primeiro defeito é principalmente de política: o firmware anuncia indefinidamente, mas o primeiro pareamento é bloqueado por uma janela Serial de 60 segundos. O segundo é compatível com divergência de bond e com a falta de versionamento/telemetria, possivelmente incluindo firmware físico anterior ao commit que já tentou corrigir reconexão.

A solução robusta não deve ser apenas “aumentar a janela para infinito”. Ela deve alinhar:

- política clara de proprietário/novos peers;
- persistência e validação correta de bonds;
- identidade resolvida antes da autorização final;
- advertising autorrecuperável;
- cliente Windows com versão e fluxo de pairing controlados;
- boot BLE independente da calibração demorada;
- alimentação externa validada sob carga;
- testes repetíveis de reconexão e estados divergentes.

Somente esse conjunto garante “ligou, apareceu, conectou e reconectou” sem transformar o Exus em um periférico háptico aberto a qualquer pessoa próxima.

## 16. Resultado da implementação

### Firmware

- `BleTransport` autoriza toda conexão GATT sem exigir criptografia ou bond.
- `command` e `emergency` perderam `WRITE_ENC`.
- Bonds legados locais são preservados para uma migração suave, mas não
  autorizam nem bloqueiam acesso; nenhum novo bond é exigido.
- `advertiseOnDisconnect(true)` foi mantido e recebeu um supervisor independente
  que reinicia o anúncio ilimitado quando necessário.
- O BLE inicia antes da calibração dos atuadores.
- Desconexão continua parando o scheduler e descartando comandos pendentes.
- `ble status` informa `access=OPEN`, advertising, bonds, reinícios e último erro.
- O motivo de reset passou a ser registrado na Serial.

### Exus Control

- O cliente não chama mais `BleakClient.pair()`.
- A desconexão inesperada é sinalizada por callback e invalida respostas pendentes.
- Não há replay automático de comandos após reconexão.
- A consulta inicial `Q` tolera a calibração ainda em andamento.
- Bleak foi fixado em `2.1.1` para builds reproduzíveis.

### Verificações executadas

- suíte Python: 25 testes aprovados;
- `git diff --check`: sem erros de whitespace;
- bibliotecas Arduino presentes: NimBLE-Arduino 2.5.1, Adafruit DRV2605 1.2.4 e
  Adafruit BusIO 1.17.4;
- firmware compilado para `esp32:esp32:esp32c3`: 635.575 bytes de flash (48%) e
  26.140 bytes de RAM global (7%);
- pacote `Exus-Control-Windows.zip` reconstruído com Bleak 2.1.1 e inicialização
  do executável validada;
- teste físico de rádio, fonte externa e 50 ciclos permanece obrigatório porque
  nenhum ESP32 foi conectado ao ambiente durante esta implementação.

As seções 3 a 15 preservam o diagnóstico e as alternativas anteriores para
rastreabilidade. Quando mencionam a política antiga de bond/janela, descrevem o
baseline que foi substituído, não o comportamento atual.

## 17. Fontes consultadas

### Internas

- [`README.md`](README.md)
- [`docs/SPEC-003.md`](docs/SPEC-003.md)
- [`docs/SPEC-003.5.md`](docs/SPEC-003.5.md)
- [`docs/projeto_eksus_documentacao.pdf`](docs/projeto_eksus_documentacao.pdf)
- [`firmware/BleTransport.cpp`](firmware/BleTransport.cpp)
- [`firmware/Config.h`](firmware/Config.h)
- [`exus_control/ble_client.py`](exus_control/ble_client.py)

### Oficiais externas

- [NimBLE-Arduino - documentação](https://h2zero.github.io/NimBLE-Arduino/)
- [NimBLE-Arduino - migração 1.x para 2.x](https://h2zero.github.io/NimBLE-Arduino/md_1_8x__to2_8x__migration__guide.html)
- [NimBLE-Arduino - releases](https://github.com/h2zero/NimBLE-Arduino/releases)
- [BleakClient - documentação](https://bleak.readthedocs.io/en/latest/api/client.html)
- [Bleak - releases](https://github.com/hbldh/bleak/releases)
- [Espressif - ESP32-C3 Hardware Design Guidelines](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32c3/)
