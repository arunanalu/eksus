# Exus Boat Demo

Demo 2D leve de navegação a vela para testar vento direcional e impacto de gelo no protótipo Exus.

## Executar

1. Abra esta pasta no Godot 4.5.x e execute a cena principal.
2. Inicie o Exus Control para receber UDP local em `127.0.0.1:4242`.
3. O jogo inicia em simulação. Para testar hardware, habilite a saída no Control e pressione `F6` no jogo; o HUD passa a mostrar `Saída: REAL LIGADA`.

Controles: `A/D` ou setas para leme; `W/S` para vela; `F6` alterna a solicitação de saída real; `F7` a desliga; `F8` mostra painel.

## Calibração háptica

Edite `config/haptics/boat-demo.v1.json` e copie a alteração para `exus_control/profiles/boat-demo.v1.json` (o teste automatizado exige que sejam idênticos). O arquivo do Control é o efetivamente carregado. Ajuste `gain`, `min_pct` e `max_pct` de uma zona por vez e mantenha os limites do firmware.

## Gerar o .exe

Com Godot e os export templates Windows instalados:

```powershell
.\tools\build_windows.ps1 -Godot 'C:\Caminho\godot.exe'
```

O ZIP sai em `build/Exus-Boat-Demo-Windows.zip`. Não separe o `.exe` do `.pck`.
